/*
 * This file is part of JS80P, a synthesizer plugin.
 * Copyright (C) 2026  Attila M. Magyar
 *
 * JS80P is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * JS80P is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * The JUCE editor, hosted straight off the original wrapper's IPlugView.
 *
 * The whole point of this file is that the plugin keeps the hand-rolled VST3
 * implementation -- its class IDs, ParamID scheme, state format, MIDI mapping
 * and program list, hence its compatibility with projects that were saved with
 * the original JS80P -- while rendering the JUCE GUI. So JUCE is linked for
 * juce_gui_basics only: no juce_audio_processors, no juce_add_plugin, and none
 * of JUCE's own VST3 wrapper (see CMakeLists.txt).
 *
 * That means the standard glue JUCE's wrapper would have provided has to be
 * provided here instead, which is what everything below is: the library
 * initialiser, and on Linux the bridge from JUCE's message loop onto the host's
 * run loop. Both mirror JUCE's own VST3 wrapper
 * (juce_audio_plugin_client_VST3.cpp), which is the reference for running a JUCE
 * GUI inside a foreign VST3.
 *
 * Compiled instead of plugin-xcb.cpp / plugin-win32.cpp / plugin-macos.cpp, and
 * unlike those it is a translation unit of its own rather than one #include'd
 * into plugin.cpp, so that plugin.cpp never sees a JUCE header.
 */

/* plugin.hpp is not self-contained: it names Vst::IParamValueQueue without
 * declaring it, and has so far got away with it because plugin.cpp pulls the SDK
 * in first. This is the second translation unit to include it. */
#include <vst3sdk/pluginterfaces/vst/ivstparameterchanges.h>

#include "plugin/vst3/plugin.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#if SMTG_OS_LINUX
/* Not exported through juce_events.h: JUCE's own VST3 wrapper reaches for it by
 * path in exactly the same way. */
#include <juce_events/native/juce_EventLoopInternal_linux.h>
#endif

#include "ui/editor_root.hpp"
#include "ui/glyph_sheet.hpp"

#include "plugin/vst3/juce_gui.hpp"


using namespace Steinberg;


namespace JS80P
{

namespace
{

/**
 * \brief Keeps JUCE up and the cross-editor caches warm for as long as a plugin
 *        instance exists. Held by the Controller, which owns the editor.
 */
class JuceRuntime
{
    public:
        /* Declaration order is load-bearing: members die in reverse, so the glyph
         * cache (which holds juce::Image objects, possibly Direct2D-backed) is
         * released while JUCE's graphics runtime is still up, and the runtime
         * goes last. Letting the images outlive it -- or fall to atexit at
         * DLL-unload -- hangs the host. See GlyphSheet. */
        juce::ScopedJuceInitialiser_GUI initialiser;
        juce::SharedResourcePointer<GlyphSheet> glyph_sheet;
};


#if SMTG_OS_LINUX

/**
 * \brief Runs JUCE's message loop off the host's run loop.
 *
 * On Linux a VST3 plugin may not spin its own event loop: the host owns the
 * thread and lends it out through Linux::IRunLoop. JUCE expects the opposite, so
 * the two are bridged by handing JUCE's file descriptors to the host and
 * dispatching each readiness callback back into JUCE. Without this, nothing --
 * no repaint, no mouse event, no juce::Timer -- is ever delivered.
 *
 * JUCE's set of descriptors is not fixed (opening the X11 connection adds one),
 * hence the Listener: whenever it changes, all of them are re-registered, since
 * IRunLoop can only drop a handler's descriptors wholesale.
 */
class JuceEventHandler : public Linux::IEventHandler,
                         public FObject,
                         private juce::LinuxEventLoopInternal::Listener
{
    public:
        explicit JuceEventHandler(Linux::IRunLoop* const run_loop)
            : run_loop(run_loop)
        {
            /* We are called on the thread the host runs its loop on; from here on
             * that is the thread JUCE must regard as its message thread. */
            juce::MessageManager::getInstance()->setCurrentThreadAsMessageThread();

            juce::LinuxEventLoopInternal::registerLinuxEventLoopListener(*this);

            attach();
        }

        virtual ~JuceEventHandler()
        {
            juce::LinuxEventLoopInternal::deregisterLinuxEventLoopListener(*this);

            detach();
        }

        void PLUGIN_API onFDIsSet(Linux::FileDescriptor fd) override
        {
            juce::LinuxEventLoopInternal::invokeEventLoopCallbackForFd(fd);
        }

        DELEGATE_REFCOUNT(Steinberg::FObject);

        DEFINE_INTERFACES
            DEF_INTERFACE(Steinberg::Linux::IEventHandler);
        END_DEFINE_INTERFACES(Steinberg::FObject)

    private:
        void fdCallbacksChanged() override
        {
            detach();
            attach();
        }

        void attach()
        {
            if (run_loop == NULL) {
                return;
            }

            for (int const fd : juce::LinuxEventLoopInternal::getRegisteredFds()) {
                run_loop->registerEventHandler(this, fd);
            }
        }

        void detach()
        {
            if (run_loop == NULL) {
                return;
            }

            run_loop->unregisterEventHandler(this);
        }

        Linux::IRunLoop* const run_loop;
};

#endif

}


void* JuceGui::create_runtime()
{
    return (void*)new JuceRuntime();
}


void JuceGui::destroy_runtime(void* const runtime)
{
    delete (JuceRuntime*)runtime;
}


void JuceGui::preferred_size(int& width, int& height)
{
    EditorRoot::preferred_size(width, height);
}


Vst3Plugin::GUI::GUI(Synth& synth, ViewRect& gui_size)
    : CPluginView(&gui_size),
    synth(synth),
    gui_size(gui_size),
    editor(NULL),
    content_scale(1.0f),
    has_host_scale(false)
#if SMTG_OS_LINUX
    , run_loop(NULL),
    event_handler(NULL),
    timer_handler(NULL)
#endif
{
}


Vst3Plugin::GUI::~GUI()
{
    removedFromParent();
}


tresult PLUGIN_API Vst3Plugin::GUI::isPlatformTypeSupported(FIDString type)
{
    if (FIDStringsEqual(type, JS80P_VST3_GUI_PLATFORM)) {
        return kResultTrue;
    }

    return kResultFalse;
}


tresult PLUGIN_API Vst3Plugin::GUI::canResize()
{
    return kResultTrue;
}


tresult PLUGIN_API Vst3Plugin::GUI::checkSizeConstraint(ViewRect* rect)
{
    if (editor == NULL) {
        return kResultTrue;
    }

    EditorRoot* const root = (EditorRoot*)editor;

    /* The host proposes a rectangle in its own (physical) pixels; the editor's
     * aspect/limit rule is expressed in JUCE's logical pixels, so drop into
     * logical space to apply it and lift the answer back out. */
    int width = root->to_logical((int)rect->getWidth());
    int height = root->to_logical((int)rect->getHeight());

    root->apply_size_constraints(width, height);

    rect->right = rect->left + (int32)root->to_physical(width);
    rect->bottom = rect->top + (int32)root->to_physical(height);

    return kResultTrue;
}


tresult PLUGIN_API Vst3Plugin::GUI::onSize(ViewRect* newSize)
{
    tresult result = CPluginView::onSize(newSize);

    if (editor == NULL || result != kResultTrue) {
        return result;
    }

    gui_size = *newSize;

    EditorRoot* const root = (EditorRoot*)editor;

    /* newSize is in the host's physical pixels; the component is laid out in
     * logical ones and its peer scales those up by the display scale it detected
     * (see EditorRoot::content_scale()). setSize() carries the new bounds down to
     * that peer -- the window the host handed us -- as well. */
    root->setSize(
        root->to_logical((int)newSize->getWidth()),
        root->to_logical((int)newSize->getHeight())
    );

    return result;
}


void Vst3Plugin::GUI::attachedToParent()
{
    show_if_needed();
}


void Vst3Plugin::GUI::handle_resize_request(
        int const new_width,
        int const new_height
) {
    if (editor == NULL || plugFrame == NULL) {
        return;
    }

    EditorRoot* const root = (EditorRoot*)editor;

    int width = new_width;
    int height = new_height;

    root->apply_size_constraints(width, height);

    /* new_width/height come from the editor in logical pixels; the host's rect
     * is in physical ones. */
    int const phys_width = root->to_physical(width);
    int const phys_height = root->to_physical(height);

    ViewRect current_rect;

    getSize(&current_rect);

    if (current_rect.getWidth() == phys_width
            && current_rect.getHeight() == phys_height) {
        return;
    }

    current_rect.right = current_rect.left + (int32)phys_width;
    current_rect.bottom = current_rect.top + (int32)phys_height;

    /* Comes back to us as onSize(), which is what actually resizes the editor. */
    plugFrame->resizeView(this, &current_rect);
}


void Vst3Plugin::GUI::show_if_needed()
{
    if (!isAttached()) {
        return;
    }

    initialize();
}


void Vst3Plugin::GUI::initialize()
{
#if SMTG_OS_LINUX
    /* Before anything JUCE: constructing the editor starts timers and opens the
     * X11 connection, which are only ever serviced through this bridge. */
    Linux::IRunLoop* loop = NULL;

    if (plugFrame != NULL) {
        plugFrame->queryInterface(Linux::IRunLoop::iid, (void**)&loop);
    }

    if (loop != NULL) {
        run_loop = (void*)loop;
        event_handler = (void*)new JuceEventHandler(loop);
    }
#endif

    EditorRoot* const root = new EditorRoot(synth, kVstVersionString);

    editor = (void*)root;

    root->on_resize_request = [this](int const width, int const height)
    {
        handle_resize_request(width, height);
    };

    root->setOpaque(true);
    root->addToDesktop(0, (void*)systemWindow);
    root->setVisible(true);

    /* The peer exists now, which means the editor can see the display scale --
     * and if the host happened to negotiate one before the window existed, it can
     * be pushed onto the peer now. Either way this settles before anything is
     * measured or painted below. */
    if (has_host_scale) {
        root->set_host_scale((double)content_scale);
    }

    /* The host sized the window from whatever getSize() reported before the
     * editor existed; tell it what the editor actually wants now. The editor
     * thinks in logical pixels, gui_size is what the host sees, i.e. physical --
     * already at the right scale for this display, without having had to wait for
     * setContentScaleFactor() (which some hosts never send). */
    int width = 0;
    int height = 0;

    EditorRoot::preferred_size(width, height);
    root->setSize(width, height);

    int phys_width = 0;
    int phys_height = 0;

    root->preferred_physical_size(phys_width, phys_height);

    gui_size.right = gui_size.left + (int32)phys_width;
    gui_size.bottom = gui_size.top + (int32)phys_height;
    setRect(gui_size);

    if (plugFrame != NULL) {
        plugFrame->resizeView(this, &gui_size);
    }
}


tresult PLUGIN_API Vst3Plugin::GUI::setContentScaleFactor(
        IPlugViewContentScaleSupport::ScaleFactor factor
) {
#if SMTG_OS_MACOS
    /* macOS scales the backing store itself; JUCE's own VST3 wrapper likewise
     * declines this on the Mac. */
    (void)factor;

    return kResultFalse;
#else
    if (factor <= 0.0f) {
        return kResultFalse;
    }

    /* Only ever an override of what the editor already detected for itself: the
     * window is sized correctly for the display from the moment it opens, and
     * this corrects it afterwards if the host disagrees. Remembered even when the
     * editor doesn't exist yet, so initialize() can apply it to the new peer. */
    content_scale = (float)factor;
    has_host_scale = true;

    if (editor == NULL) {
        /* No editor to ask for a size, but the interface contract still expects
         * getSize() to reflect the new scale. */
        int logical_width = 0;
        int logical_height = 0;

        EditorRoot::preferred_size(logical_width, logical_height);

        report_size(
            juce::roundToInt((double)logical_width * (double)factor),
            juce::roundToInt((double)logical_height * (double)factor)
        );

        return kResultTrue;
    }

    EditorRoot* const root = (EditorRoot*)editor;

    if (!root->set_host_scale((double)factor)) {
        return kResultTrue;
    }

    /* Re-report our size at the new scale (in physical pixels) so the host
     * resizes the window to match; it answers with onSize(). */
    report_size(
        root->to_physical(root->getWidth()), root->to_physical(root->getHeight())
    );

    return kResultTrue;
#endif
}


void Vst3Plugin::GUI::report_size(int const phys_width, int const phys_height)
{
    gui_size.right = gui_size.left + (int32)phys_width;
    gui_size.bottom = gui_size.top + (int32)phys_height;
    setRect(gui_size);

    if (plugFrame != NULL) {
        plugFrame->resizeView(this, &gui_size);
    }
}


void Vst3Plugin::GUI::removedFromParent()
{
    if (editor != NULL) {
        EditorRoot* const root = (EditorRoot*)editor;

        root->removeFromDesktop();

        delete root;

        editor = NULL;
    }

#if SMTG_OS_LINUX
    /* Unregisters itself from the run loop on the way out. */
    delete (JuceEventHandler*)event_handler;

    event_handler = NULL;
    run_loop = NULL;
#endif
}

}

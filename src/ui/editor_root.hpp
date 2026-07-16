#ifndef JS80P__UI__EDITOR_ROOT_HPP
#define JS80P__UI__EDITOR_ROOT_HPP

#include <functional>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "js80p.hpp"
#include "synth.hpp"

#include "gui/gui.hpp"

#include "ui/new_gui.hpp"


namespace JS80P
{

/**
 * \brief The whole editor surface, bound to a \c Synth and nothing else: the new
 *        simplified GUI (NewGui) on top, and the original toolkit-agnostic
 *        \c JS80P::GUI (JUCE Widget backend) behind it, shown under the MATRIX
 *        tab. Drives \c GUI::idle() from a \c juce::Timer.
 *
 *        Deliberately knows no \c juce::AudioProcessor, so it can be hosted
 *        either by a \c juce::AudioProcessorEditor (the Standalone build, see
 *        plugin/juce/PluginEditor.hpp) or straight off the hand-rolled VST3
 *        wrapper's \c IPlugView (the compatibility build, see
 *        plugin/vst3/plugin-juce.cpp). The host owns the window, so instead of
 *        resizing itself this component asks through \c on_resize_request.
 */
class EditorRoot : public juce::Component,
                   public juce::Timer,
                   public JS80P::GUI::EventHandler
{
    public:
        /** Scale range the editor may be resized through; 1.0 is base_*(). */
        static constexpr double MIN_SCALE = 0.5;
        static constexpr double MAX_SCALE = 2.0;

        /* Base (1.0x) resolution the new GUI is laid out at; it's drawn through a
         * uniform scale transform, so resizing the aspect-locked window zooms
         * rather than reflows. 0.527 matches the original default window size. */
        static int base_width();
        static int base_height();

        /** The size an editor should open at: the size last chosen this session
         *  (shared across instances), or the 1.0x default if unset. */
        static void preferred_size(int& width, int& height);

        /**
         * \brief Snap a host-proposed size to the locked aspect ratio and the
         *        MIN_SCALE .. MAX_SCALE range. The host hands us the rectangle
         *        the user dragged to but not which edge moved, so the rectangle
         *        is projected onto the locked ratio: both axes contribute, and
         *        the result is a continuous function of the input (see the
         *        implementation -- picking one axis made it discontinuous).
         */
        void apply_size_constraints(int& width, int& height) const;

        /**
         * \param version   SDK / plugin version string, shown on the About screen.
         */
        EditorRoot(Synth& synth, char const* const version);

        ~EditorRoot() override;

        /** Ask the host to resize the window to the given size. Set by the host
         *  wrapper; when unset, this component resizes itself. */
        std::function<void(int const, int const)> on_resize_request;

        void resized() override;
        void timerCallback() override;

        void handle_resize_request(
            int const new_width, int const new_height
        ) override;

    private:
        /* Window size last chosen by the user this session, shared across all
         * instances; editors opened afterwards start here. Not persisted; 0 means
         * unset (fall back to the 1.0x default). Message-thread only. */
        static int shared_width;
        static int shared_height;

        /* Every live editor, visible or not (some hosts keep editors built but
         * hidden after close, ready to re-show). Lets sync_all_to_shared() push a
         * rescale onto all of them. Expected to be touched on the message thread
         * only, but kept in a thread-safe list (register/unregister/iterate are
         * guarded by its built-in CriticalSection) so a stray access from another
         * thread can't corrupt it. */
        static juce::Array<EditorRoot*, juce::CriticalSection> instances;

        /* Re-entrancy guard while sync_all_to_shared() resizes the others (each
         * request re-enters resized()). */
        static bool syncing;

        /* Resize every other live editor to the shared size; hidden ones get
         * corrected in place so they show right when next revealed. */
        static void sync_all_to_shared(EditorRoot const* const source);

        /* Route a size change through the host when it owns the window. */
        void request_resize(int const width, int const height);

        /* Show / hide + size the embedded legacy GUI when the MATRIX tab of the
         * new GUI is entered / left. */
        void set_matrix(bool const active);
        void layout_matrix();

        /* Bottom-right resize grip, and the aspect/limit rule it drags against.
         *
         * It lives here rather than in the host wrappers because only one of
         * them could ever have provided it: the Standalone's
         * AudioProcessorEditor::setResizable(true, true) makes one, but the VST3
         * build hosts this component straight off IPlugView, which has no
         * equivalent -- so the plugin, i.e. the build that ships, had no grip at
         * all. Owning it here gives both builds the same one, dragging against
         * the same constraint.
         *
         * Declared before the children it sits over, since it is raised to the
         * front on every resized(). */
        juce::ComponentBoundsConstrainer constrainer;
        std::unique_ptr<juce::ResizableCornerComponent> resizer;

        /* The legacy GUI is built on first use, not in the constructor: it
         * decodes a dozen-plus PNGs (JucePlatform::load_image does a full
         * ImageFileFormat::loadFrom per call, uncached), which measured ~110-155ms
         * -- next to NewGui's ~6ms -- and none of it is needed until someone
         * opens the MATRIX tab. NULL until then; every user has to cope. */
        JS80P::GUI* gui;
        std::unique_ptr<NewGui> new_gui;

        /* Build the legacy GUI + reparent it into matrix_host, if not done yet.
         * Message thread only. */
        void ensure_legacy_gui();

        /* Ctor args kept for ensure_legacy_gui(). The version string is a literal
         * owned by the caller (kVstVersionString / JucePlugin_VersionString). */
        Synth& synth;
        char const* const version;

        /* Container that hosts the (reparented) legacy GUI root under the MATRIX
         * tab, sized to fit the body area below the header (background-size:
         * contain). */
        juce::Component matrix_host;
        bool matrix_active;
        bool in_resize;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditorRoot)
};

}

#endif

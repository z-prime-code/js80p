#ifndef JS80P__PLUGIN__JUCE__PLUGIN_EDITOR_HPP
#define JS80P__PLUGIN__JUCE__PLUGIN_EDITOR_HPP

#include <algorithm>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "gui/gui.hpp"

#include "plugin/juce/PluginProcessor.hpp"

#include "ui/new_gui.hpp"


namespace JS80P
{

/**
 * \brief Hosts both editors in one window: the new simplified GUI (NewGui) on
 *        top, and the original toolkit-agnostic JS80P::GUI (JUCE Widget backend)
 *        behind it, switched by an always-on-top toggle. Drives GUI::idle() from
 *        a juce::Timer and relays resize both ways.
 */
class JS80PEditor : public juce::AudioProcessorEditor,
                    public juce::Timer,
                    public JS80P::GUI::EventHandler
{
    public:
        explicit JS80PEditor(JS80PProcessor& processor);
        ~JS80PEditor() override;

        void resized() override;
        void timerCallback() override;

        void handle_resize_request(
            int const new_width, int const new_height
        ) override;

    private:
        /* Base (1.0x) resolution the new GUI is laid out at; it's drawn through a
         * uniform scale transform, so resizing the aspect-locked window zooms
         * rather than reflows. 0.527 matches the previous default window size. */
        static int base_width()  { return (int)(0.527 * (double)JS80P::GUI::WIDTH); }
        static int base_height() { return (int)(0.527 * (double)(JS80P::GUI::HEIGHT - 53)); }

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
        static juce::Array<JS80PEditor*, juce::CriticalSection> instances;

        /* Re-entrancy guard while sync_all_to_shared() resizes the others (each
         * setSize() re-enters resized()). */
        static bool syncing;

        /* Resize every other live editor to the shared size; hidden ones get
         * corrected in place so they show right when next revealed. */
        static void sync_all_to_shared(JS80PEditor const* const source);

        /* A host restoring a saved editor forces its stale size onto us before the
         * window has painted; resized() tells that from a user resize by first
         * paint. See new_gui->has_painted. */

        /* Show / hide + size the embedded legacy GUI when the MATRIX tab of the
         * new GUI is entered / left. */
        void set_matrix(bool const active);
        void layout_matrix();

        JS80PProcessor& processor;
        JS80P::GUI* gui;
        std::unique_ptr<NewGui> new_gui;
        /* Container that hosts the (reparented) legacy GUI root under the MATRIX
         * tab, sized to fit the body area below the header (background-size:
         * contain). */
        juce::Component matrix_host;
        bool matrix_active;
        bool in_resize;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JS80PEditor)
};

}

#endif

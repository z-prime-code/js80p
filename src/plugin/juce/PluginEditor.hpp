#ifndef JS80P__PLUGIN__JUCE__PLUGIN_EDITOR_HPP
#define JS80P__PLUGIN__JUCE__PLUGIN_EDITOR_HPP

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "plugin/juce/PluginProcessor.hpp"

#include "ui/editor_root.hpp"


namespace JS80P
{

/**
 * \brief Standalone / JUCE-hosted wrapper around \c EditorRoot, which is the
 *        actual editor. Contributes only what needs an \c AudioProcessorEditor:
 *        the resizable window and its aspect-locked size constraints.
 *
 *        The VST3 that ships is built from the original hand-rolled wrapper and
 *        hosts \c EditorRoot directly (see plugin/vst3/plugin-juce.cpp); this
 *        path exists for the Standalone build.
 */
class JS80PEditor : public juce::AudioProcessorEditor
{
    public:
        explicit JS80PEditor(JS80PProcessor& processor);

        void resized() override;

    private:
        EditorRoot root;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JS80PEditor)
};

}

#endif

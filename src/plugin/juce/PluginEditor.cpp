#include "plugin/juce/PluginEditor.hpp"


namespace JS80P
{

JS80PEditor::JS80PEditor(JS80PProcessor& processor)
    : juce::AudioProcessorEditor(&processor),
    root(processor.get_synth(), JucePlugin_VersionString)
{
    /* Resizable, but without AudioProcessorEditor's own corner grip: EditorRoot
     * carries one (the VST3 build has no wrapper to supply it), and two would
     * otherwise sit on top of each other here. */
    setResizable(true, false);

    int const bw = EditorRoot::base_width();
    int const bh = EditorRoot::base_height();

    /* The editor is laid out once at a fixed base resolution and drawn through a
     * uniform scale transform; the window is aspect-locked to that ratio, so
     * resizing zooms the whole UI instead of reflowing it. (The VST3 build has
     * the host enforce the same rule through IPlugView::checkSizeConstraint(),
     * which calls EditorRoot::apply_size_constraints().) */
    if (juce::ComponentBoundsConstrainer* const constrainer = getConstrainer()) {
        constrainer->setFixedAspectRatio((double)bw / (double)bh);
        constrainer->setSizeLimits(
            (int)((double)bw * EditorRoot::MIN_SCALE),
            (int)((double)bh * EditorRoot::MIN_SCALE),
            (int)((double)bw * EditorRoot::MAX_SCALE),
            (int)((double)bh * EditorRoot::MAX_SCALE)
        );
    }

    /* EditorRoot doesn't own its window; here the window is us. */
    root.on_resize_request = [this](int const width, int const height)
    {
        setSize(width, height);
    };

    addAndMakeVisible(root);

    int width = 0;
    int height = 0;

    EditorRoot::preferred_size(width, height);
    setSize(width, height);
}


void JS80PEditor::resized()
{
    root.setBounds(getLocalBounds());
}

}

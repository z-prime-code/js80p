#ifndef JS80P__UI__PARAM_LABELS_HPP
#define JS80P__UI__PARAM_LABELS_HPP

#include <juce_gui_basics/juce_gui_basics.h>


namespace JS80P
{

/* Short discrete-option names, mirroring the original GUI's tables so "type"
 * knobs can display a name instead of a raw index. Order matches the engine
 * enums (SimpleBiquadFilter, Distortion). */

inline juce::StringArray const& filter_type_labels()
{
    static juce::StringArray const labels {
        "LP", "HP", "BP", "Notch", "Bell", "LS", "HS"
    };
    return labels;
}


inline juce::StringArray const& distortion_type_labels()
{
    static juce::StringArray const labels {
        "tanh 3x", "tanh 5x", "tanh 10x", "sin x", "x^2", "sqrt x", "x^3",
        "cbrt x", "1+3", "1+5", "1+3+5", "square", "triangle",
        "bit 1", "bit 2", "bit 3", "bit 4", "bit 4.6", "bit 5", "bit 5.6",
        "bit 6", "bit 6.6", "bit 7", "bit 7.6", "bit 8", "bit 8.6", "bit 9",
        "reduce"
    };
    return labels;
}

}

#endif

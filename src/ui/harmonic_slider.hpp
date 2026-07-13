#ifndef JS80P__UI__HARMONIC_SLIDER_HPP
#define JS80P__UI__HARMONIC_SLIDER_HPP

#include <juce_gui_basics/juce_gui_basics.h>

#include "js80p.hpp"
#include "synth.hpp"

#include "ui/control.hpp"
#include "ui/param_bridge.hpp"


namespace JS80P
{

/**
 * \brief One bar of the custom-harmonics editor: a Control subclass that keeps
 *        the full value / modulation / badge core but renders as the bipolar
 *        amplitude bar the harmonics editor has always used (0.5 = zero, filling
 *        up or down from the mid line) instead of a plain slider fill. The
 *        modulation slot's badge sits below the bar (shrinking it slightly), and
 *        an assigned modulator draws a bidirectional line + target circle in the
 *        same style as the horizontal sliders — still modulating from the bar's
 *        value.
 */
class HarmonicSlider : public Control
{
    public:
        HarmonicSlider(ParamBridge& bridge, Synth::ParamId const param_id);

        void paint(juce::Graphics& g) override;

    protected:
        /* The bar occupies the whole cell width, less a strip at the bottom for
         * the modulation badge. */
        juce::Rectangle<float> track_rect() const override;
        juce::Rectangle<float> badge_rect() const override;

    private:
        static constexpr float BADGE_STRIP = 12.0f;   /* badge height under the bar */
        static constexpr float BADGE_GAP = 3.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonicSlider)
};

}

#endif

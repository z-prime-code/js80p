#ifndef JS80P__UI__CURVE_SELECTOR_HPP
#define JS80P__UI__CURVE_SELECTOR_HPP

#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "js80p.hpp"
#include "synth.hpp"

#include "ui/param_bridge.hpp"


namespace JS80P
{

/**
 * \brief A tiny square that plots a modulation transfer curve and edits it on
 *        vertical drag or mouse wheel.
 *
 * Two modes:
 *
 *  - **Envelope discrete-shape mode** (the multi-target constructor): cycles a
 *    discrete shape index through every mirrored target and plots the envelope
 *    shapes.
 *
 *  - **Macro bipolar-curve mode** (the DST/DCV constructor): a single signed
 *    axis \c value in [-1, +1] that drives BOTH the macro's distortion *amount*
 *    (\c DST = |value|) and its distortion *curve type* (\c DCV): drag up for a
 *    logarithmic bend (\c SHARP_SMOOTH), drag down for an exponential one
 *    (\c SMOOTH_SHARP). At the centre \c DST is 0, so the response is linear
 *    regardless of curve type. The two remaining S-shapes (contrast / expand)
 *    are only reachable from the legacy MATRIX tab. Plots \c Math::distort with
 *    the live blended amount.
 */
class CurveSelector : public juce::Component
{
    public:
        CurveSelector(
            ParamBridge& bridge,
            std::vector<Synth::ParamId> targets,
            bool const falling,
            bool const distortion = false
        );

        CurveSelector(
            ParamBridge& bridge,
            Synth::ParamId const dst_id,
            Synth::ParamId const dcv_id
        );

        void refresh();

        void paint(juce::Graphics& g) override;
        void mouseDown(juce::MouseEvent const& event) override;
        void mouseDrag(juce::MouseEvent const& event) override;
        void mouseWheelMove(
            juce::MouseEvent const& event, juce::MouseWheelDetails const& wheel
        ) override;
        void mouseDoubleClick(juce::MouseEvent const& event) override;

    private:
        /* Vertical drag distance (px) that sweeps the bipolar value by 1.0. */
        static constexpr double BIPOLAR_DRAG_UNIT_PX = 64.0;

        void set_index(int const index);

        void set_value(double const v);
        void write_value();
        double read_value() const;

        ParamBridge& bridge;
        std::vector<Synth::ParamId> targets;
        Synth::ParamId dst_id;
        Synth::ParamId dcv_id;
        bool const falling;
        bool const distortion;
        bool const bipolar;
        int index;
        int drag_start;
        double value;
        double drag_start_value;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CurveSelector)
};

}

#endif

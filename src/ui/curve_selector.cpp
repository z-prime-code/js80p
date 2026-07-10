/*
 * This file is part of JS80P, a synthesizer plugin.
 * Copyright (C) 2023, 2024, 2025, 2026  Attila M. Magyar
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

#include <cmath>
#include <utility>

#include "ui/curve_selector.hpp"

#include "dsp/envelope.hpp"
#include "dsp/math.hpp"
#include "ui/theme.hpp"


namespace JS80P
{

namespace
{

/* The envelope curve square is a logical axis, not the raw enum order: drag down
 * for shapes that favour LOW values (slow rise), drag up for shapes that favour
 * HIGH values (fast rise), linear in the centre. Only the two monotone families
 * live on the axis (exponential x^n vs. logarithmic / inverse-exponential),
 * mirroring the macro curve control; the symmetric smoothstep / inverse-S shapes
 * are intentionally left off.
 *
 * A falling segment computes value = 1 - shape(t), so the same shape flips which
 * way it "favours" — hence the falling axis is the rising one reversed, keeping
 * "drag up = favours high values" true for attack, decay and release alike. */
constexpr int ENV_AXIS_LEN = 7;

/* favour-low (slow rise) → linear → favour-high (fast rise) */
constexpr int ENV_AXIS_RISING[ENV_AXIS_LEN] = {
    Math::ENV_SHAPE_SMOOTH_SHARP_STEEPER,   /* x^5  - slowest rise   */
    Math::ENV_SHAPE_SMOOTH_SHARP_STEEP,     /* x^3                   */
    Math::ENV_SHAPE_SMOOTH_SHARP,           /* x^2                   */
    Envelope::SHAPE_LINEAR,                 /* linear - centre       */
    Math::ENV_SHAPE_SHARP_SMOOTH,           /* log                   */
    Math::ENV_SHAPE_SHARP_SMOOTH_STEEP,     /* log^(2/3)             */
    Math::ENV_SHAPE_SHARP_SMOOTH_STEEPER,   /* log^(1/3) - fastest   */
};

constexpr int ENV_AXIS_FALLING[ENV_AXIS_LEN] = {
    Math::ENV_SHAPE_SHARP_SMOOTH_STEEPER,
    Math::ENV_SHAPE_SHARP_SMOOTH_STEEP,
    Math::ENV_SHAPE_SHARP_SMOOTH,
    Envelope::SHAPE_LINEAR,
    Math::ENV_SHAPE_SMOOTH_SHARP,
    Math::ENV_SHAPE_SMOOTH_SHARP_STEEP,
    Math::ENV_SHAPE_SMOOTH_SHARP_STEEPER,
};

int const* env_axis(bool const falling)
{
    return falling ? ENV_AXIS_FALLING : ENV_AXIS_RISING;
}

int env_axis_shape(bool const falling, int const pos)
{
    return env_axis(falling)[juce::jlimit(0, ENV_AXIS_LEN - 1, pos)];
}

/* Position of a shape on the axis; off-axis shapes (a preset picked a symmetric
 * one) fall back to the centre so a drag starts from linear. */
int env_axis_pos(bool const falling, int const shape)
{
    int const* const axis = env_axis(falling);

    for (int i = 0; i != ENV_AXIS_LEN; ++i) {
        if (axis[i] == shape) {
            return i;
        }
    }

    return ENV_AXIS_LEN / 2;
}

}

CurveSelector::CurveSelector(
        ParamBridge& bridge,
        std::vector<Synth::ParamId> targets,
        bool const falling,
        bool const distortion
) : bridge(bridge),
    targets(std::move(targets)),
    dst_id(Synth::ParamId::INVALID_PARAM_ID),
    dcv_id(Synth::ParamId::INVALID_PARAM_ID),
    falling(falling),
    distortion(distortion),
    bipolar(false),
    index(0),
    drag_start(0),
    value(0.0),
    drag_start_value(0.0)
{
    setWantsKeyboardFocus(false);
    index = bridge.get_discrete(this->targets.front());
}


CurveSelector::CurveSelector(
        ParamBridge& bridge,
        Synth::ParamId const dst_id,
        Synth::ParamId const dcv_id
) : bridge(bridge),
    targets(),
    dst_id(dst_id),
    dcv_id(dcv_id),
    falling(false),
    distortion(true),
    bipolar(true),
    index(0),
    drag_start(0),
    value(0.0),
    drag_start_value(0.0)
{
    setWantsKeyboardFocus(false);
    value = read_value();
}


void CurveSelector::refresh()
{
    if (bipolar) {
        double const live = read_value();

        if (std::abs(live - value) > 1e-6) {
            value = live;
            repaint();
        }

        return;
    }

    int const live = bridge.get_discrete(targets.front());

    if (live != index) {
        index = live;
        repaint();
    }
}


double CurveSelector::read_value() const
{
    /* |value| is the distortion amount; its sign is recovered from the curve
     * type (log = up = positive, anything else = down = negative). Curve types
     * set from the MATRIX tab that the axis can't represent read as negative,
     * but the plot below still honours the real amount. */
    double const amount = (double)bridge.get_ratio(dst_id);
    bool const logarithmic = (
        bridge.get_discrete(dcv_id) == Math::DistortionCurve::DIST_CURVE_SHARP_SMOOTH
    );

    return logarithmic ? amount : -amount;
}


void CurveSelector::write_value()
{
    bridge.set_ratio(dst_id, (Number)std::abs(value));
    bridge.set_discrete(
        dcv_id,
        value >= 0.0
            ? Math::DistortionCurve::DIST_CURVE_SHARP_SMOOTH   /* logarithmic */
            : Math::DistortionCurve::DIST_CURVE_SMOOTH_SHARP   /* exponential */
    );
    repaint();
}


void CurveSelector::set_value(double const v)
{
    value = juce::jlimit(-1.0, 1.0, v);
    write_value();
}


void CurveSelector::set_index(int const idx)
{
    int const count = bridge.option_count(targets.front());
    index = juce::jlimit(0, juce::jmax(0, count - 1), idx);

    for (Synth::ParamId const p : targets) {
        bridge.set_discrete(p, index);
    }

    repaint();
}


void CurveSelector::paint(juce::Graphics& g)
{
    juce::Rectangle<float> const box = getLocalBounds().toFloat();
    g.setColour(Theme::INSET);
    g.fillRoundedRectangle(box, 2.0f);

    int const linear = 12;   /* SHAPE_LINEAR; not in Math::EnvelopeShape (0-11) */
    juce::Rectangle<float> const in = box.reduced(2.0f);
    juce::Path curve;

    /* Bipolar macro mode blends the log/exp curve by |value|; the centre is a
     * flat linear ramp. */
    double const bp_amount = std::abs(value);
    Math::DistortionCurve const bp_curve = (
        value >= 0.0
            ? Math::DistortionCurve::DIST_CURVE_SHARP_SMOOTH
            : Math::DistortionCurve::DIST_CURVE_SMOOTH_SHARP
    );

    for (int i = 0; i <= 14; ++i) {
        double const x = (double)i / 14.0;
        double y = bipolar
            ? (double)Math::distort(bp_amount, (Number)x, bp_curve)
            : (distortion
                ? (double)Math::distort(1.0, (Number)x, (Math::DistortionCurve)index)
                : (index >= linear
                    ? x
                    : (double)Math::apply_envelope_shape((Math::EnvelopeShape)index, (Number)x)));
        if (falling) {
            y = 1.0 - y;
        }
        float const px = in.getX() + (float)x * in.getWidth();
        float const py = in.getBottom() - (float)y * in.getHeight();
        if (i == 0) curve.startNewSubPath(px, py);
        else curve.lineTo(px, py);
    }

    /* The bipolar macro curve matches the macro dials' magenta; the legacy
     * distortion mode keeps Theme::MACRO, envelopes use Theme::ENV. */
    g.setColour(bipolar ? Theme::MIDI : (distortion ? Theme::MACRO : Theme::ENV));
    g.strokePath(curve, juce::PathStrokeType(1.4f));
}


void CurveSelector::mouseDown(juce::MouseEvent const& /* event */)
{
    /* Envelope mode drags along the logical axis position, not the raw index. */
    drag_start = env_axis_pos(falling, index);
    drag_start_value = value;
}


void CurveSelector::mouseDrag(juce::MouseEvent const& event)
{
    double const dy = (double)event.getDistanceFromDragStartY();

    if (bipolar) {
        set_value(drag_start_value - dy / BIPOLAR_DRAG_UNIT_PX);

        return;
    }

    int const step = (int)std::lround(-dy / 12.0);
    set_index(env_axis_shape(falling, drag_start + step));
}


void CurveSelector::mouseWheelMove(
        juce::MouseEvent const& /* event */, juce::MouseWheelDetails const& wheel
) {
    double const dir = wheel.deltaY > 0.0f ? 1.0 : -1.0;

    if (bipolar) {
        set_value(value + dir * 0.1);

        return;
    }

    set_index(env_axis_shape(falling, env_axis_pos(falling, index) + (int)dir));
}


void CurveSelector::mouseDoubleClick(juce::MouseEvent const& /* event */)
{
    if (bipolar) {
        set_value(0.0);   /* linear: DST = 0 */

        return;
    }

    set_index(bridge.get_discrete_default(targets.front()));
}

}

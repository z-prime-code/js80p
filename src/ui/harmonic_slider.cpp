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

#include <algorithm>
#include <cmath>

#include "ui/harmonic_slider.hpp"

#include "ui/theme.hpp"


namespace JS80P
{

HarmonicSlider::HarmonicSlider(ParamBridge& bridge, Synth::ParamId const param_id)
    : Control(bridge, param_id, juce::String(), Style::V_SLIDER, Size::NORMAL)
{
    /* No caption / value strip: the bar fills the cell, the badge sits below it. */
    set_label_placement(LabelPos::NONE);
    set_value_display(ValueDisplay::NONE);
}


juce::Rectangle<float> HarmonicSlider::track_rect() const
{
    /* Full-width bar (unlike the base slider's thin centred groove), leaving a
     * strip at the bottom for the modulation badge. */
    juce::Rectangle<float> b = body_bounds().toFloat();
    b.removeFromBottom(BADGE_STRIP + BADGE_GAP);
    return b;
}


juce::Rectangle<float> HarmonicSlider::badge_rect() const
{
    /* Centred under the bar, spanning (nearly) the whole cell width. */
    juce::Rectangle<float> const b = body_bounds().toFloat();
    return juce::Rectangle<float>(
        b.getX() + 1.0f, b.getBottom() - BADGE_STRIP,
        juce::jmax(1.0f, b.getWidth() - 2.0f), BADGE_STRIP
    );
}


void HarmonicSlider::paint(juce::Graphics& g)
{
    juce::Rectangle<float> const cell = track_rect();
    float const mid_y = cell.getCentreY();
    juce::Colour const active = active_colour();

    /* Groove background. */
    g.setColour(Theme::INSET);
    g.fillRect(cell);

    /* Bipolar value fill from the mid (zero) line to the value handle. When a
     * modulator is assigned the bar shows its base value. */
    double const position = to_visual(is_assigned() ? mod_base() : value_ratio());
    juce::Point<float> const h = handle_at(position);

    float const y0 = juce::jmin(mid_y, h.y);
    float const y1 = juce::jmax(mid_y, h.y);
    g.setColour(is_assigned() ? active.withAlpha(0.55f) : active);
    g.fillRect(cell.getX(), y0, cell.getWidth(), y1 - y0);

    /* Value handle marker. */
    g.setColour(active);
    g.fillRect(cell.getX(), h.y - 1.0f, cell.getWidth(), 2.0f);

    /* Zero (mid) line. */
    g.setColour(Theme::EDGE);
    g.drawHorizontalLine((int)mid_y, cell.getX(), cell.getRight());

    /* Modulation reach: a thin bidirectional line from the value handle to the
     * modulation target with a small circle at the target - the same styling the
     * horizontal sliders use, only vertical. */
    if (is_assigned()) {
        double const target = juce::jlimit(0.0, 1.0, position + mod_depth());
        juce::Point<float> const m = handle_at(target);

        g.setColour(active);
        float const ly0 = juce::jmin(h.y, m.y);
        float const ly1 = juce::jmax(h.y, m.y);
        g.fillRect(cell.getCentreX() - 1.0f, ly0, 2.0f, ly1 - ly0);
        g.fillEllipse(m.x - 2.0f, m.y - 2.0f, 4.0f, 4.0f);
    }
}

}

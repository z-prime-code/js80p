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

#include "ui/filter_panel.hpp"

#include "ui/theme.hpp"


namespace JS80P
{

FilterPanel::FilterPanel(
        ParamBridge& bridge,
        ModulationManager& manager,
        Spec const& a,
        Spec const& b,
        juce::String label_a,
        juce::String label_b
) : bridge(bridge),
    label_a(std::move(label_a)),
    label_b(std::move(label_b)),
    active(0)
{
    setWantsKeyboardFocus(false);

    Spec const specs[2] = { a, b };

    for (int f = 0; f != 2; ++f) {
        FilterTypeSelector* const type = new FilterTypeSelector(bridge, specs[f].type);
        types.add(type);
        addChildComponent(type);

        knobs.add(new Knob(bridge, specs[f].freq, "FREQ"));
        knobs.add(new Knob(bridge, specs[f].q,    "Q"));
        knobs.add(new Knob(bridge, specs[f].gain, "GAIN"));

        /* Same exponential cutoff sweep as the effect filters: 20 Hz .. 20 kHz
         * with 1.5 kHz at mid-travel. */
        knobs[f * 3]->set_freq_range(20.0, 20000.0, 1500.0);

        for (int k = 0; k != 3; ++k) {
            knobs[f * 3 + k]->set_manager(&manager);
            addChildComponent(knobs[f * 3 + k]);
        }
    }

    set_active(0);
}


void FilterPanel::set_active(int const index)
{
    active = index;

    for (int f = 0; f != 2; ++f) {
        bool const on = (f == active);
        types[f]->setVisible(on);

        for (int k = 0; k != 3; ++k) {
            knobs[f * 3 + k]->setVisible(on);
        }
    }

    repaint();
}


void FilterPanel::refresh()
{
    for (FilterTypeSelector* const type : types) {
        type->refresh();
    }

    for (Knob* const knob : knobs) {
        knob->refresh();
    }
}


void FilterPanel::resized()
{
    juce::Rectangle<int> b = getLocalBounds().reduced(9);

    juce::Rectangle<int> const header = b.removeFromTop(20);
    int const seg_w = 26;
    int const seg_h = 15;
    seg_b_bounds = juce::Rectangle<int>(header.getRight() - seg_w, header.getY() + 1, seg_w, seg_h);
    seg_a_bounds = juce::Rectangle<int>(seg_b_bounds.getX() - seg_w - 3, header.getY() + 1, seg_w, seg_h);

    b.removeFromTop(2);

    /* Compact section: a 3-column type-selector block on the left, then the
     * CUTOFF / Q / GAIN knobs. Q / GAIN match the oscillator section's knob
     * cell; CUTOFF (FREQ) is the important one, so it is a little larger and
     * offsets upward into the space above the bottom-aligned Q / GAIN. The
     * type selector is shorter than the knobs and top-aligned with them, so the
     * whole section stays compact. */
    juce::Rectangle<int> const row = b;
    int const freq_h = 62;   /* -14: no reserved bottom value strip */
    int const small_h = 56;
    int const bottom = row.getBottom();

    int const type_w = 66;
    int const type_h = 58;
    int const knobs_x = row.getX() + type_w + 4;
    int const avail = row.getRight() - knobs_x;
    int const small_w = avail * 3 / 10;
    int const freq_w = avail - 2 * small_w;

    for (int f = 0; f != 2; ++f) {
        types[f]->setBounds(row.getX(), bottom - freq_h, type_w, type_h);
        knobs[f * 3 + 0]->setBounds(knobs_x, bottom - freq_h, freq_w, freq_h);
        knobs[f * 3 + 1]->setBounds(knobs_x + freq_w, bottom - small_h, small_w, small_h);
        knobs[f * 3 + 2]->setBounds(knobs_x + freq_w + small_w, bottom - small_h, small_w, small_h);
    }
}


void FilterPanel::mouseDown(juce::MouseEvent const& event)
{
    if (seg_a_bounds.contains(event.getPosition())) {
        set_active(0);
    } else if (seg_b_bounds.contains(event.getPosition())) {
        set_active(1);
    }
}


void FilterPanel::paint(juce::Graphics& g)
{
    /* No box - the FilterPanel sits inside its OSC panel; a divider separates it. */
    g.setColour(Theme::EDGE);
    g.fillRect(6, 0, getWidth() - 12, 1);

    g.setColour(Theme::TEXT_DIM);
    g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f).withStyle("Bold")));
    g.drawText("FILTERS", 10, 8, 100, 16, juce::Justification::centredLeft, false);

    juce::Rectangle<int> const segs[2] = { seg_a_bounds, seg_b_bounds };
    juce::String const labels[2] = { label_a, label_b };

    for (int i = 0; i != 2; ++i) {
        bool const on = (i == active);
        g.setColour(on ? Theme::ACCENT : Theme::INSET);
        g.fillRoundedRectangle(segs[i].toFloat(), 2.0f);
        g.setColour(on ? Theme::ACCENT : Theme::EDGE);
        g.drawRoundedRectangle(segs[i].toFloat().reduced(0.5f), 2.0f, 1.0f);

        g.setColour(on ? Theme::BG : Theme::TEXT_DIM);
        g.setFont(11.0f);
        g.drawText(labels[i], segs[i], juce::Justification::centred, false);
    }
}

}

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

#include <memory>

#include "ui/macro_strip.hpp"

#include "ui/curve_selector.hpp"
#include "ui/modulation.hpp"
#include "ui/theme.hpp"


namespace JS80P
{

MacroStrip::MacroStrip(ParamBridge& bridge)
{
    setWantsKeyboardFocus(false);

    for (int m = 1; m <= COUNT; ++m) {
        /* The macro rotary edits its MIN (base) / MAX (depth) directly; the
         * badge picks the input source and only then does it read as modulated.
         * A single bipolar curve square sits at the dial's bottom-right: drag up
         * for a logarithmic response (more DIST), down for exponential, centre
         * for linear. It drives the macro's DST amount + DCV curve type. */
        Control* const cell = new Control(
            bridge, Modulation::macro_min(m), "M" + juce::String(m),
            Control::Style::ROTARY, Control::Size::SMALL
        );
        cell->set_macro(m);
        cell->set_label_placement(Control::LabelPos::TOP);
        cell->set_value_display(Control::ValueDisplay::POPOVER);
        auto curve = std::make_unique<CurveSelector>(
            bridge, Modulation::macro_dst(m), Modulation::macro_dcv(m)
        );
        CurveSelector* const curve_ptr = curve.get();
        cell->set_sub_control(std::move(curve), [curve_ptr]() { curve_ptr->refresh(); });
        cells.add(cell);
        addAndMakeVisible(cell);
    }
}


void MacroStrip::refresh()
{
    for (Control* const cell : cells) {
        cell->refresh();
    }
}


void MacroStrip::resized()
{
    /* Fixed-width cells (dial + badge/curve room) packed with a small gap — about
     * half the whitespace of the old full-width division — and the whole group
     * centred horizontally on the strip. */
    int const n = cells.size();
    if (n == 0) {
        return;
    }

    int const cell_w = 84;
    int const gap = 22;
    int const total = n * cell_w + (n - 1) * gap;
    int x = (getWidth() - total) / 2;

    for (int i = 0; i != n; ++i) {
        cells[i]->setBounds(x, 2, cell_w, getHeight() - 4);
        x += cell_w + gap;
    }
}


void MacroStrip::paint(juce::Graphics& g)
{
    g.setColour(Theme::PANEL);
    g.fillRect(getLocalBounds());
    /* Top and bottom borders matching the oscillator panels' edge. */
    g.setColour(Theme::EDGE);
    g.fillRect(0, 0, getWidth(), 1);
    g.fillRect(0, getHeight() - 1, getWidth(), 1);
}

}

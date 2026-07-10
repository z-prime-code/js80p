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

#include "ui/filter_type_selector.hpp"

#include "ui/theme.hpp"


namespace JS80P
{

/* Frequency-response outline per type, as flat (x, y) pairs with x, y in
 * [0, 1] (y = 1 is the top / high gain). Order: LP HP BP Notch Bell LS HS. */
static float const* filter_glyph_points(int const type, int& point_count)
{
    static float const LP[]    = {0.0f,0.8f,  0.45f,0.78f, 0.7f,0.55f, 1.0f,0.12f};
    static float const HP[]    = {0.0f,0.12f, 0.3f,0.55f,  0.55f,0.78f,1.0f,0.8f};
    /* A downward-opening parabola (rounded arch), sampled y = 0.8 - 2.6 (x-0.5)^2. */
    static float const BP[]    = {0.0f,0.15f, 0.125f,0.43f, 0.25f,0.64f, 0.375f,0.76f,
                                  0.5f,0.8f, 0.625f,0.76f, 0.75f,0.64f, 0.875f,0.43f, 1.0f,0.15f};
    static float const NOTCH[] = {0.0f,0.75f, 0.4f,0.68f,  0.5f,0.15f, 0.6f,0.68f,  1.0f,0.75f};
    static float const BELL[]  = {0.0f,0.45f, 0.35f,0.5f,  0.5f,0.85f, 0.65f,0.5f,  1.0f,0.45f};
    static float const LS[]    = {0.0f,0.82f, 0.4f,0.8f,   0.58f,0.42f,1.0f,0.38f};
    static float const HS[]    = {0.0f,0.38f, 0.42f,0.4f,  0.6f,0.8f,  1.0f,0.82f};

    switch (type) {
        case 0:  point_count = 4; return LP;
        case 1:  point_count = 4; return HP;
        case 2:  point_count = 9; return BP;
        case 3:  point_count = 5; return NOTCH;
        case 4:  point_count = 5; return BELL;
        case 5:  point_count = 4; return LS;
        default: point_count = 4; return HS;
    }
}


/* Fixed 3-column grouping (col, row) per type index. Types are ordered
 * LP HP BP Notch Bell LS HS, so:
 *   col 0: LP, HP, BP        col 1: Notch, Peak(Bell)     col 2: LS, HS   */
static constexpr int GRID_COLS = 3;
static constexpr int GRID_ROWS = 3;
static constexpr int CELL_COL[FilterTypeSelector::COUNT] = { 0, 0, 0, 1, 1, 2, 2 };
static constexpr int CELL_ROW[FilterTypeSelector::COUNT] = { 0, 1, 2, 0, 1, 0, 1 };

/* Full names for the hover popover (order: LP HP BP Notch Bell LS HS). */
static char const* const FILTER_NAMES[FilterTypeSelector::COUNT] = {
    "Low-pass", "High-pass", "Band-pass", "Notch", "Bell", "Low shelf", "High shelf"
};


FilterTypeSelector::FilterTypeSelector(
        ParamBridge& bridge, Synth::ParamId const param_id
) : bridge(bridge),
    param_id(param_id),
    selected(bridge.get_discrete(param_id))
{
    setWantsKeyboardFocus(false);
}


void FilterTypeSelector::refresh()
{
    int const live = bridge.get_discrete(param_id);

    if (live != selected) {
        selected = live;
        repaint();
    }
}


void FilterTypeSelector::draw_glyph(
        juce::Graphics& g, juce::Rectangle<float> area, int const type
) const {
    int count = 0;
    float const* const points = filter_glyph_points(type, count);
    juce::Path path;

    for (int i = 0; i != count; ++i) {
        float const x = area.getX() + points[i * 2] * area.getWidth();
        float const y = area.getBottom() - points[i * 2 + 1] * area.getHeight();

        if (i == 0) {
            path.startNewSubPath(x, y);
        } else {
            path.lineTo(x, y);
        }
    }

    g.strokePath(
        path,
        juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
    );
}


void FilterTypeSelector::paint(juce::Graphics& g)
{
    int const w = getWidth() / GRID_COLS;
    int const h = getHeight() / GRID_ROWS;

    for (int i = 0; i != COUNT; ++i) {
        juce::Rectangle<int> const cell =
            juce::Rectangle<int>(CELL_COL[i] * w, CELL_ROW[i] * h, w, h).reduced(2);
        bool const is_selected = (i == selected);

        g.setColour(is_selected ? Theme::PANEL_2 : Theme::INSET);
        g.fillRoundedRectangle(cell.toFloat(), 2.0f);

        if (is_selected) {
            g.setColour(Theme::ACCENT);
            g.drawRoundedRectangle(cell.toFloat().reduced(0.5f), 2.0f, 1.2f);
        }

        g.setColour(is_selected ? Theme::ACCENT : Theme::TEXT_DIM);
        draw_glyph(g, cell.toFloat().reduced(3.0f), i);
    }
}


void FilterTypeSelector::mouseDown(juce::MouseEvent const& event)
{
    int const index = index_at(event.getPosition());

    if (index >= 0) {
        selected = index;
        bridge.set_discrete(param_id, index);
        repaint();
    }
}


int FilterTypeSelector::index_at(juce::Point<int> const p) const
{
    int const col = p.x / juce::jmax(1, getWidth() / GRID_COLS);
    int const row = p.y / juce::jmax(1, getHeight() / GRID_ROWS);

    for (int i = 0; i != COUNT; ++i) {
        if (CELL_COL[i] == col && CELL_ROW[i] == row) {
            return i;
        }
    }
    return -1;
}


juce::Rectangle<int> FilterTypeSelector::cell_bounds(int const type) const
{
    int const w = getWidth() / GRID_COLS;
    int const h = getHeight() / GRID_ROWS;
    return juce::Rectangle<int>(CELL_COL[type] * w, CELL_ROW[type] * h, w, h);
}


void FilterTypeSelector::update_name_popover()
{
    if (hovered < 0 || hovered >= COUNT) {
        ValuePopover::hide(name_popover);
        return;
    }

    ValuePopover::show(
        name_popover, *this, cell_bounds(hovered), {}, FILTER_NAMES[hovered], Theme::ACCENT
    );
}


void FilterTypeSelector::mouseMove(juce::MouseEvent const& event)
{
    int const index = index_at(event.getPosition());
    if (index != hovered) {
        hovered = index;
        update_name_popover();
    }
}


void FilterTypeSelector::mouseEnter(juce::MouseEvent const& event)
{
    hovered = index_at(event.getPosition());
    update_name_popover();
}


void FilterTypeSelector::mouseExit(juce::MouseEvent const& /* event */)
{
    hovered = -1;
    ValuePopover::hide(name_popover);
}

}

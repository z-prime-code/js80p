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

#ifndef JS80P__UI__FILTER_TYPE_SELECTOR_HPP
#define JS80P__UI__FILTER_TYPE_SELECTOR_HPP

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "js80p.hpp"
#include "synth.hpp"

#include "ui/param_bridge.hpp"
#include "ui/value_popover.hpp"


namespace JS80P
{

/**
 * \brief The 7 biquad filter types as vector-drawn icon buttons (frequency-
 *        response glyphs), bound to a discrete filter-type parameter. Laid out
 *        in a fixed 3-column grouping - column 0: LP HP BP; column 1: Notch,
 *        Peak; column 2: low shelf, high shelf. Order matches
 *        SimpleBiquadFilter: LP HP BP Notch Bell LS HS.
 */
class FilterTypeSelector : public juce::Component
{
    public:
        static constexpr int COUNT = 7;

        FilterTypeSelector(
            ParamBridge& bridge,
            Synth::ParamId const param_id
        );

        void refresh();

        void paint(juce::Graphics& g) override;
        void mouseDown(juce::MouseEvent const& event) override;
        void mouseMove(juce::MouseEvent const& event) override;
        void mouseEnter(juce::MouseEvent const& event) override;
        void mouseExit(juce::MouseEvent const& event) override;

    private:
        int index_at(juce::Point<int> const p) const;
        juce::Rectangle<int> cell_bounds(int const type) const;
        void update_name_popover();
        void draw_glyph(
            juce::Graphics& g, juce::Rectangle<float> area, int const type
        ) const;

        ParamBridge& bridge;
        Synth::ParamId const param_id;
        int selected;
        int hovered { -1 };
        std::unique_ptr<ValuePopover> name_popover;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterTypeSelector)
};

}

#endif

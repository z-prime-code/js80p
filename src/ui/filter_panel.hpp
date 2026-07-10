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

#ifndef JS80P__UI__FILTER_PANEL_HPP
#define JS80P__UI__FILTER_PANEL_HPP

#include <juce_gui_basics/juce_gui_basics.h>

#include "js80p.hpp"
#include "synth.hpp"

#include "ui/filter_type_selector.hpp"
#include "ui/knob.hpp"
#include "ui/modulation_manager.hpp"
#include "ui/param_bridge.hpp"


namespace JS80P
{

/**
 * \brief A bordered container editing two of an oscillator's filters (e.g.
 *        1 | 2 under Osc 1, or 3 | 4 under Osc 2). A segment button group
 *        chooses which filter is shown; each filter is a filter-type icon
 *        button group over FREQ / Q / GAIN knobs.
 */
class FilterPanel : public juce::Component
{
    public:
        struct Spec {
            Synth::ParamId type;
            Synth::ParamId freq;
            Synth::ParamId q;
            Synth::ParamId gain;
        };

        FilterPanel(
            ParamBridge& bridge,
            ModulationManager& manager,
            Spec const& a,
            Spec const& b,
            juce::String label_a,
            juce::String label_b
        );

        void refresh();

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(juce::MouseEvent const& event) override;

    private:
        void set_active(int const index);

        ParamBridge& bridge;
        juce::String const label_a;
        juce::String const label_b;
        int active;

        juce::OwnedArray<FilterTypeSelector> types;   /* [0] = a, [1] = b */
        juce::OwnedArray<Knob> knobs;                 /* 0-2 = a, 3-5 = b */

        juce::Rectangle<int> seg_a_bounds;
        juce::Rectangle<int> seg_b_bounds;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterPanel)
};

}

#endif

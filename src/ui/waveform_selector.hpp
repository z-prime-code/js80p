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

#ifndef JS80P__UI__WAVEFORM_SELECTOR_HPP
#define JS80P__UI__WAVEFORM_SELECTOR_HPP

#include <functional>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "js80p.hpp"
#include "synth.hpp"

#include "ui/param_bridge.hpp"
#include "ui/value_popover.hpp"


namespace JS80P
{

/**
 * \brief The 14 oscillator/LFO waveforms as a 2-row grid of vector-drawn icon
 *        buttons (no dropdown). Bound to a discrete waveform parameter.
 */
class WaveformSelector : public juce::Component
{
    public:
        /* Oscillator::Waveform values (see src/dsp/oscillator.hpp). */
        static constexpr int SHAPE_COUNT = 14;
        static constexpr int CUSTOM = 13;

        /* Grid shape of the expanded picker (used by ModulatorCard to size the
         * collapsed selected-shape button to match one cell). */
        static constexpr int COLUMNS = 7;
        static constexpr int ROWS = 2;

        WaveformSelector(ParamBridge& bridge, Synth::ParamId const param_id);

        /** Collapsed mode: draw only the current shape as one button. */
        void set_single(bool const on);
        void set_on_click(std::function<void()> cb);
        void set_on_select(std::function<void(int)> cb);

        void refresh();

        void paint(juce::Graphics& g) override;
        void mouseDown(juce::MouseEvent const& event) override;
        void mouseMove(juce::MouseEvent const& event) override;
        void mouseEnter(juce::MouseEvent const& event) override;
        void mouseExit(juce::MouseEvent const& event) override;

    private:
        juce::Rectangle<int> cell_bounds(int const index) const;
        int index_at(juce::Point<int> const p) const;
        void update_name_popover();
        void draw_glyph(
            juce::Graphics& g, juce::Rectangle<float> area, int const shape
        ) const;

        ParamBridge& bridge;
        Synth::ParamId const param_id;
        int selected;
        int count;
        bool single;
        int hovered { -1 };
        std::unique_ptr<ValuePopover> name_popover;
        std::function<void()> on_click;
        std::function<void(int)> on_select;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformSelector)
};

}

#endif

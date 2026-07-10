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

#ifndef JS80P__UI__MINI_BUTTON_HPP
#define JS80P__UI__MINI_BUTTON_HPP

#include <functional>
#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

#include "synth.hpp"

#include "ui/param_bridge.hpp"


namespace JS80P
{

/**
 * \brief A small orange toggle/cycle button bound to a discrete parameter,
 *        styled like the LFO BPM toggle: orange outline when the value is 0,
 *        solid orange when non-zero. Clicking advances to the next option
 *        (wrapping); for a two-option parameter that is a plain on/off toggle.
 *        A fixed label is shown, or the current option's name when option
 *        labels are supplied (e.g. the side-chain COMP / EXPD mode).
 *
 *        A second form is a plain *action* button (fixed label + click handler,
 *        no parameter) drawn in the outline state, used for the header's INIT /
 *        preset actions so they match the BPM / COMP styling exactly.
 */
class MiniButton : public juce::Component
{
    public:
        MiniButton(
            ParamBridge& bridge,
            Synth::ParamId const param_id,
            juce::String label
        );

        /** Action button: outline styling, fires \c on_click, no bound param. */
        MiniButton(juce::String label, std::function<void()> on_click);

        /** The preset-action glyphs baked into the legacy synth.png sprite. */
        enum class Icon { OPEN, RANDOMIZE, SAVE };

        /** Icon action button: draws \c icon (tinted to the accent colour) with
         *  generous horizontal padding, fires \c on_click, no bound param. */
        MiniButton(juce::Image icon, std::function<void()> on_click);

        /** Crop one of the preset-action glyphs from the embedded synth.png. */
        static juce::Image preset_icon(Icon which);

        /** Show the current option's name (from \c labels) instead of the fixed
         *  label text. */
        void set_option_labels(juce::StringArray labels);

        /** Width that fits the widest text this button can show. */
        int preferred_width() const;

        void refresh();

        void paint(juce::Graphics& g) override;
        void mouseUp(juce::MouseEvent const& event) override;
        void mouseEnter(juce::MouseEvent const& event) override;
        void mouseExit(juce::MouseEvent const& event) override;

    private:
        ParamBridge* bridge;
        Synth::ParamId const param_id;
        juce::String label;
        juce::StringArray option_labels;
        std::function<void()> on_click;   /* set for action buttons */
        juce::Image icon;                 /* set for icon action buttons */
        bool action;
        int value;
        bool hover;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MiniButton)
};

}

#endif

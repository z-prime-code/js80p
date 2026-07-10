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

#ifndef JS80P__UI__MACRO_STRIP_HPP
#define JS80P__UI__MACRO_STRIP_HPP

#include <juce_gui_basics/juce_gui_basics.h>

#include "js80p.hpp"
#include "synth.hpp"

#include "ui/control.hpp"
#include "ui/param_bridge.hpp"


namespace JS80P
{

/**
 * \brief The top performance strip: macros 1-8, each a Control (macro mode) whose
 *        rotary is the macro's minimum output and whose source badge sets the
 *        maximum (the CC modulation range) and picks the MIDI input source. See
 *        doc/z-gui.md.
 */
class MacroStrip : public juce::Component
{
    public:
        static constexpr int COUNT = 8;

        explicit MacroStrip(ParamBridge& bridge);

        void refresh();

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        juce::OwnedArray<Control> cells;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MacroStrip)
};

}

#endif

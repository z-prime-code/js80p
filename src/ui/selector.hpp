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

#ifndef JS80P__UI__SELECTOR_HPP
#define JS80P__UI__SELECTOR_HPP

#include <juce_gui_basics/juce_gui_basics.h>

#include "js80p.hpp"
#include "synth.hpp"

#include "ui/param_bridge.hpp"


namespace JS80P
{

/**
 * \brief A compact discrete-parameter chooser: a caption + a boxed current
 *        option with a chevron; clicking opens a popup of the options. Bound to
 *        a discrete Synth parameter (mode / filter type / distortion type).
 */
class Selector : public juce::Component
{
    public:
        Selector(
            ParamBridge& bridge,
            Synth::ParamId const param_id,
            juce::StringArray options,
            juce::String caption
        );

        /**
         * \brief Also write the selected index to \p mirror_param_id on every
         *        selection, while this Selector keeps reading/refreshing from
         *        its primary param. Used to drive two params (e.g. both
         *        oscillators' tuning) from a single control.
         */
        void set_mirror(Synth::ParamId const mirror_param_id);

        void refresh();

        void paint(juce::Graphics& g) override;
        void mouseDown(juce::MouseEvent const& event) override;

    private:
        static constexpr Synth::ParamId NO_MIRROR = Synth::ParamId::INVALID_PARAM_ID;

        juce::String option_label(int const index) const;

        ParamBridge& bridge;
        Synth::ParamId const param_id;
        Synth::ParamId mirror_param_id;
        juce::StringArray const options;
        juce::String const caption;
        int selected;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Selector)
};

}

#endif

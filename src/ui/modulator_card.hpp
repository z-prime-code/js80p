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

#ifndef JS80P__UI__MODULATOR_CARD_HPP
#define JS80P__UI__MODULATOR_CARD_HPP

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "js80p.hpp"
#include "synth.hpp"

#include "ui/curve_selector.hpp"
#include "ui/control.hpp"
#include "ui/knob.hpp"
#include "ui/modulation.hpp"
#include "ui/modulation_manager.hpp"
#include "ui/param_bridge.hpp"
#include "ui/waveform_selector.hpp"


namespace JS80P
{

/**
 * \brief One editor for a group of pooled slots sharing a shape (see
 *        doc/z-gui.md 5.2): an envelope's A/H/D/R or an LFO's waveform + rate,
 *        edited once and propagated to every duplicate slot in the group, with
 *        the group's destinations shown as badges.
 */
class ModulatorCard : public juce::Component
{
    public:
        ModulatorCard(
            ParamBridge& bridge, ModulationManager& manager,
            ModulationManager::Group const& group
        );

        /** LFO cards are taller to fit the 2-row waveform button group. */
        int preferred_height() const;

        /** Copy the representative slot's shape to every group member. */
        void propagate();
        void refresh();

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        /**
         * \brief LFO tempo-sync (BPM) toggle: an orange-outlined button when
         *        off, filled solid orange when on. A real child component, so
         *        the whole drawn area is clickable and it shows a hover cue.
         */
        class SyncButton : public juce::Component
        {
            public:
                std::function<bool()> is_active;   /* current on/off state */
                std::function<void()> on_toggle;   /* invoked on click */

                void paint(juce::Graphics& g) override;
                void mouseUp(juce::MouseEvent const& event) override;
                void mouseEnter(juce::MouseEvent const& event) override;
                void mouseExit(juce::MouseEvent const& event) override;

            private:
                bool hover = false;
        };

        void set_expanded(bool const expanded);
        void toggle_sync();
        void write_sustain();   /* SUS = INI + fraction*(PK-INI) per member */

        ParamBridge& bridge;
        ModulationManager& manager;
        Modulation::Type type;
        int rep;
        std::vector<int> members;
        std::vector<Synth::ParamId> destinations;
        std::vector<std::pair<int, Synth::ParamId>> connections;

        juce::OwnedArray<Knob> knobs;        /* env D/A/H/D/R or LFO rate/phase/... */
        juce::OwnedArray<CurveSelector> curves;   /* env attack/decay/release shapes */
        std::unique_ptr<Control> sus_fader;   /* env sustain fraction (V_SLIDER) */
        std::unique_ptr<Control> env_scale;   /* env SCL (H_SLIDER, header row) */
        std::unique_ptr<Control> tin_dot;   /* env time inaccuracy, header far-right */
        std::unique_ptr<Control> vin_dot;   /* env level inaccuracy, header far-right */
        double sus_fraction;
        std::unique_ptr<WaveformSelector> wave;        /* LFO shape button */
        std::unique_ptr<WaveformSelector> shape_grid;  /* LFO shape picker */
        bool lfo_expanded;
        std::unique_ptr<SyncButton> sync_button;   /* LFO tempo-sync (BPM) toggle */

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulatorCard)
};

}

#endif

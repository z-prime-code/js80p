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

#ifndef JS80P__UI__EFFECTS_PAGE_HPP
#define JS80P__UI__EFFECTS_PAGE_HPP

#include <initializer_list>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "js80p.hpp"
#include "synth.hpp"

#include "ui/knob.hpp"
#include "ui/mini_button.hpp"
#include "ui/control.hpp"
#include "ui/modulation_manager.hpp"
#include "ui/param_bridge.hpp"


namespace JS80P
{

/**
 * \brief The EFFECTS page of the new GUI: recreates the original GUI's effect
 *        chain (input/output volumes, two distortions, two filters, tape,
 *        chorus, echo, reverb) as boxes of controls in three visual tiers:
 *        large Knobs for the primary controls, ~2/3-size medium Knobs for the
 *        related secondary controls next to them, and tiny Control DOT pie-dots
 *        (clustered, right of each panel) for infrequently used trims. Discrete
 *        "type"/mode params are shown as knobs that step through the options;
 *        the old logarithmic-scale toggles are intentionally omitted. Scrolls
 *        vertically via an internal Viewport when the body is short.
 */
class EffectsPage : public juce::Component
{
    public:
        EffectsPage(ParamBridge& bridge, ModulationManager& manager);

        void resized() override;

        /** Re-read every knob from the live Synth (called from NewGui's timer). */
        void refresh();

    private:
        struct KnobSpec
        {
            Synth::ParamId id;
            char const* label;
        };

        /* One control in a panel's left-to-right row. Either a knob (large or
         * medium) or a combined WET/DRY MIX cell (always large). Keeping the row
         * as one ordered list lets a large knob sit anywhere among the mediums
         * (e.g. TAPE's trailing STOP, or WIDTH after TYPE/DIST). */
        struct Cell
        {
            Knob* knob = nullptr;    /* large or medium knob */
            Control* mix = nullptr;  /* combined WET/DRY cell (large) */
            bool medium = false;     /* ~4/5-size related control */
            Synth::ParamId id = Synth::ParamId::PARAM_ID_COUNT;   /* knob's param */
        };

        /* A title-bar button pinned above (centred over) the knob it affects,
         * optionally followed by more buttons laid out to its right. */
        struct AnchoredButton
        {
            MiniButton* button;
            Synth::ParamId anchor;              /* the cell knob to centre over */
            std::vector<MiniButton*> trailing;  /* placed to the right, in order */
        };

        struct Panel
        {
            juce::String title;
            int row;          /* 0 = top compact strip; >=1 = full-width rows */
            std::vector<Cell> cells;                 /* controls, left to right */
            std::vector<MiniButton*> buttons;        /* title bar, right-aligned */
            std::vector<AnchoredButton> anchored;    /* title bar, over a knob */
            Control* header_mix = nullptr;           /* WET/DRY pie, title bar right */
            Control* header_knob = nullptr;          /* single-param pie, title bar */
            bool fill = false;   /* stretch to share the row's leftover width */
            int right_pad = -1;  /* explicit right pad (px); -1 = auto (MOD_RIGHT_PAD) */
            juce::Rectangle<int> bounds;
        };

        /* Inner scroll content: draws the panel boxes behind the knobs. */
        class Content : public juce::Component
        {
            public:
                explicit Content(EffectsPage& owner) : owner(owner) {}
                void paint(juce::Graphics& g) override;

            private:
                EffectsPage& owner;
        };

        int begin_panel(juce::String title, int const row);
        Knob* make_knob(KnobSpec const& spec, bool const medium);
        void add_large(int const panel, std::initializer_list<KnobSpec> const specs);
        void add_medium(int const panel, std::initializer_list<KnobSpec> const specs);
        void add_mix(int const panel, Synth::ParamId const wet, Synth::ParamId const dry);
        /** Add a single-param pie to the panel's title bar (like the MIX pie but
         *  bound directly to one parameter, e.g. TAPE's HISS). */
        Control* add_header_knob(int const panel, Synth::ParamId const id, juce::String label);
        MiniButton* add_button(
            int const panel,
            Synth::ParamId const id,
            juce::String label,
            Synth::ParamId const anchor = Synth::ParamId::PARAM_ID_COUNT
        );
        /** Add a button laid out to the right of the panel's last anchored
         *  button (which must already exist). */
        MiniButton* add_button_trailing(
            int const panel, Synth::ParamId const id, juce::String label
        );
        /** True when a panel's last (right-most) cell is a modulation
         *  destination, so its mod badge overhangs the panel's right edge. */
        bool last_cell_modulatable(Panel const& panel) const;
        juce::Point<int> panel_size(Panel const& panel) const;
        void place_panel(Panel& panel);
        void layout();
        void paint_content(juce::Graphics& g);

        ParamBridge& bridge;
        ModulationManager& manager;
        Content content;
        juce::Viewport viewport;
        juce::OwnedArray<Knob> knobs;
        juce::OwnedArray<Control> mix_knobs;
        juce::OwnedArray<MiniButton> buttons;
        std::vector<Panel> panels;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsPage)
};

}

#endif

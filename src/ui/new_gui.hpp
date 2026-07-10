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

#ifndef JS80P__UI__NEW_GUI_HPP
#define JS80P__UI__NEW_GUI_HPP

#include <functional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "js80p.hpp"
#include "synth.hpp"

#include <string>

#include "ui/control.hpp"
#include "ui/effects_page.hpp"
#include "ui/filter_panel.hpp"
#include "ui/knob.hpp"
#include "ui/macro_strip.hpp"
#include "ui/mini_button.hpp"
#include "ui/modulation_manager.hpp"
#include "ui/modulator_card.hpp"
#include "ui/param_bridge.hpp"
#include "ui/per_type_editor.hpp"
#include "ui/selector.hpp"
#include "ui/waveform_selector.hpp"


namespace JS80P
{

/**
 * \brief The new simplified editor surface (work in progress). Opaque top-level
 *        component: header + left two-thirds (Osc 1 / Mix / Osc 2) + right-third
 *        Modulators panel. Each oscillator has a waveform selector over amp /
 *        tuning / type knob rows and a two-filter panel; the Mix column has the
 *        mode selector over MIX/PM/FM/AM. All controls bound to the live Synth;
 *        the original GUI stays available behind an editor-owned toggle.
 */
class NewGui : public juce::Component, private juce::Timer
{
    public:
        enum class Page { SYNTH, EFFECTS, MATRIX };

        explicit NewGui(Synth& synth);
        ~NewGui() override;

        /** Notified (with true) when the MATRIX tab becomes active and (with
         *  false) when it is left, so the host editor can show / hide the embedded
         *  legacy GUI in the body area. */
        std::function<void(bool)> on_matrix;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseUp(juce::MouseEvent const& event) override;

    private:
        void timerCallback() override;

        void set_page(Page const page);
        void set_synth_visible(bool const visible);
        void paint_tabs(juce::Graphics& g);

        Knob& add_knob(std::vector<Knob*>& column, Synth::ParamId const id, char const* const label);
        Control* add_dot(Synth::ParamId const id, char const* const title, char const* const tooltip);
        WaveformSelector* add_wave(Synth::ParamId const id);
        Selector* add_selector(Synth::ParamId const id, juce::StringArray options, juce::String caption);
        FilterPanel* add_filters(
            FilterPanel::Spec const& a,
            FilterPanel::Spec const& b,
            juce::String label_a,
            juce::String label_b
        );

        void lay_out_osc(
            juce::Rectangle<int> panel,
            WaveformSelector* wave,
            std::vector<Knob*>& main,
            PerTypeEditor* per_type
        );
        void lay_out_mix(
            juce::Rectangle<int> panel,
            Selector* mode,
            Selector* tuning,
            Selector* poly,
            std::vector<Knob*>& knobs
        );
        void draw_panel(juce::Graphics& g, juce::Rectangle<int> const& r, char const* const title) const;
        void draw_section_title(juce::Graphics& g, juce::Rectangle<int> const& r, char const* const title) const;

        void rebuild_cards();
        void layout_cards();
        void init_patch();

        /* Header preset actions (mirroring the original GUI's buttons). */
        void open_preset();
        void save_preset();
        void randomize_preset();

        /* RANDOMIZE runs on the audio thread and consumes macro slots from 1
         * upward, clobbering the 8 performance macros. randomize_preset() snaps
         * their input + range first; once the message has drained,
         * restore_performance_macros() relocates the random patch's use of
         * macros 1-8 onto free pool slots (9-30) and writes the captured values
         * back into 1-8. macro_signature() is a cheap fingerprint used to detect
         * that the randomize has landed. */
        void restore_performance_macros();
        double macro_signature() const;

        struct SavedMacro {
            Synth::ControllerId input;
            double min;
            double max;
        };

        ParamBridge bridge;
        /* Header action buttons, styled like the BPM / COMP mini buttons:
         * INIT, then OPEN / RND / SAVE preset actions, left-aligned. */
        juce::OwnedArray<MiniButton> header_buttons;
        std::unique_ptr<juce::FileChooser> file_chooser;
        /* Global effect output volume (EV3V), a header knob to the right of the
         * tabs with its OUT caption drawn to the left (no value readout). */
        std::unique_ptr<Knob> out_knob;
        juce::Rectangle<int> out_label_bounds;
        ModulationManager manager;
        MacroStrip macro_strip;
        EffectsPage effects_page;
        Page active_page;

        /* Performance-macro preservation across RANDOMIZE (see above). */
        SavedMacro saved_macros[MacroStrip::COUNT];
        int restore_macros_ticks;    /* >0 while waiting for randomize to drain */
        double randomize_signature;  /* macro fingerprint captured pre-randomize */
        /* Enables the small dot controls' tooltips (osc/env inaccuracy). */
        juce::TooltipWindow tooltip_window { this };
        juce::Viewport mod_viewport;
        juce::Component mod_content;
        juce::OwnedArray<ModulatorCard> cards;
        std::string card_sig;
        juce::OwnedArray<Knob> knobs;
        juce::OwnedArray<Control> dots;
        juce::OwnedArray<WaveformSelector> waves;
        juce::OwnedArray<Selector> selectors;
        juce::OwnedArray<FilterPanel> filters;
        juce::OwnedArray<PerTypeEditor> type_editors;

        WaveformSelector* osc1_wave;
        WaveformSelector* osc2_wave;
        Selector* mode_selector;
        Selector* tuning_selector;
        Selector* poly_selector;
        FilterPanel* osc1_filters;
        FilterPanel* osc2_filters;
        PerTypeEditor* osc1_type;
        PerTypeEditor* osc2_type;

        /* Tiny pie-fill dots at the top-right of each oscillator title:
         * inaccuracy + instability. */
        Control* osc1_inacc;
        Control* osc1_instab;
        Control* osc2_inacc;
        Control* osc2_instab;

        std::vector<Knob*> osc1;
        std::vector<Knob*> osc2;
        std::vector<Knob*> mix;

        juce::Rectangle<int> header_bounds;
        juce::Rectangle<int> tab_synth_bounds;
        juce::Rectangle<int> tab_effects_bounds;
        juce::Rectangle<int> tab_matrix_bounds;
        juce::Rectangle<int> body_bounds;
        juce::Rectangle<int> osc1_bounds;
        juce::Rectangle<int> osc1_panel_bounds;   /* full column incl. filters */
        juce::Rectangle<int> mix_bounds;
        juce::Rectangle<int> osc2_bounds;
        juce::Rectangle<int> osc2_panel_bounds;
        juce::Rectangle<int> mod_bounds;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NewGui)
};

}

#endif

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

#include <cmath>
#include <map>
#include <set>
#include <utility>

#include "ui/new_gui.hpp"

#include "serializer.hpp"

#include "ui/param_labels.hpp"
#include "ui/theme.hpp"


namespace JS80P
{

static juce::StringArray const MODES {
    "Mix&Mod",
    "Split C3", "Split Db3", "Split D3", "Split Eb3", "Split E3", "Split F3",
    "Split Gb3", "Split G3", "Split Ab3", "Split A3", "Split Bb3", "Split B3",
    "Split C4"
};


static juce::StringArray const TUNINGS {
    "440 12TET",
    "432 12TET",
    "C MTS-ESP",
    "N MTS-ESP"
};


/* Note-handling / polyphony modes (Synth::ParamId::NH), matching the order of
 * the engine's NOTE_HANDLING_* constants. */
static juce::StringArray const POLY_MODES {
    "MONO", "M HOLD", "M IS", "M H IS",
    "POLY", "P HOLD", "P IS", "P H IS",
    "P RET", "P H RET", "P H R IS"
};


NewGui::NewGui(Synth& synth)
    : bridge(synth),
    manager(bridge),
    macro_strip(bridge),
    effects_page(bridge, manager),
    active_page(Page::SYNTH),
    saved_macros(),
    restore_macros_ticks(0),
    randomize_signature(0.0),
    osc1_wave(nullptr),
    osc2_wave(nullptr),
    mode_selector(nullptr),
    tuning_selector(nullptr),
    poly_selector(nullptr),
    osc1_filters(nullptr),
    osc2_filters(nullptr),
    osc1_type(nullptr),
    osc2_type(nullptr),
    osc1_inacc(nullptr),
    osc1_instab(nullptr),
    osc2_inacc(nullptr),
    osc2_instab(nullptr)
{
    setOpaque(true);

    addAndMakeVisible(macro_strip);

    mod_viewport.setViewedComponent(&mod_content, false);
    mod_viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(mod_viewport);

    /* Header action buttons, styled exactly like the BPM / COMP mini buttons:
     * INIT (reset patch) followed by the OPEN / RND / SAVE preset actions. */
    auto add_header_button = [this](juce::String label, std::function<void()> action) {
        MiniButton* const button = new MiniButton(std::move(label), std::move(action));
        header_buttons.add(button);
        addAndMakeVisible(button);
    };
    auto add_header_icon = [this](MiniButton::Icon icon, std::function<void()> action) {
        MiniButton* const button =
            new MiniButton(MiniButton::preset_icon(icon), std::move(action));
        header_buttons.add(button);
        addAndMakeVisible(button);
    };
    /* INIT is a text button; OPEN / RND / SAVE reuse the folder / dice / download
     * glyphs from the legacy synth.png, tinted orange. */
    add_header_button("INIT", [this]() { init_patch(); });
    add_header_icon(MiniButton::Icon::OPEN,      [this]() { open_preset(); });
    add_header_icon(MiniButton::Icon::RANDOMIZE, [this]() { randomize_preset(); });
    add_header_icon(MiniButton::Icon::SAVE,      [this]() { save_preset(); });

    /* Global effect output volume, promoted to a header knob (right of the
     * tabs). Bare dial with the OUT caption drawn to its left; a macro-
     * modulation destination like the other effect volumes. */
    out_knob = std::make_unique<Knob>(
        bridge, Synth::ParamId::EV3V, "OUT", Control::Style::ROTARY, Control::Size::SMALL
    );
    out_knob->set_manager(&manager);
    out_knob->set_mod_caps(Modulation::CAP_LFO | Modulation::CAP_MACRO);   /* EV3V: LFO + macro */
    out_knob->set_bare(true);
    out_knob->set_source_placeholder(true);   /* always show an (empty) modulation box */
    out_knob->set_badge_centred(true);   /* modulation box centred on the dial, not top-right */
    addAndMakeVisible(*out_knob);

    /* The EFFECTS page shares the body area; hidden until its tab is active. */
    addChildComponent(effects_page);

    /* Osc 1 (modulator). */
    osc1_wave = add_wave(Synth::ParamId::MWFM);
    add_knob(osc1, Synth::ParamId::MAMP, "AMP");
    add_knob(osc1, Synth::ParamId::MVS,  "VEL");
    add_knob(osc1, Synth::ParamId::MWID, "WID");
    add_knob(osc1, Synth::ParamId::MPAN, "PAN");
    add_knob(osc1, Synth::ParamId::MDTN, "DTN").set_semitone_snap(true);
    add_knob(osc1, Synth::ParamId::MFIN, "FIN").set_semitone_snap(true);
    add_knob(osc1, Synth::ParamId::MPRD, "PRD");
    add_knob(osc1, Synth::ParamId::MPRT, "PRT");
    add_knob(osc1, Synth::ParamId::MN,   "NOISE");
    add_knob(osc1, Synth::ParamId::MFLD, "FOLD");
    add_knob(osc1, Synth::ParamId::MSUB, "SUB");
    osc1_filters = add_filters(
        { Synth::ParamId::MF1TYP, Synth::ParamId::MF1FRQ, Synth::ParamId::MF1Q, Synth::ParamId::MF1G },
        { Synth::ParamId::MF2TYP, Synth::ParamId::MF2FRQ, Synth::ParamId::MF2Q, Synth::ParamId::MF2G },
        "1", "2"
    );
    osc1_type = new PerTypeEditor(
        bridge, manager, Synth::ParamId::MWFM, Synth::ParamId::MPW, Synth::ParamId::MC1
    );
    type_editors.add(osc1_type);
    addAndMakeVisible(osc1_type);

    /* Osc 1 (modulator) inaccuracy + instability dots. */
    osc1_inacc = add_dot(Synth::ParamId::MOIA, "Inaccuracy", "Oscillator inaccuracy");
    osc1_instab = add_dot(Synth::ParamId::MOIS, "Instability", "Oscillator instability");

    /* Mix column. */
    mode_selector = add_selector(Synth::ParamId::MODE, MODES, "MODE");
    /* Single combined per-oscillator tuning: reads/displays OSC 1 (modulator)
     * tuning, applies the choice to both oscillators via the mirror param. */
    tuning_selector = add_selector(Synth::ParamId::MTUN, TUNINGS, "TUNING");
    tuning_selector->set_mirror(Synth::ParamId::CTUN);
    /* Polyphony / note-handling selector (above TUNING). */
    poly_selector = add_selector(Synth::ParamId::NH, POLY_MODES, "POLY");
    add_knob(mix, Synth::ParamId::MIX, "MIX");
    add_knob(mix, Synth::ParamId::PM,  "PM");
    add_knob(mix, Synth::ParamId::FM,  "FM");
    add_knob(mix, Synth::ParamId::AM,  "AM");

    /* Osc 2 (carrier). Its two filters are the 3rd and 4th overall. */
    osc2_wave = add_wave(Synth::ParamId::CWFM);
    add_knob(osc2, Synth::ParamId::CAMP, "AMP");
    add_knob(osc2, Synth::ParamId::CVS,  "VEL");
    add_knob(osc2, Synth::ParamId::CWID, "WID");
    add_knob(osc2, Synth::ParamId::CPAN, "PAN");
    add_knob(osc2, Synth::ParamId::CDTN, "DTN").set_semitone_snap(true);
    add_knob(osc2, Synth::ParamId::CFIN, "FIN").set_semitone_snap(true);
    add_knob(osc2, Synth::ParamId::CPRD, "PRD");
    add_knob(osc2, Synth::ParamId::CPRT, "PRT");
    add_knob(osc2, Synth::ParamId::CN,   "NOISE");
    add_knob(osc2, Synth::ParamId::CFLD, "FOLD");
    add_knob(osc2, Synth::ParamId::CDL,   "DIST");
    /* Carrier distortion type: shown as a knob stepping through the types (by
     * name), sitting next to the DIST (level) knob. */
    add_knob(osc2, Synth::ParamId::CDTYP, "TYPE")
        .set_discrete_labels(distortion_type_labels());
    osc2_filters = add_filters(
        { Synth::ParamId::CF1TYP, Synth::ParamId::CF1FRQ, Synth::ParamId::CF1Q, Synth::ParamId::CF1G },
        { Synth::ParamId::CF2TYP, Synth::ParamId::CF2FRQ, Synth::ParamId::CF2Q, Synth::ParamId::CF2G },
        "3", "4"
    );
    osc2_type = new PerTypeEditor(
        bridge, manager, Synth::ParamId::CWFM, Synth::ParamId::CPW, Synth::ParamId::CC1
    );
    type_editors.add(osc2_type);
    addAndMakeVisible(osc2_type);

    /* Osc 2 (carrier) inaccuracy + instability dots. */
    osc2_inacc = add_dot(Synth::ParamId::COIA, "Inaccuracy", "Oscillator inaccuracy");
    osc2_instab = add_dot(Synth::ParamId::COIS, "Instability", "Oscillator instability");

    /* Apply the initial page's visibility (synth children start visible). */
    if (active_page != Page::SYNTH) {
        set_synth_visible(false);
        effects_page.setVisible(true);
    }

    startTimerHz(30);
}


NewGui::~NewGui()
{
    stopTimer();
}


Knob& NewGui::add_knob(
        std::vector<Knob*>& column,
        Synth::ParamId const id,
        char const* const label
) {
    Knob* const knob = new Knob(bridge, id, label);
    knob->set_manager(&manager);
    /* Discrete (byte) params - e.g. the carrier distortion TYPE knob - only accept
     * macro / MIDI controllers in the engine; the LFO / envelope routes are no-ops
     * for them, so don't offer them. Continuous oscillator params keep CAP_ALL. */
    if (bridge.is_discrete(id)) {
        knob->set_mod_caps(Modulation::CAP_MACRO);
    }
    knobs.add(knob);
    column.push_back(knob);
    addAndMakeVisible(knob);

    return *knob;
}


Control* NewGui::add_dot(Synth::ParamId const id, char const* const title, char const* const tooltip)
{
    /* The dot renders no caption or value, but carries its title so the hover /
     * drag popover can show it above the value on two lines. */
    Control* const dot = new Control(
        bridge, id, juce::String(title), Control::Style::DOT, Control::Size::TINY
    );
    dot->set_value_display(Control::ValueDisplay::POPOVER);
    dot->setTooltip(tooltip);
    dots.add(dot);
    addAndMakeVisible(dot);

    return dot;
}


WaveformSelector* NewGui::add_wave(Synth::ParamId const id)
{
    WaveformSelector* const wave = new WaveformSelector(bridge, id);
    waves.add(wave);
    addAndMakeVisible(wave);

    return wave;
}


Selector* NewGui::add_selector(
        Synth::ParamId const id, juce::StringArray options, juce::String caption
) {
    Selector* const selector = new Selector(
        bridge, id, std::move(options), std::move(caption)
    );
    selectors.add(selector);
    addAndMakeVisible(selector);

    return selector;
}


FilterPanel* NewGui::add_filters(
        FilterPanel::Spec const& a,
        FilterPanel::Spec const& b,
        juce::String label_a,
        juce::String label_b
) {
    FilterPanel* const panel = new FilterPanel(
        bridge, manager, a, b, std::move(label_a), std::move(label_b)
    );
    filters.add(panel);
    addAndMakeVisible(panel);

    return panel;
}


void NewGui::timerCallback()
{
    /* A pending RANDOMIZE: once the audio thread has applied it (the macro
     * fingerprint changes) - or a ~1s safety timeout elapses - relocate its use
     * of macros 1-8 and write the saved performance macros back. */
    if (restore_macros_ticks > 0) {
        --restore_macros_ticks;

        bool const drained =
            std::fabs(macro_signature() - randomize_signature) > 1.0e-9;

        if (drained || restore_macros_ticks == 0) {
            restore_macros_ticks = 0;
            restore_performance_macros();
        }
    }

    /* Keep grouped duplicates in sync before regrouping, so a shape edit
     * doesn't momentarily split a group. */
    for (ModulatorCard* const card : cards) {
        card->propagate();
    }

    manager.rescan();

    std::string sig;
    for (ModulationManager::Group const& g : manager.groups()) {
        if (g.type == Modulation::MACRO) {
            continue;
        }
        sig += std::to_string((int)g.type) + ":" + std::to_string(g.rep) + ":"
             + std::to_string(g.members.size()) + ":"
             + std::to_string(g.destinations.size()) + ";";
    }

    if (sig != card_sig) {
        card_sig = sig;
        rebuild_cards();
        repaint(mod_bounds);
    }

    out_knob->refresh();

    for (Knob* const knob : knobs) {
        knob->refresh();
    }
    for (Control* const dot : dots) {
        dot->refresh();
    }
    for (WaveformSelector* const wave : waves) {
        wave->refresh();
    }
    for (Selector* const selector : selectors) {
        selector->refresh();
    }
    for (FilterPanel* const filter : filters) {
        filter->refresh();
    }
    for (PerTypeEditor* const editor : type_editors) {
        editor->refresh();
    }
    for (ModulatorCard* const card : cards) {
        card->refresh();
    }

    macro_strip.refresh();

    if (active_page == Page::EFFECTS) {
        effects_page.refresh();
    }
}


void NewGui::set_synth_visible(bool const visible)
{
    /* The macro strip stays visible on the EFFECTS page too, since effect knobs
     * are macro-modulation destinations. */
    mod_viewport.setVisible(visible);

    for (Knob* const knob : knobs) {
        knob->setVisible(visible);
    }
    for (Control* const dot : dots) {
        dot->setVisible(visible);
    }
    for (WaveformSelector* const wave : waves) {
        wave->setVisible(visible);
    }
    for (Selector* const selector : selectors) {
        selector->setVisible(visible);
    }
    for (FilterPanel* const filter : filters) {
        filter->setVisible(visible);
    }
    for (PerTypeEditor* const editor : type_editors) {
        editor->setVisible(visible);
    }
}


void NewGui::set_page(Page const page)
{
    if (page == active_page) {
        return;
    }

    active_page = page;

    bool const synth = (page == Page::SYNTH);
    bool const effects = (page == Page::EFFECTS);
    bool const matrix = (page == Page::MATRIX);

    set_synth_visible(synth);
    effects_page.setVisible(effects);

    /* The macro strip stays on the SYNTH and EFFECTS pages, but the MATRIX page
     * hands the whole body to the embedded legacy GUI. */
    macro_strip.setVisible(!matrix);

    if (effects) {
        effects_page.refresh();
    }

    /* Let the host editor show / hide the embedded legacy GUI in the body. */
    if (on_matrix) {
        on_matrix(matrix);
    }

    repaint();
}


void NewGui::mouseUp(juce::MouseEvent const& event)
{
    juce::Point<int> const p = event.getPosition();

    if (tab_synth_bounds.contains(p)) {
        set_page(Page::SYNTH);
    } else if (tab_effects_bounds.contains(p)) {
        set_page(Page::EFFECTS);
    } else if (tab_matrix_bounds.contains(p)) {
        set_page(Page::MATRIX);
    }
}


void NewGui::init_patch()
{
    /* 1. Keep the performance macros' MIDI-CC inputs across the reset. */
    Synth::ControllerId saved[MacroStrip::COUNT];
    for (int m = 0; m < MacroStrip::COUNT; ++m) {
        saved[m] = bridge.controller(Modulation::macro_in(m + 1));
    }

    /* 2. Reset every parameter to its default and clear all controllers. */
    for (int p = 0; p < (int)Synth::ParamId::PARAM_ID_COUNT; ++p) {
        Synth::ParamId const id = (Synth::ParamId)p;
        bridge.assign_controller(id, Synth::ControllerId::NONE);
        bridge.set_ratio(id, bridge.get_default_ratio(id));
    }

    /* 3. Restore the saved macro CC inputs. */
    for (int m = 0; m < MacroStrip::COUNT; ++m) {
        if (saved[m] != Synth::ControllerId::NONE) {
            bridge.assign_controller(Modulation::macro_in(m + 1), saved[m]);
        }
    }

    /* 4. Two explicit envelope groups (AMP = env 1+2, filter = env 3+4). */
    manager.reset();
    manager.reserve_group(Modulation::ENVELOPE, { 1, 2 });
    manager.reserve_group(Modulation::ENVELOPE, { 3, 4 });

    /* Uniform envelope times on every envelope: 0.001 s attack, 0 hold, 1 s
     * decay, 0.1 s release. Only the level shape (base / peak / sustain) differs
     * between the AMP and filter groups. */
    auto setup_env = [this](
            int const slot, double const base, double const peak, double const sus) {
        bridge.set_ratio(Modulation::env_ini(slot), base);
        bridge.set_ratio(Modulation::env_fin(slot), base);
        bridge.set_ratio(Modulation::env_pk(slot), peak);
        bridge.set_ratio(Modulation::env_sus(slot), sus);
        bridge.set_ratio(Modulation::env_atk(slot), bridge.ratio_for_display(Modulation::env_atk(slot), 0.001));
        bridge.set_ratio(Modulation::env_hld(slot), 0.0);
        bridge.set_ratio(Modulation::env_dec(slot), bridge.ratio_for_display(Modulation::env_dec(slot), 1.0));
        bridge.set_ratio(Modulation::env_rel(slot), bridge.ratio_for_display(Modulation::env_rel(slot), 0.1));
    };

    /* 5a. AMP: level 0, full-range modulation; 100% sustain (env 1 -> Osc 1 AMP,
     * env 2 -> Osc 2 AMP). */
    setup_env(1, 0.0, 1.0, 1.0);
    setup_env(2, 0.0, 1.0, 1.0);
    bridge.assign_controller(Synth::ParamId::MAMP, Modulation::controller_id(Modulation::ENVELOPE, 1));
    bridge.assign_controller(Synth::ParamId::CAMP, Modulation::controller_id(Modulation::ENVELOPE, 2));

    /* 5b. Filter cutoff (Filter 1 = MF1, Filter 3 = CF1): 1 kHz base sweeping up
     * to 20 kHz at the envelope peak; 50% sustain. */
    setup_env(
        3,
        bridge.ratio_for_display(Synth::ParamId::MF1FRQ, 1000.0),
        bridge.ratio_for_display(Synth::ParamId::MF1FRQ, 20000.0),
        0.5
    );
    setup_env(
        4,
        bridge.ratio_for_display(Synth::ParamId::CF1FRQ, 1000.0),
        bridge.ratio_for_display(Synth::ParamId::CF1FRQ, 20000.0),
        0.5
    );
    bridge.assign_controller(Synth::ParamId::MF1FRQ, Modulation::controller_id(Modulation::ENVELOPE, 3));
    bridge.assign_controller(Synth::ParamId::CF1FRQ, Modulation::controller_id(Modulation::ENVELOPE, 4));

    /* 6. Guarantee one readily-automatable performance macro: if the user never
     * configured M1's input, default it to the modulation wheel with a full
     * 0..100% range, so there is always a mod-wheel macro to automate. */
    if (bridge.controller(Modulation::macro_in(1)) == Synth::ControllerId::NONE) {
        bridge.assign_controller(
            Modulation::macro_in(1), Synth::ControllerId::MODULATION_WHEEL
        );
        bridge.set_ratio(Modulation::macro_min(1), 0.0);
        bridge.set_ratio(Modulation::macro_max(1), 1.0);
    }
}


void NewGui::open_preset()
{
    file_chooser = std::make_unique<juce::FileChooser>(
        "Import patch", juce::File(), "*.js80p"
    );

    NewGui* const self = this;

    file_chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [self](juce::FileChooser const& fc) {
            juce::File const file = fc.getResult();

            if (file == juce::File()) {
                return;
            }

            juce::MemoryBlock block;

            if (!file.loadFileAsData(block)) {
                return;
            }

            size_t const size = juce::jmin(
                (size_t)Serializer::MAX_SIZE, block.getSize()
            );
            std::string const patch(static_cast<char const*>(block.getData()), size);

            /* GUI-thread import enqueues the changes to the audio thread; the
             * 30 Hz refresh then picks up the new values and assignments. */
            Serializer::import_patch_in_gui_thread(self->bridge.get_synth(), patch);
        }
    );
}


void NewGui::save_preset()
{
    file_chooser = std::make_unique<juce::FileChooser>(
        "Export patch", juce::File(), "*.js80p"
    );

    NewGui* const self = this;

    file_chooser->launchAsync(
        juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [self](juce::FileChooser const& fc) {
            juce::File file = fc.getResult();

            if (file == juce::File()) {
                return;
            }

            if (file.getFileExtension().isEmpty()) {
                file = file.withFileExtension("js80p");
            }

            std::string const patch = Serializer::serialize(self->bridge.get_synth());
            file.replaceWithText(
                juce::String::fromUTF8(patch.data(), (int)patch.size())
            );
        }
    );
}


double NewGui::macro_signature() const
{
    /* A cheap fingerprint of the 8 performance macros (input + range), used to
     * notice when the audio-thread RANDOMIZE has actually landed. */
    double s = 0.0;

    for (int m = 1; m <= MacroStrip::COUNT; ++m) {
        s += bridge.get_ratio(Modulation::macro_min(m)) * 7.0;
        s += bridge.get_ratio(Modulation::macro_max(m)) * 13.0;
        s += (double)(int)bridge.controller(Modulation::macro_in(m));
    }

    return s;
}


void NewGui::randomize_preset()
{
    /* Snapshot the 8 performance macros' input + range before the random patch
     * generator (which consumes macro slots from 1 upward) overwrites them. */
    for (int m = 0; m != MacroStrip::COUNT; ++m) {
        saved_macros[m].input = bridge.controller(Modulation::macro_in(m + 1));
        saved_macros[m].min = bridge.get_ratio(Modulation::macro_min(m + 1));
        saved_macros[m].max = bridge.get_ratio(Modulation::macro_max(m + 1));
    }

    randomize_signature = macro_signature();

    bridge.get_synth().push_message(
        Synth::MessageType::RANDOMIZE, Synth::ParamId::INVALID_PARAM_ID, 0.0, 0
    );

    /* RANDOMIZE is processed on the audio thread; defer the macro restore until
     * it has drained so we can see (and relocate) the params it wired to macros
     * 1-8. timerCallback() polls macro_signature() and, once it changes (or a ~1s
     * timeout elapses), calls restore_performance_macros(). */
    restore_macros_ticks = 30;
}


void NewGui::restore_performance_macros()
{
    using P = Synth::ParamId;

    int const N = MacroStrip::COUNT;   /* 8 performance macros */

    /* 1. Scan every parameter's controller once. Record which macro slots are
     *    referenced anywhere (so relocation can pick genuinely free pool slots),
     *    and every param the random patch wired to a performance macro (1-8). */
    std::set<int> used_macro;
    std::vector<std::pair<P, int>> refs_to_perf;   /* (param, perf slot 1..N) */

    for (int p = 0; p != (int)P::PARAM_ID_COUNT; ++p) {
        P const id = (P)p;
        Modulation::Type type;
        int slot;

        if (Modulation::decode(bridge.controller(id), type, slot)
                && type == Modulation::MACRO) {
            used_macro.insert(slot);

            if (slot >= 1 && slot <= N) {
                refs_to_perf.push_back({ id, slot });
            }
        }
    }

    /* Copy a macro's full definition (input controller + the 7-param block +
     * distortion curve) from one slot to another. */
    auto copy_macro = [this](int const from, int const to) {
        int const bf = (int)Modulation::macro_in(from) - 1;   /* M?MID, block start */
        int const bt = (int)Modulation::macro_in(to) - 1;

        for (int k = 0; k != 7; ++k) {
            P const src = (P)(bf + k);
            P const dst = (P)(bt + k);

            if (k == 1) {   /* the input is a controller assignment, not a ratio */
                bridge.assign_controller(dst, bridge.controller(src));
            } else {
                bridge.set_ratio(dst, bridge.get_ratio(src));
            }
        }

        bridge.set_ratio(Modulation::macro_dcv(to), bridge.get_ratio(Modulation::macro_dcv(from)));
    };

    auto alloc_free = [&used_macro]() -> int {
        for (int s = Modulation::MACRO_POOL_FIRST; s <= Modulation::MACRO_POOL_LAST; ++s) {
            if (used_macro.count(s) == 0) {
                used_macro.insert(s);
                return s;
            }
        }
        return 0;   /* pool exhausted */
    };

    /* 2. Relocate the random patch's use of macros 1-8 onto free pool slots:
     *    copy each used performance macro's random definition to a fresh slot and
     *    repoint every referencing param there. Params that shared a performance
     *    macro move together to the same relocated slot. */
    std::map<int, int> relocated;   /* perf slot -> new pool slot */

    for (std::pair<P, int> const& ref : refs_to_perf) {
        int const perf = ref.second;

        if (relocated.count(perf) == 0) {
            int const dst = alloc_free();

            if (dst == 0) {
                break;   /* no free pool slots; leave the rest on 1-8 */
            }

            copy_macro(perf, dst);
            relocated[perf] = dst;
        }

        bridge.assign_controller(
            ref.first, Modulation::controller_id(Modulation::MACRO, relocated[perf])
        );
    }

    /* 3. Reset macros 1-8 to a clean default, then write back the captured input
     *    + range, so the performance macros survive the randomize intact. */
    for (int m = 0; m != N; ++m) {
        int const slot = m + 1;
        int const base = (int)Modulation::macro_in(slot) - 1;

        for (int k = 0; k != 7; ++k) {
            P const id = (P)(base + k);

            if (k == 1) {
                bridge.assign_controller(id, Synth::ControllerId::NONE);
            } else {
                bridge.set_ratio(id, bridge.get_default_ratio(id));
            }
        }

        bridge.set_ratio(
            Modulation::macro_dcv(slot), bridge.get_default_ratio(Modulation::macro_dcv(slot))
        );

        if (saved_macros[m].input != Synth::ControllerId::NONE) {
            bridge.assign_controller(Modulation::macro_in(slot), saved_macros[m].input);
        }

        bridge.set_ratio(Modulation::macro_min(slot), saved_macros[m].min);
        bridge.set_ratio(Modulation::macro_max(slot), saved_macros[m].max);
    }
}


void NewGui::rebuild_cards()
{
    cards.clear();

    for (ModulationManager::Group const& g : manager.groups()) {
        if (g.type == Modulation::MACRO) {
            continue;
        }

        ModulatorCard* const card = new ModulatorCard(bridge, manager, g);
        cards.add(card);
        mod_content.addAndMakeVisible(card);
    }

    layout_cards();
}


void NewGui::layout_cards()
{
    int const w = mod_viewport.getWidth() - mod_viewport.getScrollBarThickness();
    int y = 0;

    for (ModulatorCard* const card : cards) {
        int const h = card->preferred_height();
        card->setBounds(0, y, juce::jmax(0, w), h);
        y += h + 6;
    }

    mod_content.setSize(juce::jmax(0, w), y + 2);
}


void NewGui::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    header_bounds = area.removeFromTop(46);

    /* Header action buttons, left-aligned just after the JS80P title, laid out
     * left to right. INIT is a slightly taller button; the icon buttons share its
     * height for a larger click target (their glyphs stay the same size, centred). */
    {
        int const bh = 22;   /* ~110% of the original 20 (nearest even) for readability */
        int const by = header_bounds.getCentreY() - bh / 2;
        int bx = header_bounds.getX() + 96;

        for (int i = 0; i != header_buttons.size(); ++i) {
            MiniButton* const button = header_buttons[i];
            int const bw = button->preferred_width();
            button->setBounds(bx, by, bw, bh);
            bx += bw + 4;
            /* Extra breathing room after the INIT button (first), separating it
             * from the OPEN / RND / SAVE preset icons. */
            if (i == 0) {
                bx += 4;
            }
        }
    }

    /* Global OUT knob pinned to the header's right edge, its (empty) modulation
     * box padded 8px from the plugin's right side. A small effects-sized dial
     * with its caption to the left; the box is vertically centred on the dial. */
    {
        int const out_sz = 34;
        int const badge_pad = 8;   /* gap from the plugin's right edge to the mod box */
        /* The centred mod box sits ~15px right of the dial's bounds (dial right
         * edge + 4px gap + 16px box, less the 5px the circle is inset). */
        int const badge_overhang = 15;
        int const kx = header_bounds.getRight() - badge_pad - badge_overhang - out_sz;
        int const ky = header_bounds.getCentreY() - out_sz / 2;
        out_knob->setBounds(kx, ky, out_sz, out_sz);
        out_label_bounds =
            juce::Rectangle<int>(kx - 4 - 34, header_bounds.getY(), 34, header_bounds.getHeight());
    }

    /* SYNTH / EFFECTS / MATRIX tabs, centred in the header row. */
    {
        int const tab_w = 92;
        int const tab_gap = 4;
        int const total = 3 * tab_w + 2 * tab_gap;
        int const x0 = header_bounds.getCentreX() - total / 2;
        /* Tabs span the full header height (no vertical padding) so the active
         * tab's edge glow is measured against the whole header. */
        int const ty = header_bounds.getY();
        int const tab_h = header_bounds.getHeight();
        tab_synth_bounds = juce::Rectangle<int>(x0, ty, tab_w, tab_h);
        tab_effects_bounds = juce::Rectangle<int>(x0 + tab_w + tab_gap, ty, tab_w, tab_h);
        tab_matrix_bounds = juce::Rectangle<int>(x0 + 2 * (tab_w + tab_gap), ty, tab_w, tab_h);
    }

    /* The macro strip is always visible (both pages). */
    macro_strip.setBounds(area.removeFromTop(66));
    area.reduce(5, 5);

    /* The EFFECTS page fills the body below the macro strip. */
    body_bounds = area;
    effects_page.setBounds(body_bounds);

    int const gap = 5;
    int const filter_height = 102;   /* -14: filter knobs no longer reserve a bottom value strip */

    int const mod_width = juce::jmax(220, area.getWidth() / 3);
    mod_bounds = area.removeFromRight(mod_width);
    area.removeFromRight(gap);

    int const mix_width = 84;
    int const osc_width = (area.getWidth() - mix_width - 2 * gap) / 2;

    juce::Rectangle<int> osc1_col = area.removeFromLeft(osc_width);
    area.removeFromLeft(gap);
    mix_bounds = area.removeFromLeft(mix_width);
    area.removeFromLeft(gap);
    juce::Rectangle<int> osc2_col = area;

    /* Each oscillator column is one panel that contains its filters at the
     * bottom. */
    osc1_panel_bounds = osc1_col;
    osc2_panel_bounds = osc2_col;

    juce::Rectangle<int> const osc1_filter_bounds = osc1_col.removeFromBottom(filter_height);
    osc1_col.removeFromBottom(gap);
    osc1_bounds = osc1_col;

    juce::Rectangle<int> const osc2_filter_bounds = osc2_col.removeFromBottom(filter_height);
    osc2_col.removeFromBottom(gap);
    osc2_bounds = osc2_col;

    osc1_filters->setBounds(osc1_filter_bounds);
    osc2_filters->setBounds(osc2_filter_bounds);

    lay_out_osc(osc1_bounds, osc1_wave, osc1, osc1_type);
    lay_out_mix(mix_bounds, mode_selector, tuning_selector, poly_selector, mix);
    lay_out_osc(osc2_bounds, osc2_wave, osc2, osc2_type);

    /* Two tiny pie dots right-aligned in each oscillator's title row,
     * vertically centred on the 13px title text (see draw_section_title). */
    auto place_dots = [](
            juce::Rectangle<int> const& panel, Control* const a, Control* const b) {
        int const sz = 16;
        int const gap = 5;
        int const right = panel.getRight() - 10;
        int const y = panel.getY() + 19 - sz / 2;   /* title text centre */
        b->setBounds(right - sz, y, sz, sz);                 /* instability (rightmost) */
        a->setBounds(right - 2 * sz - gap, y, sz, sz);       /* inaccuracy */
    };
    place_dots(osc1_panel_bounds, osc1_inacc, osc1_instab);
    place_dots(osc2_panel_bounds, osc2_inacc, osc2_instab);

    juce::Rectangle<int> mv = mod_bounds;
    mv.removeFromBottom(6);
    mod_viewport.setBounds(mv);    /* no title, no horizontal padding: cards align to the top */

    layout_cards();
}


void NewGui::lay_out_osc(
        juce::Rectangle<int> panel,
        WaveformSelector* wave,
        std::vector<Knob*>& main,
        PerTypeEditor* per_type
) {
    juce::Rectangle<int> inner = panel.reduced(10);
    inner.removeFromTop(26);   /* title + 8px padding below it */

    if (wave != nullptr) {
        wave->setBounds(inner.removeFromTop(40));
    }

    inner.removeFromTop(8);

    int const columns = 4;
    int const cell_w = inner.getWidth() / columns;
    int const cell_h = 58;   /* dial (40px) + caption strip; no bottom value strip */

    for (int i = 0; i != (int)main.size(); ++i) {
        main[(size_t)i]->setBounds(
            inner.getX() + (i % columns) * cell_w,
            inner.getY() + (i / columns) * cell_h,
            cell_w,
            cell_h
        );
    }

    int const main_rows = ((int)main.size() + columns - 1) / columns;
    inner.removeFromTop(main_rows * cell_h + 6);

    if (per_type != nullptr) {
        /* Cap the per-type editor so the harmonics/pulse-width area doesn't grow
         * unbounded on tall windows. */
        per_type->setBounds(inner.removeFromTop(juce::jmin(inner.getHeight(), 150)));
    }
}


void NewGui::lay_out_mix(
        juce::Rectangle<int> panel,
        Selector* mode,
        Selector* tuning,
        Selector* poly,
        std::vector<Knob*>& knobs_
) {
    juce::Rectangle<int> inner = panel.reduced(10);

    /* Selectors stack up from the bottom: MODE, then TUNING, then POLY, so from
     * the top the column reads MIX / PM / FM / AM / POLY / TUNING / MODE. */
    if (mode != nullptr) {
        mode->setBounds(inner.removeFromBottom(40));
    }

    if (tuning != nullptr) {
        inner.removeFromBottom(8);
        tuning->setBounds(inner.removeFromBottom(40));
    }

    if (poly != nullptr) {
        inner.removeFromBottom(8);
        poly->setBounds(inner.removeFromBottom(40));
    }

    /* A little top padding so the MIX knob isn't cramped against the top. */
    inner.removeFromTop(10);

    int const cell_h = 58;   /* dial (40px) + caption strip; no bottom value strip */

    for (int i = 0; i != (int)knobs_.size(); ++i) {
        knobs_[(size_t)i]->setBounds(
            inner.getX(), inner.getY() + i * cell_h, inner.getWidth(), cell_h
        );
    }
}


void NewGui::draw_section_title(
        juce::Graphics& g,
        juce::Rectangle<int> const& r,
        char const* const title
) const {
    g.setColour(Theme::TEXT_DIM);
    g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f).withStyle("Bold")));
    g.drawText(
        title,
        r.reduced(14, 10).removeFromTop(18),
        juce::Justification::centredLeft,
        false
    );
}


void NewGui::draw_panel(
        juce::Graphics& g,
        juce::Rectangle<int> const& r,
        char const* const title
) const {
    g.setColour(Theme::PANEL);
    g.fillRoundedRectangle(r.toFloat(), Theme::RADIUS);
    g.setColour(Theme::EDGE);
    g.drawRoundedRectangle(r.toFloat().reduced(0.5f), Theme::RADIUS, 1.0f);

    draw_section_title(g, r, title);
}


void NewGui::paint_tabs(juce::Graphics& g)
{
    struct { juce::Rectangle<int> const& r; char const* label; bool on; } const tabs[3] = {
        { tab_synth_bounds, "SYNTH", active_page == Page::SYNTH },
        { tab_effects_bounds, "EFFECTS", active_page == Page::EFFECTS },
        { tab_matrix_bounds, "MATRIX", active_page == Page::MATRIX },
    };

    g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Bold")));

    /* Active tab: a 2px orange underline sitting on the top bar's baseline.
     * Inactive tabs: dim text, no underline, no background. */
    int const baseline = header_bounds.getBottom();

    for (auto const& tab : tabs) {
        /* Active tab: a soft radial bloom mirrored on the bottom AND top edges,
         * each anchored on its edge, brightest at the centre and falling off
         * outward to transparent. Squashed hard vertically so the circular
         * gradient reads as a thin, wide, tab-shaped bloom. Behind the label. */
        if (tab.on) {
            auto const rf = tab.r.toFloat();
            float const cx = rf.getCentreX();

            /* Draw one edge bloom anchored at anchor_y, filling toward fill_y
             * (kept inside the tab so it never leaks past the edge). */
            auto const bloom = [&](juce::Colour const color, float const radius, float const scale, float const anchor_y, float const fill_y) {
                juce::ColourGradient grad(
                    color, cx, anchor_y,
                    color.withAlpha(0.0f), cx + radius, anchor_y, true);

                juce::Graphics::ScopedSaveState const state(g);
                g.addTransform(juce::AffineTransform::scale(1.0f, scale, cx, anchor_y));
                g.setGradientFill(grad);
                g.fillRect(juce::Rectangle<float>::leftTopRightBottom(
                    rf.getX(), juce::jmin(anchor_y, fill_y),
                    rf.getRight(), juce::jmax(anchor_y, fill_y)));
            };

            /* rises from the baseline */
            bloom(Theme::ACCENT.withAlpha(0.25f), rf.getWidth() * 0.5f, 0.25f, rf.getBottom() + 2.0f, rf.getY());
            /* off-screen and only its fading tail shows — no clipped flat edge. */
            bloom(Theme::ACCENT.withAlpha(0.2f), rf.getWidth() * 0.55f, 0.25f, rf.getY() - 7.0f, rf.getBottom());
            // alternate shadow style:
            // bloom(Theme::SHADOW.withAlpha(0.55f), rf.getWidth() * 0.5f, 0.25f, rf.getBottom() - 2.0f, rf.getY());
            // bloom(Theme::SHADOW.withAlpha(0.55f), rf.getWidth() * 0.5f, 0.25f, rf.getY() - 2.0f, rf.getBottom());
        }

        g.setColour(tab.on ? Theme::TEXT : Theme::TEXT_DIM);
        g.drawText(tab.label, tab.r, juce::Justification::centred, false);

        if (tab.on) {
            g.setColour(Theme::ACCENT);
            g.fillRect(tab.r.getX(), baseline - 2, tab.r.getWidth(), 2);
        }
    }
}


void NewGui::paint(juce::Graphics& g)
{
    g.fillAll(Theme::BG);

    g.setColour(Theme::PANEL_2);
    g.fillRect(header_bounds);
    g.setColour(Theme::EDGE_SOFT);
    g.fillRect(header_bounds.getX(), header_bounds.getBottom() - 1, header_bounds.getWidth(), 1);

    g.setColour(Theme::TEXT);
    g.setFont(juce::Font(juce::FontOptions().withHeight(20.0f).withStyle("Bold")));
    g.drawText("JS80P", header_bounds.reduced(16, 0), juce::Justification::centredLeft, false);

    /* OUT caption to the left of the header output knob. */
    g.setColour(Theme::TEXT_DIM);
    g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f).withStyle("Bold")));
    g.drawText("OUT", out_label_bounds, juce::Justification::centredRight, false);

    paint_tabs(g);

    /* Only the SYNTH page draws the oscillator / mix / modulator panels; the
     * EFFECTS page draws its own children, and the MATRIX page hands the body to
     * the embedded legacy GUI (leaving the uncovered area as empty background). */
    if (active_page != Page::SYNTH) {
        return;
    }

    draw_panel(g, osc1_panel_bounds, "OSC 1  (modulator)");
    draw_panel(g, osc2_panel_bounds, "OSC 2  (carrier)");
    /* MIX and MODULATORS are title-less, transparent sections. */

    /* Signal-flow hint: two small right-pointing triangles in the mix column,
     * one hugging OSC 1 (flow into the mix knobs) and one hugging OSC 2 (flow
     * out to the carrier). Aligned with the bottom of the PM knob's dial (its
     * circle, excluding the caption). */
    {
        float const h = 8.0f;
        float const w = h * 0.62f;
        float const cy = mix.size() < 2
            ? (float)mix_bounds.getCentreY()
            : (float)(mix[1]->getY() + mix[1]->control_bounds().getBottom());
        auto arrow = [&g, h, w, cy](float const cx) {
            juce::Path tri;
            tri.startNewSubPath(cx - w * 0.5f, cy - h * 0.5f);
            tri.lineTo(cx + w * 0.5f, cy);
            tri.lineTo(cx - w * 0.5f, cy + h * 0.5f);
            tri.closeSubPath();
            g.fillPath(tri);
        };
        g.setColour(Theme::TEXT_DIM.withAlpha(0.5f));
        arrow((float)mix_bounds.getX() + w * 0.5f + 2.0f);
        arrow((float)mix_bounds.getRight() - w * 0.5f - 2.0f);
    }

    if (cards.isEmpty()) {
        g.setColour(Theme::TEXT_FAINT);
        g.setFont(11.0f);
        g.drawText(
            "right-click any knob to assign an envelope or LFO",
            mod_bounds.reduced(16, 0),
            juce::Justification::centred,
            false
        );
    }
}

}

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

#include "ui/effects_page.hpp"

#include "ui/modulation.hpp"
#include "ui/param_labels.hpp"
#include "ui/theme.hpp"


namespace JS80P
{

/* Two control sizes. Large matches the synth page's 72px cell. Medium is the
 * same knob at ~4/5 the circle size, vertically centred so its circle lines up
 * with the large row. */
static constexpr int LARGE_W = 56;
static constexpr int KNOB_H  = 58;   /* -14: no reserved bottom value strip */
static constexpr int MED_W   = 48;
static constexpr int MED_H   = 51;   /* -14: matches KNOB_H, medium circle stays aligned */
static constexpr int TITLE_H = 22;
static constexpr int PANEL_PAD = 8;
static constexpr int PANEL_GAP = 6;
/* Gap between adjacent knob cells so a knob's modulation badge (top-right of the
 * dial) has room and does not overlap the next cell. */
static constexpr int CELL_GAP = 8;
/* Extra right padding for panels whose last (right-most) control is a modulation
 * destination: keeps its mod badge clear of the edge and gives the panel some
 * breathing room now that the row 1 panels no longer need every spare pixel. */
static constexpr int MOD_RIGHT_PAD = 14;


EffectsPage::EffectsPage(ParamBridge& bridge, ModulationManager& manager)
    : bridge(bridge),
    manager(manager),
    content(*this)
{
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    /* The effect chain, grouped the way the original GUI groups it. Each panel
     * is one left-to-right row of controls in two sizes: large primary knobs
     * and ~4/5-size medium knobs for the related secondary controls, mixed in
     * whatever order reads best. "TYPE" / mode params stay as knobs (they show
     * the option name); the old logarithmic-scale toggles are omitted. */
    using P = Synth::ParamId;

    /* Row 0 - top compact strip: input, the gain stages, the two distortions
     * and the two filters. (The global output volume now lives in the header.) */
    add_large(begin_panel("INPUT", 0), { { P::INVOL, "VOL" } });
    add_large(begin_panel("VOL 1", 0), { { P::EV1V, "VOL" } });

    int const dist1 = begin_panel("DIST 1", 0);
    add_large(dist1, { { P::ED1L, "LEVEL" } });
    add_medium(dist1, { { P::ED1TYP, "TYPE" } });

    int const dist2 = begin_panel("DIST 2", 0);
    add_large(dist2, { { P::ED2L, "LEVEL" } });
    add_medium(dist2, { { P::ED2TYP, "TYPE" } });

    int const filter1 = begin_panel("FILTER 1", 0);
    add_large(filter1, { { P::EF1FRQ, "FREQ" } });
    add_medium(filter1, { { P::EF1TYP, "TYPE" }, { P::EF1Q, "Q" }, { P::EF1G, "GAIN" } });

    int const filter2 = begin_panel("FILTER 2", 0);
    add_large(filter2, { { P::EF2FRQ, "FREQ" } });
    add_medium(filter2, { { P::EF2TYP, "TYPE" }, { P::EF2Q, "Q" }, { P::EF2G, "GAIN" } });

    add_large(begin_panel("VOL 2", 0), { { P::EV2V, "VOL" } });

    /* Row 1 - CHORUS (left) and TAPE (right) share a single row. WIDTH is a
     * large knob kept in its natural place, right after the TYPE selector. */
    int const chorus = begin_panel("CHORUS", 1);
    add_mix(chorus, P::ECWET, P::ECDRY);   /* MIX pie in the panel header */
    add_medium(chorus, { { P::ECTYP, "TYPE" } });
    add_large(chorus, { { P::ECFRQ, "FREQ" }, { P::ECDPT, "DEPTH" } });
    add_medium(chorus, { { P::ECDEL, "DELAY" }, { P::ECFB, "FB" } });
    add_large(chorus, { { P::ECWID, "WIDTH" } });
    add_medium(chorus, {
        { P::ECDF, "DAMP F" }, { P::ECDG, "DAMP G" },
        { P::ECHPF, "HPF" }, { P::ECHPQ, "HP Q" }
    });
    /* Tempo-sync toggle for the LFO rate, centred over the FREQ knob. */
    add_button(chorus, P::ECSYN, "BPM", P::ECFRQ);

    /* TAPE: STOP is a large primary knob and sits last in the row. HISS moves to
     * the title bar as a standard pie (right-aligned, left of the PRE/POST FX
     * button). */
    int const tape = begin_panel("TAPE", 1);
    /* STOP is a large knob, whose mod badge overhangs its cell ~2px less than a
     * medium knob's; trim 2px off the default pad so its mod box clears the panel
     * edge by 12px, matching CHORUS (which ends on a medium HP Q knob). */
    panels[(size_t)tape].right_pad = MOD_RIGHT_PAD - 2;
    add_large(tape, { { P::ETWFA, "WOW" }, { P::ETSAT, "SAT" } });
    add_medium(tape, {
        { P::ETWFS, "SPEED" }, { P::ETCLR, "COLOR" },
        { P::ETSTR, "STEREO" }, { P::ETSTYP, "TYPE" }
    });
    add_large(tape, { { P::ETSTP, "STOP" } });
    add_header_knob(tape, P::ETHSS, "HISS");
    add_button(tape, P::ETEND, "PRE FX")
        ->set_option_labels({ "PRE FX", "POST FX" });

    /* Row 2 - ECHO. REV 1 / REV 2 / SC MODE move to title buttons. WIDTH is a
     * large knob kept right after DIST. */
    int const echo = begin_panel("ECHO", 2);
    add_mix(echo, P::EEWET, P::EEDRY);   /* MIX pie in the panel header */
    add_medium(echo, { { P::EEINV, "IN" } });
    add_large(echo, { { P::EEDEL, "DELAY" }, { P::EEFB, "FB" } });
    add_medium(echo, { { P::EEDST, "DIST" } });
    add_large(echo, { { P::EEWID, "WIDTH" } });
    add_medium(echo, {
        { P::EEDF, "DAMP F" }, { P::EEDG, "DAMP G" },
        { P::EEHPF, "HPF" }, { P::EEHPQ, "HP Q" },
        { P::EECTH, "SC TH" }, { P::EECAT, "SC AT" },
        { P::EECRL, "SC RL" }, { P::EECR, "SC R" }
    });
    /* Tempo-sync toggle for the DELAY time, centred over the DELAY knob. */
    add_button(echo, P::EESYN, "BPM", P::EEDEL);
    /* REV 1 sits over the FB knob, with REV 2 laid out just to its right. */
    add_button(echo, P::EER1, "REV 1", P::EEFB);
    add_button_trailing(echo, P::EER2, "REV 2");
    /* Side-chain mode toggle, centred over the side-chain group's lead knob. */
    add_button(echo, P::EECM, "SC", P::EECTH)->set_option_labels({ "COMP", "EXPD" });

    /* Row 3 - REVERB. WIDTH is a large knob kept right after DIST. */
    int const reverb = begin_panel("REVERB", 3);
    add_mix(reverb, P::ERWET, P::ERDRY);   /* MIX pie in the panel header */
    add_medium(reverb, { { P::ERTYP, "TYPE" } });
    add_large(reverb, { { P::ERRS, "SIZE" }, { P::ERRR, "REFL" } });
    add_medium(reverb, { { P::ERDST, "DIST" } });
    add_large(reverb, { { P::ERWID, "WIDTH" } });
    add_medium(reverb, {
        { P::ERDF, "DAMP F" }, { P::ERDG, "DAMP G" },
        { P::ERHPF, "HPF" }, { P::ERHPQ, "HP Q" },
        { P::ERCTH, "SC TH" }, { P::ERCAT, "SC AT" },
        { P::ERCRL, "SC RL" }, { P::ERCR, "SC R" }
    });
    /* Side-chain mode toggle, centred over the side-chain group's lead knob,
     * matching the ECHO panel. */
    add_button(reverb, P::ERCM, "SC", P::ERCTH)->set_option_labels({ "COMP", "EXPD" });
}


int EffectsPage::begin_panel(juce::String title, int const row)
{
    Panel panel;
    panel.title = std::move(title);
    panel.row = row;
    panels.push_back(std::move(panel));
    return (int)panels.size() - 1;
}


/* Modulation kinds each continuous effect param accepts, matching the original
 * GUI's per-knob controller choices (verified against the engine): effects sit on
 * the single global bus, so they never take a per-voice envelope. Most take LFO +
 * macro; the tape transport/colour trims and the compressor side-chain params take
 * macro/MIDI only (no LFO); a few tape trims take no modulation at all. */
static int fx_mod_caps(Synth::ParamId const id)
{
    using P = Synth::ParamId;

    switch (id) {
        /* SCREW trims in the original GUI (controller choices = 0). */
        case P::ETWFS: case P::ETSTR: case P::ETHSS:
            return 0;

        /* Macro / MIDI only (no LFO): tape wow-amount / colour / stop and both
         * compressors' side-chain threshold / attack / release / ratio. */
        case P::ETWFA: case P::ETCLR: case P::ETSTP:
        case P::EECTH: case P::EECAT: case P::EECRL: case P::EECR:
        case P::ERCTH: case P::ERCAT: case P::ERCRL: case P::ERCR:
            return Modulation::CAP_MACRO;

        /* Every other continuous effect param: LFO + macro, no envelope. */
        default:
            return Modulation::CAP_LFO | Modulation::CAP_MACRO;
    }
}


Knob* EffectsPage::make_knob(KnobSpec const& spec, bool const medium)
{
    using P = Synth::ParamId;

    Knob* const knob = new Knob(bridge, spec.id, spec.label);

    /* Continuous effect knobs are modulation destinations (LFO / macro, never an
     * envelope on the global bus); the discrete "type" / "mode" selectors and a
     * few unmodulatable tape trims are not. */
    if (!bridge.is_discrete(spec.id)) {
        int const caps = fx_mod_caps(spec.id);

        if (caps != 0) {
            knob->set_manager(&manager);
            knob->set_mod_caps(caps);
        }
    }

    /* Frequency cutoffs (filters, damping, HPF): an exponential 20 Hz .. 20 kHz
     * sweep with 1.5 kHz at mid-travel, instead of the parameter's full native
     * range. */
    switch (spec.id) {
        case P::EF1FRQ: case P::EF2FRQ:
        case P::ECDF: case P::ECHPF:
        case P::EEDF: case P::EEHPF:
        case P::ERDF: case P::ERHPF:
            knob->set_freq_range(20.0, 20000.0, 1500.0);
            break;

        /* Chorus LFO rate: exponential, 3 Hz at mid-travel. */
        case P::ECFRQ:
            knob->set_center_value(3.0);
            break;

        default:
            break;
    }

    /* Show names instead of indices for the type / mode selectors. */
    if (spec.id == P::EF1TYP || spec.id == P::EF2TYP) {
        knob->set_discrete_labels(filter_type_labels());
    } else if (spec.id == P::ED1TYP || spec.id == P::ED2TYP
            || spec.id == P::ETSTYP) {
        knob->set_discrete_labels(distortion_type_labels());
    } else if (spec.id == P::ERCM) {
        knob->set_discrete_labels({ "COMP", "EXPD" });
    }

    if (medium) {
        knob->set_compact(true);
    }

    knobs.add(knob);
    content.addAndMakeVisible(knob);
    return knob;
}


void EffectsPage::add_large(
        int const panel, std::initializer_list<KnobSpec> const specs
) {
    for (KnobSpec const& spec : specs) {
        Cell cell;
        cell.knob = make_knob(spec, false);
        cell.id = spec.id;
        panels[(size_t)panel].cells.push_back(cell);
    }
}


void EffectsPage::add_medium(
        int const panel, std::initializer_list<KnobSpec> const specs
) {
    for (KnobSpec const& spec : specs) {
        Cell cell;
        cell.knob = make_knob(spec, true);
        cell.medium = true;
        cell.id = spec.id;
        panels[(size_t)panel].cells.push_back(cell);
    }
}


void EffectsPage::add_mix(
        int const panel, Synth::ParamId const wet, Synth::ParamId const dry
) {
    /* One rotary folding an effect's WET and DRY volumes into a single MIX:
     * mid-travel = both full; clockwise holds WET at 100% and fades DRY; counter-
     * clockwise holds DRY at 100% and fades WET. Driven through value hooks so it
     * inherits the standard drag / reset / typed-entry behaviour. */
    ParamBridge* const bp = &bridge;

    auto const read_mix = [bp, wet, dry]() -> double {
        double const w = bp->get_ratio(wet);
        double const d = bp->get_ratio(dry);
        return w >= d ? juce::jlimit(0.5, 1.0, 1.0 - d * 0.5)
                      : juce::jlimit(0.0, 0.5, w * 0.5);
    };
    auto const write_mix = [bp, wet, dry](double const m) {
        bp->set_ratio(wet, juce::jmin(1.0, 2.0 * m));
        bp->set_ratio(dry, juce::jmin(1.0, 2.0 * (1.0 - m)));
    };
    auto const format_mix = [](double const m) {
        int const wet_pct = (int)std::lround(juce::jmin(1.0, 2.0 * m) * 100.0);
        int const dry_pct = (int)std::lround(juce::jmin(1.0, 2.0 * (1.0 - m)) * 100.0);
        return juce::String(wet_pct) + "/" + juce::String(dry_pct);
    };

    /* The MIX sits in the panel's title bar (right-aligned), drawn as the small
     * pie-style control used elsewhere in panel headers, captioned "MIX". */
    Control* const mix = new Control(bridge, wet, "MIX", Control::Style::DOT, Control::Size::TINY);
    mix->set_value_hooks(read_mix, write_mix, format_mix);
    mix->set_hook_default(0.5);
    mix->set_label_placement(Control::LabelPos::LEFT);
    mix->set_value_display(Control::ValueDisplay::POPOVER);
    /* A macro-modulation destination on the effect's WET amount, with the empty
     * modulation box always shown (like the other header knobs). A modulator
     * crossfades: it sweeps WET up while DRY is driven inversely (down). */
    mix->set_manager(&manager);
    mix->set_mod_caps(Modulation::CAP_MACRO);
    mix->set_source_placeholder(true);
    mix->set_inverse_mirrors({ dry });
    mix_knobs.add(mix);
    content.addAndMakeVisible(mix);
    panels[(size_t)panel].header_mix = mix;
}


Control* EffectsPage::add_header_knob(
        int const panel, Synth::ParamId const id, juce::String label
) {
    /* A single-parameter version of the header MIX pie: the same small LEFT-
     * captioned DOT control, but bound directly to one param (e.g. TAPE HISS,
     * which is an unmodulatable trim, so no modulation manager / badge). */
    Control* const knob = new Control(
        bridge, id, std::move(label), Control::Style::DOT, Control::Size::TINY
    );
    knob->set_label_placement(Control::LabelPos::LEFT);
    knob->set_value_display(Control::ValueDisplay::POPOVER);

    mix_knobs.add(knob);
    content.addAndMakeVisible(knob);
    panels[(size_t)panel].header_knob = knob;
    return knob;
}


MiniButton* EffectsPage::add_button(
        int const panel, Synth::ParamId const id, juce::String label,
        Synth::ParamId const anchor
) {
    MiniButton* const button = new MiniButton(bridge, id, std::move(label));
    buttons.add(button);
    content.addAndMakeVisible(button);

    if (anchor != Synth::ParamId::PARAM_ID_COUNT) {
        panels[(size_t)panel].anchored.push_back({ button, anchor, {} });
    } else {
        panels[(size_t)panel].buttons.push_back(button);
    }

    return button;
}


MiniButton* EffectsPage::add_button_trailing(
        int const panel, Synth::ParamId const id, juce::String label
) {
    MiniButton* const button = new MiniButton(bridge, id, std::move(label));
    buttons.add(button);
    content.addAndMakeVisible(button);
    panels[(size_t)panel].anchored.back().trailing.push_back(button);
    return button;
}


void EffectsPage::resized()
{
    viewport.setBounds(getLocalBounds());
    layout();
}


bool EffectsPage::last_cell_modulatable(Panel const& panel) const
{
    if (panel.cells.empty()) {
        return false;
    }

    Cell const& last = panel.cells.back();

    if (last.mix != nullptr) {
        return true;   /* the WET/DRY MIX cell is a macro destination */
    }

    return !bridge.is_discrete(last.id) && fx_mod_caps(last.id) != 0;
}


juce::Point<int> EffectsPage::panel_size(Panel const& p) const
{
    int inner = 0;
    for (Cell const& c : p.cells) {
        inner += c.medium ? MED_W : LARGE_W;
    }
    if (!p.cells.empty()) {
        inner += CELL_GAP * ((int)p.cells.size() - 1);
    }

    /* Give panels whose last control is modulatable extra right padding so that
     * control's mod badge does not overhang the panel edge, and so the panel is
     * not cramped. (The CHORUS / TAPE panels flex to fill row 1, so this base pad
     * is subsumed by their leftover share.) */
    int const right_pad = p.right_pad >= 0
        ? p.right_pad
        : (last_cell_modulatable(p) ? MOD_RIGHT_PAD : 0);

    return juce::Point<int>(
        inner + 2 * PANEL_PAD + right_pad, TITLE_H + KNOB_H + PANEL_PAD
    );
}


void EffectsPage::place_panel(Panel& panel)
{
    int const inner_y = panel.bounds.getY() + TITLE_H;
    int const med_y = inner_y + (KNOB_H - MED_H) / 2;
    int x = panel.bounds.getX() + PANEL_PAD;

    /* One left-to-right row. Large cells (primary knobs, the MIX cell) are full
     * height; medium cells are shorter and vertically centred so their circles
     * line up with the large row. */
    for (Cell const& cell : panel.cells) {
        if (cell.medium) {
            cell.knob->setBounds(x, med_y, MED_W, MED_H);
            x += MED_W + CELL_GAP;
        } else if (cell.mix != nullptr) {
            cell.mix->setBounds(x, inner_y, LARGE_W, KNOB_H);
            x += LARGE_W + CELL_GAP;
        } else {
            cell.knob->setBounds(x, inner_y, LARGE_W, KNOB_H);
            x += LARGE_W + CELL_GAP;
        }
    }

    /* Title-bar buttons, vertically centred on the title text. The title is
     * drawn in a 16px band starting PANEL pad-6 below the panel top (see
     * paint_content), so its centre sits at getY()+14. */
    int const bh = 14;
    int const by = panel.bounds.getY() + 14 - bh / 2;

    /* Anchored title buttons: horizontally centred over the knob they affect
     * (e.g. the CHORUS/ECHO BPM toggle over FREQ / DELAY). */
    for (AnchoredButton const& ab : panel.anchored) {
        int const bw = ab.button->preferred_width();
        int cx = panel.bounds.getX() + PANEL_PAD + LARGE_W / 2;   /* fallback */

        for (Cell const& cell : panel.cells) {
            if (cell.knob != nullptr && cell.id == ab.anchor) {
                cx = cell.knob->getBounds().getCentreX();
                break;
            }
        }

        ab.button->setBounds(cx - bw / 2, by, bw, bh);

        /* Any trailing buttons flow to the right of the anchored one. */
        int tx = cx - bw / 2 + bw + 4;
        for (MiniButton* const t : ab.trailing) {
            int const tw = t->preferred_width();
            t->setBounds(tx, by, tw, bh);
            tx += tw + 4;
        }
    }

    int bx = panel.bounds.getRight() - PANEL_PAD;

    /* Title-bar MIX pie, pinned to the panel's right edge: caption + pie + its
     * (empty) modulation box, which overhangs to the pie's right. The box is
     * sized with a little slack (margin) around the pie so its top reach-ring and
     * right-hand modulation badge are not clipped by the control's own bounds. */
    if (panel.header_mix != nullptr) {
        int const mw = 74;
        int const mh = 22;
        bx -= mw;
        panel.header_mix->setBounds(bx, panel.bounds.getY() + 14 - mh / 2, mw, mh);
        bx -= 6;
    }

    /* Title-bar buttons, right-aligned and laid out left-to-right in order. */
    for (int i = (int)panel.buttons.size() - 1; i >= 0; --i) {
        int const bw = panel.buttons[(size_t)i]->preferred_width();
        bx -= bw;
        panel.buttons[(size_t)i]->setBounds(bx, by, bw, bh);
        bx -= 4;
    }

    /* Title-bar single-param pie (e.g. TAPE HISS), placed to the left of the
     * right-aligned buttons: caption + pie, no modulation box, so it is narrower
     * than the MIX pie. */
    if (panel.header_knob != nullptr) {
        int const kw = 54;
        int const kh = 22;
        bx -= kw;
        panel.header_knob->setBounds(bx, panel.bounds.getY() + 14 - kh / 2, kw, kh);
        bx -= 6;
    }
}


void EffectsPage::layout()
{
    int const avail = juce::jmax(
        1, viewport.getWidth() - viewport.getScrollBarThickness()
    );

    int max_row = 0;
    for (Panel const& panel : panels) {
        max_row = juce::jmax(max_row, panel.row);
    }

    int y = 0;
    int content_w = 0;

    for (int row = 0; row <= max_row; ++row) {
        /* First pass: gather this row's panels and their natural sizes, and count
         * the ones that flex to fill leftover width. */
        std::vector<Panel*> row_panels;
        int natural_w = 0;
        int fill_count = 0;

        for (Panel& panel : panels) {
            if (panel.row != row) {
                continue;
            }
            juce::Point<int> const sz = panel_size(panel);
            panel.bounds.setSize(sz.x, sz.y);   /* stash natural size for pass 2 */
            row_panels.push_back(&panel);
            natural_w += sz.x;
            if (panel.fill) {
                ++fill_count;
            }
        }

        if (row_panels.empty()) {
            continue;
        }

        /* Split any leftover width equally among the flex panels so together they
         * fill the row (minus the inter-panel gaps). */
        int const gaps = ((int)row_panels.size() - 1) * PANEL_GAP;
        int extra = 0;
        if (fill_count > 0) {
            int const leftover = avail - natural_w - gaps;
            if (leftover > 0) {
                extra = leftover / fill_count;
            }
        }

        /* Second pass: place left to right. */
        int x = 0;
        int row_h = 0;

        for (Panel* const pp : row_panels) {
            Panel& panel = *pp;
            int const w = panel.bounds.getWidth() + (panel.fill ? extra : 0);
            int const h = panel.bounds.getHeight();
            panel.bounds = juce::Rectangle<int>(x, y, w, h);
            place_panel(panel);

            x += w + PANEL_GAP;
            row_h = juce::jmax(row_h, h);
        }

        content_w = juce::jmax(content_w, x - PANEL_GAP);
        y += row_h + PANEL_GAP;
    }

    content.setSize(juce::jmax(avail, content_w), y + 2);
    content.repaint();
}


void EffectsPage::paint_content(juce::Graphics& g)
{
    g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f).withStyle("Bold")));

    for (Panel const& panel : panels) {
        juce::Rectangle<float> const r = panel.bounds.toFloat();
        g.setColour(Theme::PANEL);
        g.fillRoundedRectangle(r, Theme::RADIUS);
        g.setColour(Theme::EDGE);
        g.drawRoundedRectangle(r.reduced(0.5f), Theme::RADIUS, 1.0f);

        g.setColour(Theme::TEXT_DIM);
        g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f).withStyle("Bold")));
        g.drawText(
            panel.title,
            panel.bounds.reduced(10, 6).removeFromTop(16),
            juce::Justification::centredLeft,
            false
        );
    }
}


void EffectsPage::Content::paint(juce::Graphics& g)
{
    owner.paint_content(g);
}


void EffectsPage::refresh()
{
    for (Knob* const knob : knobs) {
        knob->refresh();
    }

    for (Control* const mix : mix_knobs) {
        mix->refresh();
    }

    for (MiniButton* const button : buttons) {
        button->refresh();
    }
}

}

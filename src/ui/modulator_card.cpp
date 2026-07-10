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

#include <algorithm>
#include <cmath>

#include "ui/modulator_card.hpp"

#include "ui/theme.hpp"


namespace JS80P
{

ModulatorCard::ModulatorCard(
        ParamBridge& bridge, ModulationManager& manager,
        ModulationManager::Group const& group
) : bridge(bridge),
    manager(manager),
    type(group.type),
    rep(group.rep),
    members(group.members),
    destinations(group.destinations),
    connections(group.connections),
    sus_fraction(0.0),
    lfo_expanded(false)
{
    setWantsKeyboardFocus(false);

    auto build_curve = [this](Synth::ParamId (*fn)(int)) {
        std::vector<Synth::ParamId> v;
        for (int m : members) {
            v.push_back(fn(m));
        }
        return v;
    };

    /* The other grouped copies' matching param, so modulating a knob applies to
     * every copy. */
    auto mirror = [this](Synth::ParamId (*fn)(int)) {
        std::vector<Synth::ParamId> v;
        for (int m : members) {
            if (m != rep) {
                v.push_back(fn(m));
            }
        }
        return v;
    };

    if (type == Modulation::ENVELOPE) {
        knobs.add(new Knob(bridge, Modulation::env_del(rep), "DLY"));
        knobs.add(new Knob(bridge, Modulation::env_atk(rep), "ATK"));
        knobs.getLast()->set_min_ratio(bridge.ratio_for_display(Modulation::env_atk(rep), 0.001));
        knobs.add(new Knob(bridge, Modulation::env_hld(rep), "HLD"));
        knobs.add(new Knob(bridge, Modulation::env_dec(rep), "DEC"));
        knobs.add(new Knob(bridge, Modulation::env_rel(rep), "REL"));

        for (Knob* const k : knobs) {
            k->set_manager(&manager);
            k->set_mod_caps(Modulation::CAP_MACRO);   /* envelope params: macros only */
            k->set_center_value(1.5);   /* time knobs: exponential, 1.5 second midpoint */
            /* No reserved value strip: the value shows in the hover/drag popover,
             * so the blank strip below each dial is reclaimed and the card is that
             * much shorter (see preferred_height / resized). */
            k->set_value_display(Control::ValueDisplay::POPOVER);
        }

        knobs[0]->set_mirrors(mirror(Modulation::env_del));
        knobs[1]->set_mirrors(mirror(Modulation::env_atk));
        knobs[2]->set_mirrors(mirror(Modulation::env_hld));
        knobs[3]->set_mirrors(mirror(Modulation::env_dec));
        knobs[4]->set_mirrors(mirror(Modulation::env_rel));

        curves.add(new CurveSelector(bridge, build_curve(Modulation::env_ash), false));
        curves.add(new CurveSelector(bridge, build_curve(Modulation::env_dsh), true));
        curves.add(new CurveSelector(bridge, build_curve(Modulation::env_rsh), true));

        for (CurveSelector* const c : curves) {
            addAndMakeVisible(c);
        }

        /* Sustain as a fraction between each destination's min (INI) and max
         * (PK); read the current fraction from the representative. */
        double const ini = bridge.get_ratio(Modulation::env_ini(rep));
        double const pk = bridge.get_ratio(Modulation::env_pk(rep));
        sus_fraction = (pk - ini) > 1.0e-6
            ? juce::jlimit(0.0, 1.0, (bridge.get_ratio(Modulation::env_sus(rep)) - ini) / (pk - ini))
            : 0.0;

        double const def_ini = bridge.get_default_ratio(Modulation::env_ini(rep));
        double const def_pk = bridge.get_default_ratio(Modulation::env_pk(rep));
        double const def_sus = bridge.get_default_ratio(Modulation::env_sus(rep));
        double const def_fraction = (def_pk - def_ini) > 1.0e-6
            ? juce::jlimit(0.0, 1.0, (def_sus - def_ini) / (def_pk - def_ini)) : 1.0;

        sus_fader = std::make_unique<Control>(
            bridge, Modulation::env_sus(rep), "SUS",
            Control::Style::V_SLIDER, Control::Size::NORMAL
        );
        sus_fader->set_value_hooks(
            [this]() { return sus_fraction; },
            [this](double const v) { sus_fraction = v; write_sustain(); },
            [](double const v) { return juce::String(v, 2); }
        );
        sus_fader->set_hook_default(def_fraction);
        sus_fader->set_value_display(Control::ValueDisplay::POPOVER);   /* reclaim the value strip */
        /* SUS is a macro-modulation destination like the other envelope params;
         * once assigned it edits env_sus directly (the fraction hook and the
         * periodic write_sustain step aside — see refresh / write_sustain). */
        sus_fader->set_manager(&manager);
        sus_fader->set_mod_caps(Modulation::CAP_MACRO);
        sus_fader->set_mirrors(mirror(Modulation::env_sus));
        addAndMakeVisible(*sus_fader);

        /* SCL lives in the middle of the header row (not among the knobs): a
         * horizontal slider that is a macro-only modulation destination, its
         * value/modulation mirrored onto every grouped member's SCL. */
        std::vector<Synth::ParamId> scl_mirrors;
        for (int const m : members) {
            if (m != rep) {
                scl_mirrors.push_back(Modulation::env_scl(m));
            }
        }

        env_scale = std::make_unique<Control>(
            bridge, Modulation::env_scl(rep), "SCL",
            Control::Style::H_SLIDER, Control::Size::SMALL
        );
        env_scale->set_manager(&manager);
        env_scale->set_mod_caps(Modulation::CAP_MACRO);
        env_scale->set_label_placement(Control::LabelPos::LEFT);
        env_scale->set_value_display(Control::ValueDisplay::POPOVER);
        env_scale->set_source_placeholder(true);
        env_scale->set_mirrors(scl_mirrors);
        env_scale->set_value_mirrors(scl_mirrors);
        addAndMakeVisible(*env_scale);

        /* Two tiny pie dots in the header's far-right band: time inaccuracy and
         * level inaccuracy, mirrored onto every grouped member. */
        tin_dot = std::make_unique<Control>(
            bridge, Modulation::env_tin(rep), "Time inacc.",
            Control::Style::DOT, Control::Size::TINY
        );
        tin_dot->set_value_display(Control::ValueDisplay::POPOVER);
        tin_dot->set_value_mirrors(mirror(Modulation::env_tin));
        tin_dot->setTooltip("Envelope time inaccuracy");
        addAndMakeVisible(*tin_dot);

        vin_dot = std::make_unique<Control>(
            bridge, Modulation::env_vin(rep), "Level inacc.",
            Control::Style::DOT, Control::Size::TINY
        );
        vin_dot->set_value_display(Control::ValueDisplay::POPOVER);
        vin_dot->set_value_mirrors(mirror(Modulation::env_vin));
        vin_dot->setTooltip("Envelope level inaccuracy");
        addAndMakeVisible(*vin_dot);
    } else if (type == Modulation::LFO) {
        wave = std::make_unique<WaveformSelector>(bridge, Modulation::lfo_wav(rep));
        wave->set_single(true);
        wave->set_on_click([this] { set_expanded(true); });
        addAndMakeVisible(*wave);

        sync_button = std::make_unique<SyncButton>();
        sync_button->setWantsKeyboardFocus(false);
        sync_button->is_active = [this] {
            return this->bridge.get_discrete(Modulation::lfo_syn(rep)) != 0;
        };
        sync_button->on_toggle = [this] { toggle_sync(); };
        addAndMakeVisible(*sync_button);

        shape_grid = std::make_unique<WaveformSelector>(bridge, Modulation::lfo_wav(rep));
        shape_grid->set_on_select([this](int) { set_expanded(false); });
        addChildComponent(*shape_grid);

        knobs.add(new Knob(bridge, Modulation::lfo_frq(rep), "RATE"));
        knobs.add(new Knob(bridge, Modulation::lfo_phs(rep), "PHS"));
        knobs.add(new Knob(bridge, Modulation::lfo_dst(rep), "DIST"));
        knobs.add(new Knob(bridge, Modulation::lfo_rnd(rep), "RAND"));

        for (Knob* const k : knobs) {
            k->set_manager(&manager);
            k->set_mod_caps(Modulation::CAP_LFO | Modulation::CAP_MACRO);   /* LFOs + macros */
            k->set_value_display(Control::ValueDisplay::POPOVER);   /* reclaim the value strip */
        }

        knobs[0]->set_mirrors(mirror(Modulation::lfo_frq));
        knobs[1]->set_mirrors(mirror(Modulation::lfo_phs));
        knobs[2]->set_mirrors(mirror(Modulation::lfo_dst));
        knobs[3]->set_mirrors(mirror(Modulation::lfo_rnd));
    }

    for (Knob* const k : knobs) {
        addAndMakeVisible(k);
    }
}


int ModulatorCard::preferred_height() const
{
    /* Header + a knob cell whose dial is the oscillator size, minus the value
     * strip the dials no longer reserve (they use the popover). 100 - 14 = 86. */
    return 86;
}


void ModulatorCard::set_expanded(bool const expanded)
{
    lfo_expanded = expanded;
    resized();
    repaint();
}


void ModulatorCard::write_sustain()
{
    for (int m : members) {
        double const ini = bridge.get_ratio(Modulation::env_ini(m));
        double const pk = bridge.get_ratio(Modulation::env_pk(m));
        double const target = juce::jlimit(0.0, 1.0, ini + sus_fraction * (pk - ini));

        if (std::fabs(bridge.get_ratio(Modulation::env_sus(m)) - target) > 1.0e-5) {
            bridge.set_ratio(Modulation::env_sus(m), target);
        }
    }
}


void ModulatorCard::toggle_sync()
{
    int const on = bridge.get_discrete(Modulation::lfo_syn(rep)) != 0 ? 0 : 1;

    for (int m : members) {
        bridge.set_discrete(Modulation::lfo_syn(m), on);
    }

    repaint();
}


void ModulatorCard::propagate()
{
    for (int m : members) {
        if (m == rep) {
            continue;
        }

        if (type == Modulation::ENVELOPE) {
            int const off[] = { 0, 2, 3, 5, 6, 8 };  /* SCL, DEL, ATK, HLD, DEC, REL */
            for (int o : off) {
                bridge.set_ratio(Modulation::pid((int)Modulation::env_scl(m) + o),
                                 bridge.get_ratio(Modulation::pid((int)Modulation::env_scl(rep) + o)));
            }
        } else if (type == Modulation::LFO) {
            bridge.set_ratio(Modulation::lfo_wav(m), bridge.get_ratio(Modulation::lfo_wav(rep)));
            bridge.set_ratio(Modulation::lfo_frq(m), bridge.get_ratio(Modulation::lfo_frq(rep)));
            bridge.set_ratio(Modulation::lfo_phs(m), bridge.get_ratio(Modulation::lfo_phs(rep)));
            bridge.set_ratio(Modulation::lfo_dst(m), bridge.get_ratio(Modulation::lfo_dst(rep)));
            bridge.set_ratio(Modulation::lfo_rnd(m), bridge.get_ratio(Modulation::lfo_rnd(rep)));

            if (Modulation::lfo_has_pw(rep) && Modulation::lfo_has_pw(m)) {
                bridge.set_ratio(Modulation::lfo_pw(m), bridge.get_ratio(Modulation::lfo_pw(rep)));
            }
        }
    }
}


void ModulatorCard::refresh()
{
    if (sus_fader != nullptr) {
        /* Track the fraction as min/max change, unless a modulator now owns
         * env_sus (then it must not be overwritten every frame). */
        if (!sus_fader->is_assigned()) {
            write_sustain();
        }
        sus_fader->refresh();
    }

    if (env_scale != nullptr) {
        env_scale->refresh();
    }

    if (tin_dot != nullptr) {
        tin_dot->refresh();
    }

    if (vin_dot != nullptr) {
        vin_dot->refresh();
    }

    for (Knob* const k : knobs) {
        k->refresh();
    }

    for (CurveSelector* const c : curves) {
        c->refresh();
    }

    if (wave != nullptr) {
        wave->refresh();
    }

    if (shape_grid != nullptr) {
        shape_grid->refresh();
    }
}


void ModulatorCard::resized()
{
    juce::Rectangle<int> b = getLocalBounds().reduced(6);

    /* SCL slider right-aligned against the far-right instability dots (time /
     * level inaccuracy), sitting just to their left; destination badges keep the
     * left of the header row. */
    if (env_scale != nullptr) {
        int const scl_w = 134;
        int const scl_h = 16;
        int const scl_y = 3;
        /* Mirror the dot geometry below (sz 16, gap 5, right = width - 6) to find
         * the left edge of the leftmost (time inaccuracy) dot. */
        int const dots_left = getWidth() - 6 - 2 * 16 - 5;
        int scl_x = dots_left - 8 - scl_w;
        scl_x = juce::jmax(scl_x, 8);
        env_scale->setBounds(scl_x, scl_y, scl_w, scl_h);
    }

    /* Time / level inaccuracy dots in the reserved far-right header band. */
    if (tin_dot != nullptr && vin_dot != nullptr) {
        int const sz = 16;
        int const gap = 5;
        int const right = getWidth() - 6;
        int const y = 11 - sz / 2;   /* centre on the header line (y 3..19) */
        vin_dot->setBounds(right - sz, y, sz, sz);
        tin_dot->setBounds(right - 2 * sz - gap, y, sz, sz);
    }

    b.removeFromTop(16);   /* one-line header */

    if (type == Modulation::LFO) {
        /* The expanded 2-row shape picker fits within the space below the title
         * (b) and is centred there - the cell height is reduced from the 32px
         * button size if needed so both rows fit without covering the title, and
         * the collapsed shape button matches that height. */
        juce::Rectangle<int> const grid_area = getLocalBounds().withTrimmedLeft(2).reduced(3);
        int const rows = WaveformSelector::ROWS;
        int const shape_h = juce::jmin(32, (b.getHeight() - 4) / rows);
        int const shape_w = grid_area.getWidth() / WaveformSelector::COLUMNS;

        if (lfo_expanded) {
            int const grid_h = rows * shape_h;
            shape_grid->setBounds(
                grid_area.getX(),
                b.getY() + (b.getHeight() - grid_h) / 2,
                grid_area.getWidth(),
                grid_h
            );
            shape_grid->setVisible(true);
            wave->setVisible(false);
            sync_button->setVisible(false);
            for (Knob* const k : knobs) k->setVisible(false);
            return;
        }

        shape_grid->setVisible(false);
        wave->setVisible(true);
        sync_button->setVisible(true);

        /* Four knobs (RATE | PHS | DIST | RAND) with 2px more padding to the right
         * of each, and the last one pulled in so it lines up with the envelope
         * card's last knob (REL) - clearing the right edge and leaving room for
         * its modulation badge. The envelope-row metrics are mirrored here to find
         * REL's centre, so the two must stay in sync (see the envelope layout
         * below). The WAVE shape button fills the leftover space to the left. */
        int const n = knobs.size();
        int const cell = 60;   /* fixed knob cell (8px more padding between knobs) */
        int const gap = 2;     /* 2px more padding to the right of each knob */

        int const e_cw = 16, e_sw = 26, e_sus_pad = 6;   /* mirror the envelope row */
        int const e_kw = juce::jmax(28, (b.getWidth() - 3 * e_cw - e_sw - e_sus_pad) / 5);
        int const e_total = 5 * e_kw + 3 * e_cw + e_sw + e_sus_pad;
        int const e_x0 = b.getX() + juce::jmax(0, (b.getWidth() - e_total) / 2);
        int const rel_centre = e_x0 + 4 * e_kw + 2 * e_cw + e_sw + e_sus_pad + e_kw / 2;

        int const last_x = rel_centre - cell / 2;                  /* RAND aligned to REL */
        int const knobs_x = last_x - (n - 1) * (cell + gap);

        for (int i = 0; i != n; ++i) {
            knobs[i]->setVisible(true);
            knobs[i]->setBounds(knobs_x + i * (cell + gap), b.getY(), cell, b.getHeight());
        }

        /* Selected-shape button sized to match one expanded picker cell, so the
         * display doesn't jump size when it collapses. Vertically centred on the
         * knob row, in the space left of the knobs. */
        int const wave_w = shape_w;
        int const wy = b.getY() + (b.getHeight() - shape_h) / 2;
        int const wx = b.getX() + juce::jmax(0, (knobs_x - b.getX() - wave_w) / 2);
        wave->setBounds(wx, wy, wave_w, shape_h);

        /* BPM tempo-sync toggle lives in the title bar, centred over the RATE
         * knob, exactly like the CHORUS/ECHO BPM toggles over FREQ / DELAY. */
        int const bpm_w = 30;
        int const bpm_h = 14;
        int const bpm_x = knobs[0]->getBounds().getCentreX() - bpm_w / 2;
        sync_button->setBounds(bpm_x, 3, bpm_w, bpm_h);
        return;
    }

    /* Envelope row:
     * DLY | A-curve | ATK | HLD | DEC | D-curve | SUS | REL | R-curve.
     * Knobs and the sustain fader are full height; the curve selectors sit in
     * their own space along the bottom edge. */
    int const cw = 16;   /* curve square */
    int const sw = 26;   /* sustain fader */
    int const sus_pad = 6;   /* room right of SUS so a wide mod source clears REL */
    int const kw = juce::jmax(28, (b.getWidth() - 3 * cw - sw - sus_pad) / 5);
    int const total = 5 * kw + 3 * cw + sw + sus_pad;
    int const h = b.getHeight();
    int x = b.getX() + juce::jmax(0, (b.getWidth() - total) / 2);

    /* Bottom of a knob's circle (mirrors Knob::knob_circle), so the curve squares
     * line up with the base of the dials. The dials reserve a top caption strip
     * (14) but no bottom value strip, so the dial box is h-18 and its centre sits
     * 7px below the cell midline (not on it). */
    int const circle = juce::jmin(kw - 4, h - 18);
    int const circle_bottom = h / 2 + circle / 2 + 4;

    auto place_knob = [&](int const i) {
        knobs[i]->setBounds(x, b.getY(), kw, h);
        x += kw;
    };
    auto place_curve = [&](int const i) {
        curves[i]->setBounds(x, b.getY() + circle_bottom - cw, cw, cw);
        x += cw;
    };

    place_knob(0);    /* DLY */
    place_curve(0);   /* attack curve */
    x -= 4;           /* tighter gap after the attack curve */
    place_knob(1);    /* ATK */
    x += 2;           /* padding after ATK */
    place_knob(2);    /* HLD */
    x += 2;           /* padding after HLD */
    place_knob(3);    /* DEC */
    place_curve(1);   /* decay curve */
    sus_fader->setBounds(x, b.getY(), sw, h);   /* SUS */
    x += sw + sus_pad;
    place_knob(4);    /* REL */
    place_curve(2);   /* release curve */
}


void ModulatorCard::SyncButton::paint(juce::Graphics& g)
{
    bool const on = is_active && is_active();
    juce::Colour const c = Theme::ACCENT;
    juce::Rectangle<float> const r = getLocalBounds().toFloat().reduced(0.5f);
    float const radius = 2.0f;

    if (on) {
        g.setColour(hover ? c.brighter(0.15f) : c);
        g.fillRoundedRectangle(r, radius);
    } else if (hover) {
        g.setColour(c.withAlpha(0.18f));
        g.fillRoundedRectangle(r, radius);
    }

    g.setColour(on || !hover ? c : c.brighter(0.2f));
    g.drawRoundedRectangle(r, radius, 1.0f);

    /* Solid fill needs dark text for contrast; the outline state keeps it orange. */
    g.setColour(on ? Theme::BG : c);
    g.setFont(9.0f);
    g.drawText("BPM", getLocalBounds(), juce::Justification::centred, false);
}


void ModulatorCard::SyncButton::mouseUp(juce::MouseEvent const& event)
{
    if (on_toggle && getLocalBounds().contains(event.getPosition())) {
        on_toggle();
    }
}


void ModulatorCard::SyncButton::mouseEnter(juce::MouseEvent const& /* event */)
{
    hover = true;
    repaint();
}


void ModulatorCard::SyncButton::mouseExit(juce::MouseEvent const& /* event */)
{
    hover = false;
    repaint();
}


void ModulatorCard::paint(juce::Graphics& g)
{
    /* Transparent background; a single 2px accent line on the left. */
    g.setColour(Modulation::colour(type));
    g.fillRect(0.0f, 0.0f, 2.0f, (float)getHeight());

    juce::Font const sf(juce::FontOptions().withHeight(12.0f).withStyle("Bold"));
    juce::Font const df(juce::FontOptions().withHeight(11.0f));
    juce::Colour const accent = Modulation::colour(type);

    auto tw = [](juce::Font const& f, juce::String const& s) {
        return (int)juce::GlyphArrangement::getStringWidth(f, s);
    };

    int const left = 8;
    int const y = 3;
    int const th = 14;

    /* Keep the header text clear of the right-side header controls: on envelope
     * cards the SCL slider (with the instability dots beyond it) owns the right
     * of the row, so the title must stop at the SCL's left edge. */
    int right = getWidth() - 8;

    if (env_scale != nullptr) {
        right = juce::jmax(left, env_scale->getX() - 6);
    } else if (sync_button != nullptr && sync_button->isVisible()) {
        /* LFO cards: the BPM toggle sits in the title bar over RATE, so the
         * connection labels must stop at its left edge. */
        right = juce::jmax(left, sync_button->getX() - 6);
    }
    int const avail = right - left;

    /* Preferred layout: each slot next to its destination -
     * "E1 <dest>  E5 <dest> ...". */
    int full_w = 0;

    for (std::pair<int, Synth::ParamId> const& c : connections) {
        juce::String const slot = juce::String(Modulation::prefix(type)) + juce::String(c.first);
        juce::String const dest = Modulation::display_dest_name(bridge.param_name(c.second));
        full_w += tw(sf, slot) + 4 + tw(df, dest) + 10;
    }

    if (full_w <= avail) {
        int x = left;

        for (std::pair<int, Synth::ParamId> const& c : connections) {
            juce::String const slot = juce::String(Modulation::prefix(type)) + juce::String(c.first);
            g.setFont(sf);
            g.setColour(accent);
            g.drawText(slot, x, y, right - x, th, juce::Justification::centredLeft, false);
            x += tw(sf, slot) + 4;

            juce::String const dest = Modulation::display_dest_name(bridge.param_name(c.second));
            g.setFont(df);
            g.setColour(Theme::TEXT_DIM);
            g.drawText(dest, x, y, right - x, th, juce::Justification::centredLeft, false);
            x += tw(df, dest) + 10;
        }

        return;
    }

    /* Too wide for the interleaved form: fall back to a compact one - every
     * envelope name, a dash, then the modulation targets, ellipsised to fit,
     * e.g. "E1 E5 E7 - AMP1 AMP2 E..". */
    juce::String slots;
    std::vector<int> seen;

    for (std::pair<int, Synth::ParamId> const& c : connections) {
        if (std::find(seen.begin(), seen.end(), c.first) != seen.end()) {
            continue;
        }

        seen.push_back(c.first);
        slots += (slots.isEmpty() ? "" : " ")
            + juce::String(Modulation::prefix(type)) + juce::String(c.first);
    }

    juce::String dests;

    for (std::pair<int, Synth::ParamId> const& c : connections) {
        juce::String const dest = Modulation::display_dest_name(bridge.param_name(c.second));
        dests += (dests.isEmpty() ? "" : " ") + dest;
    }

    /* Trim a string (dropping whole characters) until it plus a trailing ".."
     * fits the given width. */
    auto fit = [&](juce::Font const& f, juce::String s, int const maxw) {
        if (tw(f, s) <= maxw) {
            return s;
        }

        while (s.isNotEmpty() && tw(f, s + "..") > maxw) {
            s = s.dropLastCharacters(1);
        }

        return s.trimEnd() + "..";
    };

    int x = left;

    juce::String const slots_fit = fit(sf, slots, right - x);
    g.setFont(sf);
    g.setColour(accent);
    g.drawText(slots_fit, x, y, right - x, th, juce::Justification::centredLeft, false);
    x += tw(sf, slots_fit);

    /* Only append the targets if all envelope names fit (otherwise the names
     * themselves were already ellipsised). */
    if (slots_fit == slots && !dests.isEmpty()) {
        juce::String const sep = " - ";
        g.setFont(df);
        g.setColour(Theme::TEXT_DIM);
        g.drawText(sep, x, y, right - x, th, juce::Justification::centredLeft, false);
        x += tw(df, sep);

        g.drawText(fit(df, dests, right - x), x, y, right - x, th,
                   juce::Justification::centredLeft, false);
    }
}

}

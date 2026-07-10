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

#include "ui/per_type_editor.hpp"

#include "ui/theme.hpp"


namespace JS80P
{

/* Oscillator::Waveform: only the pulse-family shapes carry a meaningful pulse
 * width - PULSE=9, SOFT_PULSE=10, BIPOLAR_PULSE=11, SOFT_BIPOLAR_PULSE=12.
 * SQUARE=7 / SOFT_SQUARE=8 are fixed 50% duty (no PW knob); CUSTOM=13 uses the
 * harmonics editor instead. */
static bool waveform_uses_pulse_width(int const w)
{
    return w >= 9 && w <= 12;
}


PerTypeEditor::PerTypeEditor(
        ParamBridge& bridge,
        ModulationManager& manager,
        Synth::ParamId const waveform,
        Synth::ParamId const pulse_width,
        Synth::ParamId const first_harmonic
) : bridge(bridge),
    manager(manager),
    waveform(waveform),
    pulse_width(pulse_width),
    first_harmonic(first_harmonic),
    pw_knob(bridge, pulse_width, "PW"),
    mode(NONE)
{
    setWantsKeyboardFocus(false);

    /* The groups are pure containers: their children (controls + badges) take the
     * clicks; empty areas fall through to the editor. Hidden until their mode is
     * selected. */
    pulse_group.setInterceptsMouseClicks(false, true);
    custom_group.setInterceptsMouseClicks(false, true);
    addChildComponent(pulse_group);
    addChildComponent(custom_group);

    /* Pulse width is a full modulation destination (env / LFO / macro). Its badge
     * becomes a child of pulse_group (see Control::update_badge), so hiding the
     * group hides the badge too. */
    pw_knob.set_manager(&manager);
    pulse_group.addChildComponent(pw_knob);

    /* One bipolar bar per custom harmonic. Harmonics only accept macro / random
     * assignment (no per-note env or LFO) - a harmonic that morphed continuously
     * would just be a second, redundant custom-waveform modulator. */
    for (int i = 0; i != HARMONICS; ++i) {
        HarmonicSlider* const bar = new HarmonicSlider(
            bridge, (Synth::ParamId)((int)first_harmonic + i)
        );
        bar->set_manager(&manager);
        bar->set_mod_caps(Modulation::CAP_MACRO);
        harmonics.add(bar);
        custom_group.addChildComponent(bar);
    }
}


PerTypeEditor::Mode PerTypeEditor::mode_for(int const w) const
{
    if (w == 13) {
        return CUSTOM;
    }

    if (waveform_uses_pulse_width(w)) {
        return PULSE;
    }

    return NONE;
}


void PerTypeEditor::refresh()
{
    Mode const now = mode_for(bridge.get_discrete(waveform));

    if (now != mode) {
        mode = now;

        /* Toggle the whole group (control + badges) per mode; NONE hides both, so
         * the slot is truly empty. */
        pulse_group.setVisible(mode == PULSE);
        custom_group.setVisible(mode == CUSTOM);
        pw_knob.setVisible(mode == PULSE);
        for (HarmonicSlider* const bar : harmonics) {
            bar->setVisible(mode == CUSTOM);
        }
        resized();
        repaint();
    }

    if (mode == PULSE) {
        pw_knob.refresh();
    } else if (mode == CUSTOM) {
        for (HarmonicSlider* const bar : harmonics) {
            bar->refresh();
        }
    }
}


void PerTypeEditor::resized()
{
    /* Both groups fill the whole slot, so the controls inside keep their
     * area-relative geometry (and their badges land in the same place). */
    pulse_group.setBounds(getLocalBounds());
    custom_group.setBounds(getLocalBounds());

    /* Pulse width: a single knob on the left, sized like the oscillator knobs
     * (cell 58 high, matching lay_out_osc) rather than filling the whole per-type
     * area, so its dial is the same 40px as the other oscillator knobs. */
    int const knob_w = juce::jmin(getWidth() / 4, 78);
    pw_knob.setBounds(4, 0, knob_w, 58);

    /* Custom harmonics: one bipolar bar per harmonic across the harmonics area,
     * each carrying its modulation badge below the bar. */
    juce::Rectangle<int> const area = harmonics_area();

    if (area.getWidth() > 0 && !harmonics.isEmpty()) {
        float const bar_w = (float)area.getWidth() / (float)HARMONICS;

        for (int i = 0; i != HARMONICS; ++i) {
            harmonics[i]->setBounds(
                (int)std::lround((double)area.getX() + (double)i * bar_w),
                area.getY(),
                (int)std::lround((double)bar_w),
                area.getHeight()
            );
        }
    }
}


juce::Rectangle<int> PerTypeEditor::harmonics_area() const
{
    return getLocalBounds().reduced(4, 6).withTrimmedTop(12);
}


void PerTypeEditor::paint(juce::Graphics& g)
{
    /* The bars are child HarmonicSliders now; only the section label / empty
     * placeholder is painted here. */
    /* Custom shows a section label; NONE / PULSE draw nothing here (an empty slot
     * for NONE, just the PW knob for PULSE). */
    if (mode == CUSTOM) {
        g.setColour(Theme::TEXT_DIM);
        g.setFont(10.0f);
        g.drawText("HARMONICS", getLocalBounds().reduced(4, 2).removeFromTop(11),
                   juce::Justification::centredLeft, false);
    }
}

}

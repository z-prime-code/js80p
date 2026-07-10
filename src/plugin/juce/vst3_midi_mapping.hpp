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

#ifndef JS80P__PLUGIN__JUCE__VST3_MIDI_MAPPING_HPP
#define JS80P__PLUGIN__JUCE__VST3_MIDI_MAPPING_HPP

#include <array>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>


namespace JS80P
{

/*
 * Number of MIDI controller numbers VST3 distinguishes, and the size of the
 * "is this controller mapped?" table below: CC 0-127, plus channel pressure
 * (128 = Vst::kAfterTouch) and pitch bend (129 = Vst::kPitchBend). Equals
 * Steinberg's Vst::kCountCtrlNumber (statically asserted in the .cpp).
 */
constexpr int MIDI_CTL_COUNT = 130;

/*
 * Build the VST3 client-extension object that gives the host our own
 * \c IMidiMapping, matching the legacy JS80P VST3 plugin (see BUILD.md).
 *
 * Reach: JUCE's VST3 wrapper can only hand this interface out once the host has
 * connected the component and the controller (the wrapper reaches the
 * extensions through the AudioProcessor, which the controller learns about in
 * that handshake). Hosts that query \c IMidiMapping earlier and cache it
 * (Bitwig) get the wrapper controller's own implementation instead — which is
 * why the build keeps \c JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS enabled as
 * the always-available fallback: those hosts route MIDI through JUCE's hidden
 * CC parameters into processBlock()'s MidiBuffer, while late-querying hosts
 * (REAPER) get this legacy mapping (user-provided interfaces win in the
 * wrapper).
 *
 * This is deliberately decoupled from the JS80P DSP/GUI headers so the
 * implementation translation unit needs only JUCE + the Steinberg VST3 SDK and
 * can be compiled into the VST3 target ALONE (the SDK's interface-IID symbols
 * must not leak into non-VST3 links). The caller — which does have access to the
 * synth — passes \p supported_controllers , where index \c c is true for every
 * MIDI controller number that has a corresponding automatable parameter, so the
 * mapping only advertises controllers the plugin actually exposes.
 *
 * The implementation ( \c vst3_midi_mapping.cpp ) is compiled into every
 * plugin-format target — never the shared-code target — and switches on
 * JUCE's per-format \c JucePlugin_Build_VST3 define: the VST3 target gets the
 * real mapping, every other format compiles a \c nullptr fallback (the
 * extension is never queried without a VST3 host).
 */
std::unique_ptr<juce::VST3ClientExtensions> create_vst3_midi_mapping_extensions(
    std::array<bool, MIDI_CTL_COUNT> const& supported_controllers
);

}

#endif

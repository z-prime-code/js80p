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

#ifndef JS80P__PLUGIN__JUCE__MIDI_CTL_PARAMETER_HPP
#define JS80P__PLUGIN__JUCE__MIDI_CTL_PARAMETER_HPP

#include <juce_audio_processors/juce_audio_processors.h>

#include "gui/gui.hpp"

#include "js80p.hpp"
#include "midi.hpp"
#include "synth.hpp"


namespace JS80P
{

/**
 * \brief One host-automatable parameter mirroring a single MIDI controller (a
 *        CC, pitch bend, or channel pressure) on a single MIDI channel, exactly
 *        as the original hand-rolled JS80P VST3 plugin exposed it.
 *
 * Drop-in automation compatibility hinges on the VST3 ParamID matching the
 * legacy scheme, \c (channel<<8)|controller_number . JUCE derives a VST3
 * ParamID from a parameter's string id via
 * \c juce::String::hashCode()&0x7fffffff (see
 * \c VST3ClientExtensions::convertJuceParameterId ). Because that hash is the
 * classic \c h=31*h+codepoint polynomial, a string made of a SINGLE Unicode
 * code point \c C hashes to exactly \c C . So we build the id string from the
 * one code point equal to the wanted ParamID and JUCE hands the host that exact
 * numeric id — no host-side remapping needed, so old automation reconnects in
 * every host. The ctor \c jassert guards against a future JUCE changing the
 * hash.
 *
 * The DSP is never touched: on automation we merely translate the value into the
 * same \c Synth MIDI-controller call the legacy processor made.
 */
class MidiCtlParameter : public juce::AudioParameterFloat
{
    public:
        /*
         * Steinberg Vst::ControllerNumbers values that the legacy plugin used as
         * the low byte of the ParamID for the two non-CC controllers.
         */
        static constexpr int CTL_CHANNEL_PRESSURE = 128; /* Vst::kAfterTouch */
        static constexpr int CTL_PITCH_BEND = 129;       /* Vst::kPitchBend  */

        static juce::uint32 param_id(
                Midi::Channel const channel,
                int const controller_number
        ) noexcept {
            return (
                ((juce::uint32)channel << 8)
                | (juce::uint32)(controller_number & 0xff)
            );
        }

        MidiCtlParameter(
                Midi::Channel const channel,
                int const controller_number,
                Synth::ControllerId const name_source,
                double const default_normalized
        )
            : juce::AudioParameterFloat(
                make_parameter_id(channel, controller_number),
                make_name(channel, name_source),
                juce::NormalisableRange<float>(0.0f, 100.0f),
                (float)(default_normalized * 100.0),
                make_attributes()
            ),
            channel(channel),
            controller_number(controller_number),
            last_normalized((float)default_normalized)
        {
        }

        /*
         * Current value in [0, 1]. AudioParameterFloat::getValue() is private
         * (JUCE steers callers to get()), so go through the range instead.
         */
        float get_normalized() const noexcept
        {
            return getNormalisableRange().convertTo0to1(get());
        }

        Midi::Channel const channel;
        int const controller_number;

        /* Last value dispatched to the Synth; only changes are forwarded. */
        float last_normalized;

    private:
        static juce::ParameterID make_parameter_id(
                Midi::Channel const channel,
                int const controller_number
        ) {
            juce::uint32 const id = param_id(channel, controller_number);

            /*
             * A one-code-point string whose JUCE hash equals the code point, so
             * the VST3 ParamID JUCE reports equals the legacy id (see class
             * docs). The string is JUCE-internal: the host only ever sees the
             * numeric id and the human-readable name, and our state chunk is the
             * raw Serializer patch, so this control code point is never
             * serialized or displayed.
             */
            juce::String const id_string(
                juce::String::charToString((juce::juce_wchar)id)
            );

            /* If a future JUCE changes its parameter-id hashing, this fires. */
            jassert(
                juce::VST3ClientExtensions::convertJuceParameterId(id_string)
                == id
            );

            return juce::ParameterID(id_string, 1);
        }

        static juce::String make_name(
                Midi::Channel const channel,
                Synth::ControllerId const name_source
        ) {
            GUI::Controller const* const controller = (
                GUI::get_controller(name_source)
            );

            juce::String const base(
                controller != nullptr
                    ? juce::String::fromUTF8(controller->long_name)
                    : juce::String("CC")
            );

            return base + " [" + juce::String((int)channel + 1) + "]";
        }

        static juce::AudioParameterFloatAttributes make_attributes()
        {
            return juce::AudioParameterFloatAttributes()
                .withLabel("%")
                .withStringFromValueFunction(
                    [](float const value, int) {
                        return juce::String(value, 1) + " %";
                    }
                );
        }
};

}

#endif

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
#include <string>

#include "plugin/juce/PluginProcessor.hpp"
#include "plugin/juce/PluginEditor.hpp"

#include "midi.hpp"
#include "serializer.hpp"


namespace JS80P
{

JS80PProcessor::JS80PProcessor()
    : juce::AudioProcessor(
        BusesProperties().withOutput(
            "Output", juce::AudioChannelSet::stereo(), true
        )
    ),
    synth(),
    renderer(synth),
    bank(),
    current_program(0)
{
    Serializer::import_patch_in_gui_thread(synth, bank[current_program].serialize());
    build_midi_ctl_params();
    vst3_extensions = create_vst3_midi_mapping_extensions(
        collect_supported_midi_controllers()
    );
}


JS80PProcessor::~JS80PProcessor()
{
}


void JS80PProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    synth.set_sample_rate((Frequency)sampleRate);
    renderer.reset();
    setLatencySamples((int)renderer.get_latency_samples());
}


void JS80PProcessor::releaseResources()
{
    synth.suspend();
    renderer.reset();
}


bool JS80PProcessor::isBusesLayoutSupported(BusesLayout const& layouts) const
{
    return (
        layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
    );
}


void JS80PProcessor::update_bpm()
{
    juce::AudioPlayHead* const play_head = getPlayHead();

    if (play_head == nullptr) {
        return;
    }

    juce::Optional<juce::AudioPlayHead::PositionInfo> const position = (
        play_head->getPosition()
    );

    if (!position.hasValue()) {
        return;
    }

    juce::Optional<double> const bpm = position->getBpm();

    if (bpm.hasValue()) {
        synth.set_bpm((Number)*bpm);
    }
}


void JS80PProcessor::dispatch_midi(juce::MidiBuffer& midi)
{
    for (juce::MidiMessageMetadata const metadata : midi) {
        juce::MidiMessage const message = metadata.getMessage();

        int const raw_channel = message.getChannel();

        if (raw_channel < 1) {
            continue;
        }

        Seconds const time_offset = synth.sample_count_to_time_offset(
            (Integer)metadata.samplePosition
        );
        Midi::Channel const channel = (Midi::Channel)(raw_channel - 1);

        if (message.isNoteOn()) {
            synth.note_on(
                time_offset,
                channel,
                (Midi::Note)message.getNoteNumber(),
                (Midi::Byte)message.getVelocity()
            );
        } else if (message.isNoteOff()) {
            synth.note_off(
                time_offset,
                channel,
                (Midi::Note)message.getNoteNumber(),
                (Midi::Byte)message.getVelocity()
            );
        } else if (message.isController()) {
            synth.control_change(
                time_offset,
                channel,
                (Midi::Controller)message.getControllerNumber(),
                (Midi::Byte)message.getControllerValue()
            );
        } else if (message.isPitchWheel()) {
            synth.pitch_wheel_change(
                time_offset, channel, (Midi::Word)message.getPitchWheelValue()
            );
        } else if (message.isChannelPressure()) {
            synth.channel_pressure(
                time_offset, channel, (Midi::Byte)message.getChannelPressureValue()
            );
        } else if (message.isAftertouch()) {
            synth.aftertouch(
                time_offset,
                channel,
                (Midi::Note)message.getNoteNumber(),
                (Midi::Byte)message.getAfterTouchValue()
            );
        }
    }
}


void JS80PProcessor::build_midi_ctl_params()
{
    /*
     * Mirror JS80P's original VST3 controller layout: for every MIDI channel,
     * a pitch-bend and a channel-pressure parameter, plus one parameter for
     * each supported MIDI CC. The ParamID ((channel<<8)|controller) and the
     * default values match the legacy plugin so that projects automating the
     * original JS80P reconnect (see MidiCtlParameter and BUILD.md).
     */
    for (Midi::Channel channel = 0; channel != Midi::CHANNELS; ++channel) {
        add_midi_ctl_param(
            channel,
            MidiCtlParameter::CTL_PITCH_BEND,
            Synth::ControllerId::PITCH_WHEEL,
            0.5
        );
        add_midi_ctl_param(
            channel,
            MidiCtlParameter::CTL_CHANNEL_PRESSURE,
            Synth::ControllerId::CHANNEL_PRESSURE,
            0.0
        );

        for (int cc = 0; cc != Synth::MIDI_CONTROLLERS; ++cc) {
            if (!Synth::is_supported_midi_controller((Midi::Controller)cc)) {
                continue;
            }

            double const default_normalized = (
                cc == (int)Synth::ControllerId::SUSTAIN_PEDAL ? 0.0 : 0.5
            );

            add_midi_ctl_param(
                channel, cc, (Synth::ControllerId)cc, default_normalized
            );
        }
    }
}


std::array<bool, MIDI_CTL_COUNT>
JS80PProcessor::collect_supported_midi_controllers()
{
    /*
     * The controller numbers that have a corresponding automatable parameter,
     * for the VST3 IMidiMapping to advertise (see create_vst3_midi_mapping_
     * extensions). Must stay in step with build_midi_ctl_params(): every
     * supported CC, plus pitch bend and channel pressure.
     */
    std::array<bool, MIDI_CTL_COUNT> supported{};

    supported[MidiCtlParameter::CTL_PITCH_BEND] = true;
    supported[MidiCtlParameter::CTL_CHANNEL_PRESSURE] = true;

    for (int cc = 0; cc != Synth::MIDI_CONTROLLERS; ++cc) {
        if (Synth::is_supported_midi_controller((Midi::Controller)cc)) {
            supported[(size_t)cc] = true;
        }
    }

    return supported;
}


void JS80PProcessor::add_midi_ctl_param(
        Midi::Channel const channel,
        int const controller_number,
        Synth::ControllerId const name_source,
        double const default_normalized
) {
    MidiCtlParameter* const param = new MidiCtlParameter(
        channel, controller_number, name_source, default_normalized
    );

    addParameter(param);
    midi_ctl_params.push_back(param);
}


void JS80PProcessor::dispatch_parameter_automation() noexcept
{
    Seconds const time_offset = synth.sample_count_to_time_offset(0);

    for (MidiCtlParameter* const param : midi_ctl_params) {
        float const normalized = param->get_normalized();

        if (normalized == param->last_normalized) {
            continue;
        }

        param->last_normalized = normalized;

        switch (param->controller_number) {
            case MidiCtlParameter::CTL_PITCH_BEND:
                synth.pitch_wheel_change(
                    time_offset, param->channel, float_to_midi_word(normalized)
                );
                break;

            case MidiCtlParameter::CTL_CHANNEL_PRESSURE:
                synth.channel_pressure(
                    time_offset, param->channel, float_to_midi_byte(normalized)
                );
                break;

            default:
                synth.control_change(
                    time_offset,
                    param->channel,
                    (Midi::Controller)param->controller_number,
                    float_to_midi_byte(normalized)
                );
                break;
        }
    }
}


Midi::Byte JS80PProcessor::float_to_midi_byte(float const value) noexcept
{
    return (Midi::Byte)juce::jlimit(0, 127, (int)std::round(value * 127.0f));
}


Midi::Word JS80PProcessor::float_to_midi_word(float const value) noexcept
{
    /* Outside the 14-bit range, but more accurate; matches the legacy plugin. */
    return (Midi::Word)juce::jlimit(0, 16384, (int)std::round(value * 16384.0f));
}


template<typename FloatType>
void JS80PProcessor::render(
        juce::AudioBuffer<FloatType>& buffer,
        juce::MidiBuffer& midi
) {
    juce::ScopedNoDenormals const no_denormals;

    update_bpm();
    dispatch_parameter_automation();
    dispatch_midi(midi);

    int const sample_count = buffer.getNumSamples();
    int const num_channels = buffer.getNumChannels();

    if (num_channels >= (int)Synth::OUT_CHANNELS && sample_count > 0) {
        FloatType* out_channels[Synth::OUT_CHANNELS];

        for (int c = 0; c != (int)Synth::OUT_CHANNELS; ++c) {
            out_channels[c] = buffer.getWritePointer(c);
        }

        renderer.render<FloatType>(
            (Integer)sample_count,
            (FloatType const* const*)nullptr,
            out_channels
        );
    } else {
        buffer.clear();
    }

    for (int c = (int)Synth::OUT_CHANNELS; c < num_channels; ++c) {
        buffer.clear(c, 0, sample_count);
    }

    midi.clear();
}


void JS80PProcessor::processBlock(
        juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi
) {
    render<float>(buffer, midi);
}


void JS80PProcessor::processBlock(
        juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi
) {
    render<double>(buffer, midi);
}


bool JS80PProcessor::supportsDoublePrecisionProcessing() const
{
    return true;
}


juce::AudioProcessorEditor* JS80PProcessor::createEditor()
{
    return new JS80PEditor(*this);
}


bool JS80PProcessor::hasEditor() const
{
    return true;
}


juce::VST3ClientExtensions* JS80PProcessor::getVST3ClientExtensions()
{
    return vst3_extensions.get();
}


juce::String const JS80PProcessor::getName() const
{
    return JucePlugin_Name;
}


bool JS80PProcessor::acceptsMidi() const
{
    return true;
}


bool JS80PProcessor::producesMidi() const
{
    return false;
}


bool JS80PProcessor::isMidiEffect() const
{
    return false;
}


double JS80PProcessor::getTailLengthSeconds() const
{
    return 0.0;
}


int JS80PProcessor::getNumPrograms()
{
    return (int)Bank::NUMBER_OF_PROGRAMS;
}


int JS80PProcessor::getCurrentProgram()
{
    return current_program;
}


void JS80PProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= (int)Bank::NUMBER_OF_PROGRAMS) {
        return;
    }

    current_program = index;
    Serializer::import_patch_in_gui_thread(synth, bank[index].serialize());
}


juce::String const JS80PProcessor::getProgramName(int index)
{
    if (index < 0 || index >= (int)Bank::NUMBER_OF_PROGRAMS) {
        return {};
    }

    return juce::String::fromUTF8(bank[index].get_name().c_str());
}


void JS80PProcessor::changeProgramName(
        int /* index */, juce::String const& /* new_name */
) {
}


void JS80PProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    std::string const serialized = Serializer::serialize(synth);

    dest.setSize(0);
    dest.append(serialized.data(), serialized.size());
}


void JS80PProcessor::setStateInformation(void const* data, int size_in_bytes)
{
    if (data == nullptr || size_in_bytes <= 0) {
        return;
    }

    std::string const serialized((char const*)data, (size_t)size_in_bytes);

    Serializer::import_patch_in_gui_thread(synth, serialized);
    synth.push_message(
        Synth::MessageType::CLEAR_DIRTY_FLAG,
        Synth::ParamId::INVALID_PARAM_ID,
        0.0,
        0
    );
}


Synth& JS80PProcessor::get_synth() noexcept
{
    return synth;
}

}


juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JS80P::JS80PProcessor();
}

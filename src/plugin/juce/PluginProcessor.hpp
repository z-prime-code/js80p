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

#ifndef JS80P__PLUGIN__JUCE__PLUGIN_PROCESSOR_HPP
#define JS80P__PLUGIN__JUCE__PLUGIN_PROCESSOR_HPP

#include <array>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "js80p.hpp"

#include "bank.hpp"
#include "midi.hpp"
#include "renderer.hpp"
#include "synth.hpp"

#include "plugin/juce/midi_ctl_parameter.hpp"
#include "plugin/juce/vst3_midi_mapping.hpp"


namespace JS80P
{

/**
 * \brief JUCE wrapper around the (unchanged) JS80P \c Synth. Owns the DSP; the
 *        editor talks to the live \c Synth through the existing lock-free
 *        message queue + atomic getters.
 */
class JS80PProcessor : public juce::AudioProcessor
{
    public:
        JS80PProcessor();
        ~JS80PProcessor() override;

        void prepareToPlay(double sampleRate, int samplesPerBlock) override;
        void releaseResources() override;

        bool isBusesLayoutSupported(BusesLayout const& layouts) const override;

        void processBlock(
            juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi
        ) override;

        void processBlock(
            juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi
        ) override;

        bool supportsDoublePrecisionProcessing() const override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override;

        /*
         * Hand the VST3 host our own IMidiMapping (see
         * create_vst3_midi_mapping_extensions) so live MIDI CC / pitch-bend /
         * channel-pressure reaches the legacy MIDI-controller parameters and so
         * hosts treat those parameters as MIDI destinations instead of listing
         * the whole matrix as ordinary automation. Only hosts that query the
         * interface after connecting component and controller receive it;
         * early-querying hosts (Bitwig) fall back to JUCE's built-in MIDI-CC
         * emulation, which must therefore stay enabled (see CMakeLists.txt and
         * vst3_midi_mapping.hpp).
         */
        juce::VST3ClientExtensions* getVST3ClientExtensions() override;

        juce::String const getName() const override;

        bool acceptsMidi() const override;
        bool producesMidi() const override;
        bool isMidiEffect() const override;
        double getTailLengthSeconds() const override;

        int getNumPrograms() override;
        int getCurrentProgram() override;
        void setCurrentProgram(int index) override;
        juce::String const getProgramName(int index) override;
        void changeProgramName(int index, juce::String const& new_name) override;

        void getStateInformation(juce::MemoryBlock& dest) override;
        void setStateInformation(void const* data, int size_in_bytes) override;

        Synth& get_synth() noexcept;

    private:
        template<typename FloatType>
        void render(juce::AudioBuffer<FloatType>& buffer, juce::MidiBuffer& midi);

        void update_bpm();
        void dispatch_midi(juce::MidiBuffer& midi);

        /*
         * Recreate the legacy VST3 plugin's full MIDI-controller parameter
         * matrix (every supported CC + pitch bend + channel pressure on each of
         * the 16 MIDI channels) so that host automation saved against the
         * original plugin reconnects. See \c MidiCtlParameter for how the exact
         * legacy VST3 ParamIDs are reproduced.
         */
        void build_midi_ctl_params();

        /*
         * The controller numbers that have an automatable parameter, handed to
         * the VST3 IMidiMapping (create_vst3_midi_mapping_extensions) so it only
         * advertises controllers the plugin exposes. Kept in step with
         * build_midi_ctl_params().
         */
        static std::array<bool, MIDI_CTL_COUNT>
            collect_supported_midi_controllers();

        void add_midi_ctl_param(
            Midi::Channel const channel,
            int const controller_number,
            Synth::ControllerId const name_source,
            double const default_normalized
        );

        /* Forward any changed automation parameter to the Synth. */
        void dispatch_parameter_automation() noexcept;

        static Midi::Byte float_to_midi_byte(float const value) noexcept;
        static Midi::Word float_to_midi_word(float const value) noexcept;

        Synth synth;
        Renderer renderer;
        Bank bank;
        int current_program;

        /*
         * Non-owning; the parameters are owned by the AudioProcessor's parameter
         * tree (addParameter()), which outlives this list.
         */
        std::vector<MidiCtlParameter*> midi_ctl_params;

        /* Owns the IMidiMapping we hand the VST3 host for its whole lifetime. */
        std::unique_ptr<juce::VST3ClientExtensions> vst3_extensions;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JS80PProcessor)
};

}

#endif

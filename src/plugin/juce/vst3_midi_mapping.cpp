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

/*
 * Compiled into EVERY plugin-format target (never the shared-code target), with
 * the real implementation guarded by JucePlugin_Build_VST3 — the same per-format
 * define JUCE's own plugin-client wrappers are built around; juce_add_plugin
 * sets it to 1/0 on each format target. Only the VST3 branch pulls in the
 * Steinberg VST3 SDK, whose interface-IID symbols (FUnknown::iid,
 * IMidiMapping::iid) are provided by JUCE's VST3 wrapper in the same link; the
 * other formats compile the nullptr branch, which keeps those symbols from
 * ending up undefined in their links (an IMidiMapping is never queried outside
 * a VST3 host anyway). The VST3 branch depends on nothing from the JS80P
 * DSP/GUI headers — only JUCE and the SDK — so the VST3 target needs only the
 * SDK include path, no extra JS80P compile definitions.
 */

#include "plugin/juce/vst3_midi_mapping.hpp"

#if JucePlugin_Build_VST3

#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstmidicontrollers.h>
#include <pluginterfaces/vst/vsttypes.h>


namespace JS80P
{

namespace
{

using namespace Steinberg;

static_assert(
    MIDI_CTL_COUNT == Vst::kCountCtrlNumber,
    "MIDI_CTL_COUNT must match Steinberg's Vst::kCountCtrlNumber"
);

/* MIDI has exactly 16 channels; the host passes a 0-based channel index. */
constexpr int MIDI_CHANNELS = 16;


/**
 * \brief \c IMidiMapping reproducing the legacy JS80P VST3 controller mapping:
 *        a (channel, CC / pitch-bend / channel-pressure) triple maps to the
 *        parameter whose ParamID is \c (channel<<8)|controller .
 *
 * The plugin owns exactly one instance for its lifetime, so the FUnknown
 * reference count is a no-op: the object is never heap-managed by the host and
 * must never delete itself.
 */
class MidiMapping : public Vst::IMidiMapping
{
    public:
        explicit MidiMapping(
                std::array<bool, MIDI_CTL_COUNT> const& supported_controllers
        )
            : supported_controllers(supported_controllers)
        {
        }

        tresult PLUGIN_API getMidiControllerAssignment(
                int32 const bus_index,
                int16 const channel,
                Vst::CtrlNumber const controller_number,
                Vst::ParamID& id
        ) override {
            if (
                    bus_index == 0
                    && channel >= 0
                    && channel < MIDI_CHANNELS
                    && controller_number >= 0
                    && controller_number < (Vst::CtrlNumber)MIDI_CTL_COUNT
                    && supported_controllers[(size_t)controller_number]
            ) {
                /*
                 * MUST match JS80P::MidiCtlParameter::param_id (and the legacy
                 * VST3 plugin): ParamID = (channel << 8) | controller.
                 */
                id = (
                    ((Vst::ParamID)channel << 8)
                    | (Vst::ParamID)(controller_number & 0xff)
                );

                return kResultTrue;
            }

            return kResultFalse;
        }

        tresult PLUGIN_API queryInterface(
                TUID const iid,
                void** const obj
        ) override {
            if (
                    FUnknownPrivate::iidEqual(iid, Vst::IMidiMapping_iid)
                    || FUnknownPrivate::iidEqual(iid, FUnknown_iid)
            ) {
                *obj = this;
                addRef();

                return kResultTrue;
            }

            *obj = nullptr;

            return kNoInterface;
        }

        /* Owned by the processor; reference counting is intentionally inert. */
        uint32 PLUGIN_API addRef() override
        {
            return 1000;
        }

        uint32 PLUGIN_API release() override
        {
            return 1000;
        }

    private:
        std::array<bool, MIDI_CTL_COUNT> const supported_controllers;
};


class MidiMappingExtensions : public juce::VST3ClientExtensions
{
    public:
        explicit MidiMappingExtensions(
                std::array<bool, MIDI_CTL_COUNT> const& supported_controllers
        )
            : midi_mapping(supported_controllers)
        {
        }

        int32_t queryIEditController(
                Steinberg::TUID const iid,
                void** const obj
        ) override {
            return (int32_t)midi_mapping.queryInterface(iid, obj);
        }

    private:
        MidiMapping midi_mapping;
};

}


std::unique_ptr<juce::VST3ClientExtensions> create_vst3_midi_mapping_extensions(
        std::array<bool, MIDI_CTL_COUNT> const& supported_controllers
) {
    return std::make_unique<MidiMappingExtensions>(supported_controllers);
}

}

#else /* !JucePlugin_Build_VST3 */

namespace JS80P
{

std::unique_ptr<juce::VST3ClientExtensions> create_vst3_midi_mapping_extensions(
        std::array<bool, MIDI_CTL_COUNT> const&
) {
    return nullptr;
}

}

#endif

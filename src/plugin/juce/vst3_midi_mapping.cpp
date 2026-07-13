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

#include <atomic>

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
 * Each instance is a self-owned, heap-allocated COM object with an honest
 * reference count: it is created (with a count of 1) each time the host queries
 * \c IMidiMapping and deletes itself when the host drops its last reference.
 * This is deliberately NOT tied to the plugin's lifetime — a host (e.g. REAPER)
 * can cache the interface pointer and only release it while clearing its VST3
 * data at shutdown, which happens AFTER the JUCE wrapper has already destroyed
 * the AudioProcessor (and hence anything the processor owned). An object whose
 * storage died with the processor would dangle there; because this one carries
 * its own copy of the controller table and frees itself on the final release,
 * it stays valid for exactly as long as the host holds it. (JUCE's own
 * controller also implements IMidiMapping with correct ref-counting; we
 * intentionally shadow it to hand out the legacy ParamID mapping instead —
 * "user-provided interfaces win in the wrapper", see the header.)
 */
class MidiMapping : public Vst::IMidiMapping
{
    public:
        explicit MidiMapping(
                std::array<bool, MIDI_CTL_COUNT> const& supported_controllers
        )
            : supported_controllers(supported_controllers),
            reference_count(1)
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

        uint32 PLUGIN_API addRef() override
        {
            return (uint32)(reference_count.fetch_add(1) + 1);
        }

        uint32 PLUGIN_API release() override
        {
            int32 const remaining = reference_count.fetch_sub(1) - 1;

            if (remaining == 0) {
                delete this;
            }

            return (uint32)remaining;
        }

    private:
        std::array<bool, MIDI_CTL_COUNT> const supported_controllers;
        std::atomic<int32> reference_count;
};


class MidiMappingExtensions : public juce::VST3ClientExtensions
{
    public:
        explicit MidiMappingExtensions(
                std::array<bool, MIDI_CTL_COUNT> const& supported_controllers
        )
            : supported_controllers(supported_controllers)
        {
        }

        int32_t queryIEditController(
                Steinberg::TUID const iid,
                void** const obj
        ) override {
            /*
             * Hand the host a fresh, self-owned MidiMapping (reference count 1)
             * rather than one embedded in this extension: JUCE passes the
             * returned pointer straight to the host without adding a reference
             * of its own, and the host may release it long after this extension
             * (owned by the AudioProcessor) is gone. The new object frees itself
             * on the host's final release() and never outlives its usefulness.
             * On a non-match nothing is allocated.
             */
            if (
                    FUnknownPrivate::iidEqual(iid, Vst::IMidiMapping_iid)
                    || FUnknownPrivate::iidEqual(iid, FUnknown_iid)
            ) {
                *obj = static_cast<Vst::IMidiMapping*>(
                    new MidiMapping(supported_controllers)
                );

                return (int32_t)kResultTrue;
            }

            *obj = nullptr;

            return (int32_t)kNoInterface;
        }

    private:
        std::array<bool, MIDI_CTL_COUNT> const supported_controllers;
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

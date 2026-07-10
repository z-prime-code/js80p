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

#ifndef JS80P__UI__PARAM_BRIDGE_HPP
#define JS80P__UI__PARAM_BRIDGE_HPP

#include <algorithm>

#include "js80p.hpp"
#include "synth.hpp"


namespace JS80P
{

/**
 * \brief Thin, GUI-toolkit-agnostic adapter over the existing Synth seam so new
 *        widgets can read/write any of the 724 parameters by id. No Synth
 *        changes: writes go through the lock-free message queue, reads use the
 *        atomic getters.
 */
class ParamBridge
{
    public:
        explicit ParamBridge(Synth& synth) noexcept
            : synth(synth)
        {
        }

        /** The wrapped Synth, for whole-patch operations (serialize / import /
         *  randomize) that go beyond single-parameter reads and writes. */
        Synth& get_synth() const noexcept
        {
            return synth;
        }

        /** Current value ratio [0, 1] (reflects modulation / host / GUI). */
        Number get_ratio(Synth::ParamId const id) const noexcept
        {
            return synth.get_param_ratio_atomic(id);
        }

        Number get_default_ratio(Synth::ParamId const id) const noexcept
        {
            return synth.get_param_default_ratio(id);
        }

        /** Queue a value change for the audio thread (ratio is clamped). */
        void set_ratio(Synth::ParamId const id, Number const ratio) noexcept
        {
            synth.push_message(
                Synth::MessageType::SET_PARAM,
                id,
                std::min(1.0, std::max(0.0, ratio)),
                0
            );
        }

        bool is_discrete(Synth::ParamId const id) const noexcept
        {
            return synth.is_discrete_param(id);
        }

        std::string const& param_name(Synth::ParamId const id) const noexcept
        {
            return synth.get_param_name(id);
        }

        /** Number of options for a discrete parameter (max value + 1). */
        int option_count(Synth::ParamId const id) const noexcept
        {
            return (int)synth.get_param_max_value(id) + 1;
        }

        /** Current integer value of a discrete parameter. */
        int get_discrete(Synth::ParamId const id) const noexcept
        {
            return (int)synth.byte_param_ratio_to_display_value(
                id, synth.get_param_ratio_atomic(id)
            );
        }

        void set_discrete(Synth::ParamId const id, int const value) noexcept
        {
            set_ratio(id, synth.discrete_param_value_to_ratio(id, (Byte)value));
        }

        /** Default integer value of a discrete parameter. */
        int get_discrete_default(Synth::ParamId const id) const noexcept
        {
            return (int)synth.byte_param_ratio_to_display_value(
                id, synth.get_param_default_ratio(id)
            );
        }

        /** The parameter's displayed value (Hz / % / dB / ...) at \c ratio. */
        Number display_value(
                Synth::ParamId const id, Number const ratio
        ) const noexcept {
            if (synth.is_discrete_param(id)) {
                return (Number)synth.byte_param_ratio_to_display_value(id, ratio);
            }

            return synth.float_param_ratio_to_display_value(id, ratio);
        }

        /** Inverse of display_value: the ratio whose display value is \c target
         *  (binary search; assumes the mapping is monotonic). */
        Number ratio_for_display(Synth::ParamId const id, Number const target) const noexcept
        {
            Number const at_min = display_value(id, 0.0);
            Number const at_max = display_value(id, 1.0);
            bool const increasing = at_max >= at_min;
            Number lo = 0.0;
            Number hi = 1.0;

            for (int i = 0; i != 40; ++i) {
                Number const mid = 0.5 * (lo + hi);
                if ((display_value(id, mid) < target) == increasing) lo = mid;
                else hi = mid;
            }

            return 0.5 * (lo + hi);
        }

        /** Assigned controller, or NONE when the knob drives the value itself. */
        Synth::ControllerId controller(Synth::ParamId const id) const noexcept
        {
            return synth.get_param_controller_id_atomic(id);
        }

        /** Assign (or, with NONE, clear) a controller for a parameter. */
        void assign_controller(
                Synth::ParamId const id, Synth::ControllerId const controller
        ) noexcept {
            synth.push_message(
                Synth::MessageType::ASSIGN_CONTROLLER, id, 0.0, (Byte)controller
            );
        }

    private:
        Synth& synth;
};

}

#endif

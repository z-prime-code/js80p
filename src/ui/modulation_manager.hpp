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

#ifndef JS80P__UI__MODULATION_MANAGER_HPP
#define JS80P__UI__MODULATION_MANAGER_HPP

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "js80p.hpp"
#include "synth.hpp"

#include "ui/modulation.hpp"
#include "ui/param_bridge.hpp"


namespace JS80P
{

/**
 * \brief Shape-grouping allocator over the flat modulator pools. Tracks which
 *        pool slot backs which destination, buckets slots that share a shape
 *        into groups (one on-screen editor each), allocates/clones fresh slots
 *        on assign, and keeps a group's shape in sync across its duplicates.
 *        See doc/z-gui.md 5-6. The engine is never modified.
 */
class ModulationManager
{
    public:
        struct Group {
            Modulation::Type type;
            int rep;                                  /* representative slot */
            std::vector<int> members;                 /* slots sharing the shape */
            std::vector<Synth::ParamId> destinations; /* params driven */
            /* one (slot, destination) per connection, ordered by slot */
            std::vector<std::pair<int, Synth::ParamId>> connections;
        };

        explicit ModulationManager(ParamBridge& bridge) noexcept
            : bridge(bridge), next_group_id(1)
        {
        }

        /** Re-read the engine's assignments and rebuild the group list. */
        void rescan();

        std::vector<Group> const& groups() const { return groups_; }

        int free_count(Modulation::Type const type) const;

        /**
         * \brief Assign a modulator to \c dest. If \c clone_from > 0 the shape is
         *        copied from that existing slot; otherwise a fresh default is
         *        used. The new slot's base range is set to \c base_ratio.
         *        Returns the slot index, or 0 if the pool is exhausted.
         */
        int assign(
            Synth::ParamId const dest,
            Modulation::Type const type,
            double const base_ratio,
            int const clone_from = 0
        );

        /**
         * \brief Assign a global source (macro / MIDI CC / mod wheel / ...) to
         *        \c dest through a fresh intermediate macro that carries the
         *        per-location range (its min/max). The source drives the
         *        intermediate's input. Returns the intermediate slot, or 0 if the
         *        macro pool is exhausted.
         */
        int assign_source(
            Synth::ParamId const dest,
            Synth::ControllerId const source,
            double const base_ratio
        );

        /**
         * \brief Assign a "random" source to \c dest through a fresh intermediate
         *        macro that has no input controller but full randomness
         *        (macro_rnd = 1.0), so it emits random values within the
         *        per-location range (its min/max). Returns the intermediate slot,
         *        or 0 if the macro pool is exhausted.
         */
        int assign_random(
            Synth::ParamId const dest,
            double const base_ratio
        );

        void unassign(Synth::ParamId const dest);

        /**
         * \brief True if the modulator driving \c dest shares its shape group
         *        with at least one other slot (i.e. it is a mirrored copy).
         */
        bool is_mirrored(Synth::ParamId const dest) const;

        /**
         * \brief Detach \c dest's modulator slot into its own fresh group so it
         *        stops mirroring the others and shows as a separate editor. The
         *        slot (and its values) are kept; only the grouping changes.
         */
        void split(Synth::ParamId const dest);

        /**
         * \brief Copy \c from_rep's shared shape onto \c dest's own slot and move
         *        that slot into \c from_rep's group, so the two mirror. \c dest
         *        keeps its own per-destination base range. No slot is allocated.
         */
        void merge(
            Synth::ParamId const dest,
            Modulation::Type const type,
            int const from_rep
        );

        /** Clear all grouping state (used when building a patch from scratch). */
        void reset();

        /** Reserve a set of slots as one group with a fresh id (for INIT). */
        void reserve_group(Modulation::Type const type, std::vector<int> const& slots);

    private:
        std::string shape_key(Modulation::Type const type, int const index) const;
        int allocate(Modulation::Type const type, bool const need_pw);
        void copy_shape(Modulation::Type const type, int const from, int const to);
        void reset_new_envelope(int const slot);
        void set_base(Modulation::Type const type, int const slot, double const base_ratio);

        bool is_used(Modulation::Type const type, int const index) const;

        /**
         * \brief Cascading GC over the modulator pools. A pool slot (never the 8
         *        performance macros) that no longer drives any Synth param is
         *        dead: the modulators feeding *its* own params are freed and the
         *        slot is dropped from the tracking state. Freeing a feeder can
         *        make it dead too, so the pass repeats until it stabilises.
         *        \c just_cleared is the destination unassign() has queued to NONE
         *        but whose atomic hasn't drained yet, so it is treated as NONE.
         */
        void collect_garbage(
            Synth::ParamId const just_cleared = Synth::ParamId::PARAM_ID_COUNT
        );

        ParamBridge& bridge;
        std::vector<Group> groups_;
        std::set<int> reserved;   /* type*100 + slot; local, pre-audio-thread */
        /* slot_key -> stable group id: membership persists across rescans so a
         * transient value difference (queued write not yet drained) can't split
         * a group mid-edit. New slots (e.g. from a loaded patch) join by shape. */
        std::map<int, int> slot_group;
        int next_group_id;
};

}

#endif

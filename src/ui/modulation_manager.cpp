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

#include "ui/modulation_manager.hpp"


namespace JS80P
{

static int slot_key(Modulation::Type const type, int const index)
{
    return (int)type * 100 + index;
}


static int quant(double const r)
{
    return (int)std::lround(r * 1000.0);
}


/* The params a slot owns that can carry an incoming modulation (i.e. the
 * places a *feeding* modulator plugs into). Freeing a dead slot means clearing
 * whichever of these currently have a controller. */
static void collect_slot_input_params(
        Modulation::Type const type, int const index,
        std::vector<Synth::ParamId>& out
) {
    out.clear();

    if (type == Modulation::MACRO) {
        out.push_back(Modulation::macro_in(index));
        out.push_back(Modulation::macro_min(index));
        out.push_back(Modulation::macro_max(index));
        return;
    }

    if (type == Modulation::ENVELOPE) {
        Synth::ParamId const ps[] = {
            Modulation::env_scl(index), Modulation::env_ini(index), Modulation::env_del(index),
            Modulation::env_atk(index), Modulation::env_pk(index),  Modulation::env_hld(index),
            Modulation::env_dec(index), Modulation::env_sus(index), Modulation::env_rel(index),
            Modulation::env_fin(index), Modulation::env_tin(index), Modulation::env_vin(index),
            Modulation::env_ash(index), Modulation::env_dsh(index), Modulation::env_rsh(index),
            Modulation::env_upd(index), Modulation::env_syn(index),
        };
        out.assign(std::begin(ps), std::end(ps));
        return;
    }

    /* LFO */
    Synth::ParamId const ps[] = {
        Modulation::lfo_frq(index), Modulation::lfo_phs(index), Modulation::lfo_min(index),
        Modulation::lfo_max(index), Modulation::lfo_amp(index), Modulation::lfo_dst(index),
        Modulation::lfo_rnd(index), Modulation::lfo_wav(index), Modulation::lfo_log(index),
        Modulation::lfo_cen(index), Modulation::lfo_syn(index), Modulation::lfo_aen(index),
    };
    out.assign(std::begin(ps), std::end(ps));

    if (Modulation::lfo_has_pw(index)) {
        out.push_back(Modulation::lfo_pw(index));
    }
}


std::string ModulationManager::shape_key(
        Modulation::Type const type, int const index
) const {
    /* Signature of a shared parameter: its value, unless it is modulated - then
     * follow the controller so two copies match when driven by the same source
     * with the same range (macro input + min/max), not by a stale value. */
    auto sig = [this](Synth::ParamId const p) -> std::string {
        Synth::ControllerId const c = bridge.controller(p);

        if (c == Synth::ControllerId::NONE) {
            return "v" + std::to_string(quant(bridge.get_ratio(p)));
        }

        Modulation::Type t;
        int idx;
        if (Modulation::decode(c, t, idx) && t == Modulation::MACRO) {
            return "m" + std::to_string((int)bridge.controller(Modulation::macro_in(idx)))
                + ":" + std::to_string(quant(bridge.get_ratio(Modulation::macro_min(idx))))
                + ":" + std::to_string(quant(bridge.get_ratio(Modulation::macro_max(idx))));
        }

        return "c" + std::to_string((int)c);
    };

    std::string key;

    if (type == Modulation::ENVELOPE) {
        Synth::ParamId const ps[] = {
            Modulation::env_scl(index), Modulation::env_del(index), Modulation::env_atk(index),
            Modulation::env_hld(index), Modulation::env_dec(index), Modulation::env_rel(index),
            Modulation::env_tin(index), Modulation::env_vin(index), Modulation::env_ash(index),
            Modulation::env_dsh(index), Modulation::env_rsh(index), Modulation::env_upd(index),
            Modulation::env_syn(index),
        };
        for (Synth::ParamId const p : ps) { key += sig(p); key += ';'; }

        /* Backwards-calculated sustain fraction (shared across copies even though
         * the absolute levels differ). */
        double const ini = bridge.get_ratio(Modulation::env_ini(index));
        double const pk = bridge.get_ratio(Modulation::env_pk(index));
        double const frac = std::fabs(pk - ini) > 1.0e-6
            ? (bridge.get_ratio(Modulation::env_sus(index)) - ini) / (pk - ini)
            : 0.0;
        key += "s" + std::to_string(quant(frac));
    } else if (type == Modulation::LFO) {
        Synth::ParamId const ps[] = {
            Modulation::lfo_wav(index), Modulation::lfo_frq(index), Modulation::lfo_phs(index),
            Modulation::lfo_dst(index), Modulation::lfo_rnd(index), Modulation::lfo_amp(index),
            Modulation::lfo_log(index), Modulation::lfo_cen(index), Modulation::lfo_syn(index),
            Modulation::lfo_aen(index),
        };
        for (Synth::ParamId const p : ps) { key += sig(p); key += ';'; }
        key += Modulation::lfo_has_pw(index) ? sig(Modulation::lfo_pw(index)) : "nopw";
    }

    return key;
}


bool ModulationManager::is_used(Modulation::Type const type, int const index) const
{
    if (reserved.count(slot_key(type, index)) != 0) {
        return true;
    }

    for (int pid = 0; pid != (int)Synth::ParamId::PARAM_ID_COUNT; ++pid) {
        Modulation::Type t;
        int i;

        if (Modulation::decode(bridge.controller((Synth::ParamId)pid), t, i)
                && t == type && i == index) {
            return true;
        }
    }

    return false;
}


void ModulationManager::rescan()
{
    /* slot_key -> destinations, from the live engine assignments. */
    std::map<int, std::vector<Synth::ParamId>> used;

    for (int pid = 0; pid != (int)Synth::ParamId::PARAM_ID_COUNT; ++pid) {
        Modulation::Type t;
        int i;

        if (Modulation::decode(bridge.controller((Synth::ParamId)pid), t, i)) {
            used[slot_key(t, i)].push_back((Synth::ParamId)pid);
        }
    }

    /* Drop slots that are no longer assigned (and not pending). */
    for (std::map<int, int>::iterator it = slot_group.begin(); it != slot_group.end(); ) {
        if (used.count(it->first) == 0 && reserved.count(it->first) == 0) {
            it = slot_group.erase(it);
        } else {
            ++it;
        }
    }

    /* Give each newly-seen slot a group id: match an existing group's shape
     * (for loaded patches) or start a fresh group. */
    for (std::map<int, std::vector<Synth::ParamId>>::const_iterator it = used.begin();
            it != used.end(); ++it) {
        int const sk = it->first;

        if (slot_group.count(sk) != 0) {
            continue;
        }

        Modulation::Type const type = (Modulation::Type)(sk / 100);
        std::string const key = shape_key(type, sk % 100);
        int gid = -1;

        for (std::map<int, int>::const_iterator og = slot_group.begin();
                og != slot_group.end(); ++og) {
            int const osk = og->first;

            if ((Modulation::Type)(osk / 100) == type
                    && shape_key(type, osk % 100) == key) {
                gid = og->second;
                break;
            }
        }

        slot_group[sk] = gid < 0 ? next_group_id++ : gid;
    }

    /* Build the group list from the persistent membership map. */
    std::map<int, Group> by_gid;

    for (std::map<int, int>::const_iterator it = slot_group.begin();
            it != slot_group.end(); ++it) {
        int const sk = it->first;
        Modulation::Type const type = (Modulation::Type)(sk / 100);
        int const index = sk % 100;

        Group& g = by_gid[it->second];
        g.type = type;
        g.members.push_back(index);

        std::map<int, std::vector<Synth::ParamId>>::const_iterator u = used.find(sk);
        if (u != used.end()) {
            for (Synth::ParamId const dest : u->second) {
                g.destinations.push_back(dest);
                g.connections.push_back({ index, dest });
            }
        }
    }

    groups_.clear();

    for (std::map<int, Group>::iterator it = by_gid.begin(); it != by_gid.end(); ++it) {
        Group& g = it->second;
        std::sort(g.members.begin(), g.members.end());
        std::sort(g.connections.begin(), g.connections.end());
        g.rep = g.members.empty() ? 0 : g.members.front();
        groups_.push_back(g);
    }

    /* Stable order by type then representative slot. */
    std::sort(groups_.begin(), groups_.end(), [](Group const& a, Group const& b) {
        return a.type != b.type ? a.type < b.type : a.rep < b.rep;
    });
}


int ModulationManager::free_count(Modulation::Type const type) const
{
    int n = 0;

    for (int i = Modulation::pool_first(type); i <= Modulation::pool_last(type); ++i) {
        if (!is_used(type, i)) {
            ++n;
        }
    }

    return n;
}


int ModulationManager::allocate(Modulation::Type const type, bool const need_pw)
{
    /* For an LFO that must carry a pulse width, prefer an odd slot. */
    if (type == Modulation::LFO && need_pw) {
        for (int i = 1; i <= Modulation::LFO_COUNT; ++i) {
            if (Modulation::lfo_has_pw(i) && !is_used(type, i)) {
                return i;
            }
        }
    }

    for (int i = Modulation::pool_first(type); i <= Modulation::pool_last(type); ++i) {
        if (!is_used(type, i)) {
            return i;
        }
    }

    return 0;
}


void ModulationManager::copy_shape(
        Modulation::Type const type, int const from, int const to
) {
    auto copy = [this](Synth::ParamId const a, Synth::ParamId const b) {
        bridge.set_ratio(a, bridge.get_ratio(b));
    };

    if (type == Modulation::ENVELOPE) {
        /* Everything shared by the group (all but the per-destination levels
         * INI/PK/SUS/FIN), so a copy matches its source completely. */
        copy(Modulation::env_scl(to), Modulation::env_scl(from));
        copy(Modulation::env_del(to), Modulation::env_del(from));
        copy(Modulation::env_atk(to), Modulation::env_atk(from));
        copy(Modulation::env_hld(to), Modulation::env_hld(from));
        copy(Modulation::env_dec(to), Modulation::env_dec(from));
        copy(Modulation::env_rel(to), Modulation::env_rel(from));
        copy(Modulation::env_tin(to), Modulation::env_tin(from));
        copy(Modulation::env_vin(to), Modulation::env_vin(from));
        copy(Modulation::env_ash(to), Modulation::env_ash(from));
        copy(Modulation::env_dsh(to), Modulation::env_dsh(from));
        copy(Modulation::env_rsh(to), Modulation::env_rsh(from));
        copy(Modulation::env_upd(to), Modulation::env_upd(from));
        copy(Modulation::env_syn(to), Modulation::env_syn(from));
    } else if (type == Modulation::LFO) {
        copy(Modulation::lfo_wav(to), Modulation::lfo_wav(from));
        copy(Modulation::lfo_frq(to), Modulation::lfo_frq(from));
        copy(Modulation::lfo_phs(to), Modulation::lfo_phs(from));
        copy(Modulation::lfo_dst(to), Modulation::lfo_dst(from));
        copy(Modulation::lfo_rnd(to), Modulation::lfo_rnd(from));
        copy(Modulation::lfo_amp(to), Modulation::lfo_amp(from));
        copy(Modulation::lfo_log(to), Modulation::lfo_log(from));
        copy(Modulation::lfo_cen(to), Modulation::lfo_cen(from));
        copy(Modulation::lfo_syn(to), Modulation::lfo_syn(from));
        copy(Modulation::lfo_aen(to), Modulation::lfo_aen(from));

        if (Modulation::lfo_has_pw(from) && Modulation::lfo_has_pw(to)) {
            copy(Modulation::lfo_pw(to), Modulation::lfo_pw(from));
        }
    }
}


void ModulationManager::reset_new_envelope(int const slot)
{
    /* A freshly-allocated envelope pool slot can carry stale values from a
     * previously-freed assignment, so a brand-new (non-cloned) envelope would
     * otherwise inherit whatever delay / times / curves the slot last held.
     * Reset the shared shape params to their engine defaults, then apply the
     * standard new-envelope times (matching INIT's setup_env): 0.001 s attack,
     * 0 hold, 1 s decay, 0.1 s release. The per-destination levels
     * (INI / PK / SUS / FIN) are set separately by set_base + the depth write. */
    Synth::ParamId const shape[] = {
        Modulation::env_scl(slot), Modulation::env_del(slot), Modulation::env_atk(slot),
        Modulation::env_hld(slot), Modulation::env_dec(slot), Modulation::env_rel(slot),
        Modulation::env_tin(slot), Modulation::env_vin(slot), Modulation::env_ash(slot),
        Modulation::env_dsh(slot), Modulation::env_rsh(slot), Modulation::env_upd(slot),
        Modulation::env_syn(slot),
    };

    for (Synth::ParamId const p : shape) {
        bridge.set_ratio(p, bridge.get_default_ratio(p));
    }

    bridge.set_ratio(Modulation::env_atk(slot), bridge.ratio_for_display(Modulation::env_atk(slot), 0.001));
    bridge.set_ratio(Modulation::env_hld(slot), 0.0);
    bridge.set_ratio(Modulation::env_dec(slot), bridge.ratio_for_display(Modulation::env_dec(slot), 1.0));
    bridge.set_ratio(Modulation::env_rel(slot), bridge.ratio_for_display(Modulation::env_rel(slot), 0.1));
}


void ModulationManager::set_base(
        Modulation::Type const type, int const slot, double const base_ratio
) {
    if (type == Modulation::ENVELOPE) {
        bridge.set_ratio(Modulation::env_ini(slot), base_ratio);
        bridge.set_ratio(Modulation::env_sus(slot), base_ratio);
        bridge.set_ratio(Modulation::env_fin(slot), base_ratio);
    } else if (type == Modulation::LFO) {
        bridge.set_ratio(Modulation::lfo_min(slot), base_ratio);
    } else {
        bridge.set_ratio(Modulation::macro_min(slot), base_ratio);
    }
}


int ModulationManager::assign(
        Synth::ParamId const dest,
        Modulation::Type const type,
        double const base_ratio,
        int const clone_from
) {
    bool const need_pw =
        type == Modulation::LFO && clone_from > 0 && Modulation::lfo_has_pw(clone_from);

    int const slot = allocate(type, need_pw);

    if (slot == 0) {
        return 0;   /* pool exhausted */
    }

    if (clone_from > 0) {
        copy_shape(type, clone_from, slot);
    } else if (type == Modulation::ENVELOPE) {
        reset_new_envelope(slot);
    }

    set_base(type, slot, base_ratio);

    /* default depth: reach half-scale above the base */
    double const target = juce::jlimit(0.0, 1.0, base_ratio + 0.5);
    bridge.set_ratio(Modulation::depth_param(type, slot), target);

    bridge.assign_controller(dest, Modulation::controller_id(type, slot));

    int const sk = slot_key(type, slot);
    reserved.insert(sk);

    /* Join the cloned group, or start a new one. */
    int const src = clone_from > 0 ? slot_key(type, clone_from) : 0;
    slot_group[sk] = (src != 0 && slot_group.count(src) != 0)
        ? slot_group[src]
        : next_group_id++;

    rescan();

    return slot;
}


int ModulationManager::assign_source(
        Synth::ParamId const dest,
        Synth::ControllerId const source,
        double const base_ratio
) {
    int const slot = allocate(Modulation::MACRO, false);

    if (slot == 0) {
        return 0;
    }

    /* The intermediate macro's input follows the source; its min/max is the
     * unique range at this destination. Clear randomness in case this pool slot
     * was previously used as a "Random" source. */
    bridge.assign_controller(Modulation::macro_in(slot), source);
    bridge.set_ratio(Modulation::macro_rnd(slot), 0.0);
    bridge.set_ratio(Modulation::macro_min(slot), base_ratio);
    bridge.set_ratio(Modulation::macro_max(slot), juce::jlimit(0.0, 1.0, base_ratio + 0.5));
    bridge.assign_controller(dest, Modulation::controller_id(Modulation::MACRO, slot));

    int const sk = slot_key(Modulation::MACRO, slot);
    reserved.insert(sk);
    slot_group[sk] = next_group_id++;
    rescan();

    return slot;
}


int ModulationManager::assign_random(
        Synth::ParamId const dest,
        double const base_ratio
) {
    int const slot = allocate(Modulation::MACRO, false);

    if (slot == 0) {
        return 0;
    }

    /* Full randomness makes the macro emit random values within its per-location
     * range (min/max). Its input follows Osc 2's output peak so a fresh random
     * value is drawn as the sound re-triggers, rather than staying frozen. */
    bridge.assign_controller(Modulation::macro_in(slot), Synth::ControllerId::OSC_2_PEAK);
    bridge.set_ratio(Modulation::macro_rnd(slot), 1.0);
    bridge.set_ratio(Modulation::macro_min(slot), base_ratio);
    bridge.set_ratio(Modulation::macro_max(slot), juce::jlimit(0.0, 1.0, base_ratio + 0.5));
    bridge.assign_controller(dest, Modulation::controller_id(Modulation::MACRO, slot));

    int const sk = slot_key(Modulation::MACRO, slot);
    reserved.insert(sk);
    slot_group[sk] = next_group_id++;
    rescan();

    return slot;
}


void ModulationManager::reset()
{
    reserved.clear();
    slot_group.clear();
    groups_.clear();
    next_group_id = 1;
}


void ModulationManager::reserve_group(
        Modulation::Type const type, std::vector<int> const& slots
) {
    int const gid = next_group_id++;

    for (int const s : slots) {
        int const sk = slot_key(type, s);
        reserved.insert(sk);
        slot_group[sk] = gid;
    }
}


void ModulationManager::collect_garbage(Synth::ParamId const just_cleared)
{
    int const n = (int)Synth::ParamId::PARAM_ID_COUNT;

    /* Snapshot of the live controller assignments. The cascade mutates this
     * snapshot as it frees feeders, so it converges in one call regardless of
     * when the queued writes actually drain on the audio thread. */
    std::vector<Synth::ControllerId> ctl((size_t)n);

    for (int pid = 0; pid != n; ++pid) {
        ctl[(size_t)pid] = bridge.controller((Synth::ParamId)pid);
    }

    /* unassign() has just queued this destination to NONE; the atomic hasn't
     * caught up, so honour the removal here. */
    if ((int)just_cleared < n) {
        ctl[(size_t)just_cleared] = Synth::ControllerId::NONE;
    }

    bool changed = true;

    while (changed) {
        changed = false;

        /* Pool slots that still drive at least one param (from the snapshot). */
        std::set<int> live;

        for (int pid = 0; pid != n; ++pid) {
            Modulation::Type t;
            int i;

            if (Modulation::decode(ctl[(size_t)pid], t, i)) {
                live.insert(slot_key(t, i));
            }
        }

        Modulation::Type const types[] = {
            Modulation::ENVELOPE, Modulation::LFO, Modulation::MACRO
        };

        for (Modulation::Type const type : types) {
            /* pool_first(MACRO) == 9, so the 8 performance macros are never GC'd. */
            for (int i = Modulation::pool_first(type); i <= Modulation::pool_last(type); ++i) {
                int const sk = slot_key(type, i);

                if (live.count(sk) != 0) {
                    continue;   /* still drives something -> keep it */
                }

                bool const tracked =
                    reserved.count(sk) != 0 || slot_group.count(sk) != 0;

                std::vector<Synth::ParamId> inputs;
                collect_slot_input_params(type, i, inputs);

                bool has_input = false;

                for (Synth::ParamId const p : inputs) {
                    if (ctl[(size_t)p] != Synth::ControllerId::NONE) {
                        has_input = true;
                        break;
                    }
                }

                if (!tracked && !has_input) {
                    continue;   /* dead but nothing to reclaim */
                }

                /* Garbage: free the modulators feeding this dead slot (which may
                 * turn them into garbage on the next pass) and forget the slot. */
                for (Synth::ParamId const p : inputs) {
                    if (ctl[(size_t)p] != Synth::ControllerId::NONE) {
                        bridge.assign_controller(p, Synth::ControllerId::NONE);
                        ctl[(size_t)p] = Synth::ControllerId::NONE;
                    }
                }

                reserved.erase(sk);
                slot_group.erase(sk);
                changed = true;
            }
        }
    }

    rescan();
}


bool ModulationManager::is_mirrored(Synth::ParamId const dest) const
{
    Modulation::Type type;
    int index;

    if (!Modulation::decode(bridge.controller(dest), type, index)) {
        return false;
    }

    std::map<int, int>::const_iterator const it = slot_group.find(slot_key(type, index));

    if (it == slot_group.end()) {
        return false;
    }

    int const gid = it->second;
    int count = 0;

    for (std::map<int, int>::const_iterator og = slot_group.begin(); og != slot_group.end(); ++og) {
        if (og->second == gid) {
            ++count;
        }
    }

    return count > 1;
}


void ModulationManager::split(Synth::ParamId const dest)
{
    Modulation::Type type;
    int index;

    if (!Modulation::decode(bridge.controller(dest), type, index)) {
        return;
    }

    /* A fresh group id detaches this slot from its mirror group; membership is
     * stable across rescans, so the identical shape no longer re-merges it. */
    slot_group[slot_key(type, index)] = next_group_id++;
    rescan();
}


void ModulationManager::merge(
        Synth::ParamId const dest, Modulation::Type const type, int const from_rep
) {
    Modulation::Type dtype;
    int index;

    if (!Modulation::decode(bridge.controller(dest), dtype, index) || dtype != type) {
        return;
    }

    /* Copy the shared shape onto this slot (per-destination levels are left
     * alone) and join the source group so the two mirror from now on. */
    copy_shape(type, from_rep, index);

    std::map<int, int>::const_iterator const it = slot_group.find(slot_key(type, from_rep));
    slot_group[slot_key(type, index)] =
        it != slot_group.end() ? it->second : next_group_id++;

    rescan();
}


void ModulationManager::unassign(Synth::ParamId const dest)
{
    Modulation::Type type;
    int index;

    if (Modulation::decode(bridge.controller(dest), type, index)) {
        int const sk = slot_key(type, index);
        reserved.erase(sk);
        slot_group.erase(sk);

        /* Release an intermediate macro's input too. */
        if (type == Modulation::MACRO) {
            bridge.assign_controller(Modulation::macro_in(index), Synth::ControllerId::NONE);
        }
    }

    bridge.assign_controller(dest, Synth::ControllerId::NONE);

    /* Reclaim any modulators left dangling by this removal (e.g. an LFO that
     * only modulated a param of the slot just freed). Cascades. */
    collect_garbage(dest);
}

}

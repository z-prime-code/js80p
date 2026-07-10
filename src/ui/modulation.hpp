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

#ifndef JS80P__UI__MODULATION_HPP
#define JS80P__UI__MODULATION_HPP

#include <juce_graphics/juce_graphics.h>

#include "js80p.hpp"
#include "synth.hpp"

#include "ui/theme.hpp"


namespace JS80P
{

/**
 * \brief Pure id/colour maps for the pooled modulators (envelope / LFO / macro).
 *        See doc/z-gui.md 5-6. The engine is untouched; these just translate
 *        between the GUI's (type, 1-based slot) and the engine's ParamId /
 *        ControllerId. Strides are irregular, hence the explicit tables.
 */
namespace Modulation
{
    enum Type { ENVELOPE, LFO, MACRO };

    /* Which modulator kinds a destination accepts (matches the engine's
     * per-param capability): oscillator/filter = all; envelope params take only
     * macros; LFO params take LFOs + macros. Macros are always reached through
     * the "Modulate by" menu (never offered as a copyable group). */
    enum Cap { CAP_ENV = 1, CAP_LFO = 2, CAP_MACRO = 4, CAP_ALL = 7 };

    static constexpr int ENVELOPE_COUNT = 12;
    static constexpr int LFO_COUNT = 8;
    /* Macros 1-8 are the top performance strip; 9-30 back auto-modulation. */
    static constexpr int MACRO_POOL_FIRST = 9;
    static constexpr int MACRO_POOL_LAST = 30;

    inline int pool_first(Type const t) { return t == MACRO ? MACRO_POOL_FIRST : 1; }
    inline int pool_last(Type const t)
    {
        return t == ENVELOPE ? ENVELOPE_COUNT : (t == LFO ? LFO_COUNT : MACRO_POOL_LAST);
    }

    inline Synth::ParamId pid(int const v) { return (Synth::ParamId)v; }

    /* ---- ControllerId <-> (type, 1-based slot) ---- */

    inline Synth::ControllerId controller_id(Type const type, int const i)
    {
        if (type == LFO) {
            return (Synth::ControllerId)((int)Synth::ControllerId::LFO_1 + i - 1);
        }

        if (type == ENVELOPE) {
            return i <= 6
                ? (Synth::ControllerId)((int)Synth::ControllerId::ENVELOPE_1 + i - 1)
                : (Synth::ControllerId)((int)Synth::ControllerId::ENVELOPE_7 + i - 7);
        }

        if (i <= 10) return (Synth::ControllerId)((int)Synth::ControllerId::MACRO_1 + i - 1);
        if (i <= 20) return (Synth::ControllerId)((int)Synth::ControllerId::MACRO_11 + i - 11);
        return (Synth::ControllerId)((int)Synth::ControllerId::MACRO_21 + i - 21);
    }

    inline bool decode(Synth::ControllerId const id, Type& type, int& index)
    {
        int const v = (int)id;

        if (v >= (int)Synth::ControllerId::LFO_1 && v <= (int)Synth::ControllerId::LFO_8) {
            type = LFO; index = v - (int)Synth::ControllerId::LFO_1 + 1; return true;
        }
        if (v >= (int)Synth::ControllerId::ENVELOPE_1 && v <= (int)Synth::ControllerId::ENVELOPE_6) {
            type = ENVELOPE; index = v - (int)Synth::ControllerId::ENVELOPE_1 + 1; return true;
        }
        if (v >= (int)Synth::ControllerId::ENVELOPE_7 && v <= (int)Synth::ControllerId::ENVELOPE_12) {
            type = ENVELOPE; index = v - (int)Synth::ControllerId::ENVELOPE_7 + 7; return true;
        }
        if (v >= (int)Synth::ControllerId::MACRO_1 && v <= (int)Synth::ControllerId::MACRO_10) {
            type = MACRO; index = v - (int)Synth::ControllerId::MACRO_1 + 1; return true;
        }
        if (v >= (int)Synth::ControllerId::MACRO_11 && v <= (int)Synth::ControllerId::MACRO_20) {
            type = MACRO; index = v - (int)Synth::ControllerId::MACRO_11 + 11; return true;
        }
        if (v >= (int)Synth::ControllerId::MACRO_21 && v <= (int)Synth::ControllerId::MACRO_30) {
            type = MACRO; index = v - (int)Synth::ControllerId::MACRO_21 + 21; return true;
        }
        return false;
    }

    /* ---- Envelope params (stride 12 from N1SCL = 339) ---- */

    inline Synth::ParamId env_scl(int const i) { return pid(339 + 12 * (i - 1)); }
    inline Synth::ParamId env_ini(int const i) { return pid(340 + 12 * (i - 1)); }
    inline Synth::ParamId env_del(int const i) { return pid(341 + 12 * (i - 1)); }
    inline Synth::ParamId env_atk(int const i) { return pid(342 + 12 * (i - 1)); }
    inline Synth::ParamId env_pk (int const i) { return pid(343 + 12 * (i - 1)); }
    inline Synth::ParamId env_hld(int const i) { return pid(344 + 12 * (i - 1)); }
    inline Synth::ParamId env_dec(int const i) { return pid(345 + 12 * (i - 1)); }
    inline Synth::ParamId env_sus(int const i) { return pid(346 + 12 * (i - 1)); }
    inline Synth::ParamId env_rel(int const i) { return pid(347 + 12 * (i - 1)); }
    inline Synth::ParamId env_fin(int const i) { return pid(348 + 12 * (i - 1)); }

    inline Synth::ParamId env_tin(int const i) { return pid(349 + 12 * (i - 1)); }
    inline Synth::ParamId env_vin(int const i) { return pid(350 + 12 * (i - 1)); }

    /* Envelope shape curves + mode (stride-1 blocks). */
    inline Synth::ParamId env_upd(int const i) { return pid(602 + (i - 1)); }
    inline Synth::ParamId env_syn(int const i) { return pid(637 + (i - 1)); }
    inline Synth::ParamId env_ash(int const i) { return pid(649 + (i - 1)); }
    inline Synth::ParamId env_dsh(int const i) { return pid(661 + (i - 1)); }
    inline Synth::ParamId env_rsh(int const i) { return pid(673 + (i - 1)); }

    /* ---- LFO params (irregular main block: only odd LFOs have PW) ---- */

    inline int lfo_frq_base(int const i)
    {
        static int const base[9] = { 0, 484, 491, 499, 506, 514, 521, 529, 536 };
        return base[i];
    }

    inline bool lfo_has_pw(int const i) { return (i % 2) == 1; }
    inline Synth::ParamId lfo_pw (int const i) { return pid(lfo_frq_base(i) - 1); }
    inline Synth::ParamId lfo_frq(int const i) { return pid(lfo_frq_base(i)); }
    inline Synth::ParamId lfo_phs(int const i) { return pid(lfo_frq_base(i) + 1); }
    inline Synth::ParamId lfo_min(int const i) { return pid(lfo_frq_base(i) + 2); }
    inline Synth::ParamId lfo_max(int const i) { return pid(lfo_frq_base(i) + 3); }
    inline Synth::ParamId lfo_amp(int const i) { return pid(lfo_frq_base(i) + 4); }
    inline Synth::ParamId lfo_dst(int const i) { return pid(lfo_frq_base(i) + 5); }
    inline Synth::ParamId lfo_rnd(int const i) { return pid(lfo_frq_base(i) + 6); }
    inline Synth::ParamId lfo_wav(int const i) { return pid(555 + (i - 1)); }
    inline Synth::ParamId lfo_log(int const i) { return pid(563 + (i - 1)); }
    inline Synth::ParamId lfo_cen(int const i) { return pid(571 + (i - 1)); }
    inline Synth::ParamId lfo_syn(int const i) { return pid(579 + (i - 1)); }
    inline Synth::ParamId lfo_aen(int const i) { return pid(629 + (i - 1)); }

    /* ---- Macro params (stride 7 from M1MID = 129) ---- */

    inline Synth::ParamId macro_in (int const i) { return pid(130 + 7 * (i - 1)); }
    inline Synth::ParamId macro_min(int const i) { return pid(131 + 7 * (i - 1)); }
    inline Synth::ParamId macro_max(int const i) { return pid(132 + 7 * (i - 1)); }
    inline Synth::ParamId macro_dst(int const i) { return pid(134 + 7 * (i - 1)); }
    inline Synth::ParamId macro_rnd(int const i) { return pid(135 + 7 * (i - 1)); }

    /* Distortion curve selector (M1DCV = 685, stride 1). */
    inline Synth::ParamId macro_dcv(int const i) { return pid(685 + (i - 1)); }

    /* ---- Per-destination range: base parameter(s) and depth (top) parameter.
     *      base = env INI/SUS/FIN or LFO/macro MIN; depth target = env PK or
     *      LFO/macro MAX. ---- */

    inline Synth::ParamId depth_param(Type const type, int const i)
    {
        return type == ENVELOPE ? env_pk(i) : (type == LFO ? lfo_max(i) : macro_max(i));
    }

    inline juce::Colour colour(Type const type)
    {
        switch (type) {
            case ENVELOPE: return Theme::ENV;
            case LFO:      return Theme::LFO;
            default:       return Theme::MACRO;
        }
    }

    inline char const* prefix(Type const type)
    {
        switch (type) {
            case ENVELOPE: return "E";
            case LFO:      return "L";
            default:       return "M";
        }
    }

    /**
     * \brief Colour for an assigned modulation, by source: envelope green, LFO
     *        blue, a macro/CC/wheel-fed intermediate magenta, and a per-voice
     *        "direct" source (velocity / note / aftertouch) orange.
     */
    inline juce::Colour assigned_colour(Type const type, Synth::ControllerId const macro_source)
    {
        if (type == ENVELOPE) return Theme::ENV;
        if (type == LFO)      return Theme::LFO;

        int const v = (int)macro_source;
        if (v == 130 || v == 129 || v == 155) return Theme::MACRO;   /* vel/note/AT -> orange */
        return Theme::MIDI;                                          /* macro/CC/wheel -> magenta */
    }

    /** Short label for a modulation source controller (macro / MIDI / global). */
    inline juce::String source_short_name(Synth::ControllerId const id)
    {
        int const v = (int)id;

        if (v >= (int)Synth::ControllerId::MACRO_1 && v <= (int)Synth::ControllerId::MACRO_8) {
            return juce::String("M") + juce::String(v - (int)Synth::ControllerId::MACRO_1 + 1);
        }

        switch (v) {
            case 0:   return "-";
            case 1:   return "Mod";
            case 2:   return "Breath";
            case 4:   return "Foot";
            case 7:   return "Vol";
            case 10:  return "Pan";
            case 11:  return "Expr";
            case 64:  return "Sus";
            case 71:  return "Reso";
            case 74:  return "Cut";
            case 91:  return "Rev";
            case 93:  return "Cho";
            case 128: return "Pitch";
            case 129: return "Note";
            case 130: return "Vel";
            case 155: return "AT";
            case 156: return "Lrn";
            default:  return juce::String("CC") + juce::String(v);
        }
    }

    /**
     * \brief Friendlier destination label: oscillator params get the osc number
     *        as a suffix (MAMP -> AMP1, CAMP -> AMP2) and the four per-voice
     *        filters are numbered 1-4 (MF1->F1, MF2->F2, CF1->F3, CF2->F4), so
     *        e.g. CF1FRQ -> F3FRQ. Other names pass through unchanged.
     */
    inline juce::String display_dest_name(std::string const& raw)
    {
        juce::String s(raw.c_str());

        static char const* const fpre[4] = { "MF1", "MF2", "CF1", "CF2" };
        static char const* const fnum[4] = { "F1", "F2", "F3", "F4" };

        for (int i = 0; i != 4; ++i) {
            if (s.startsWith(fpre[i])) {
                return juce::String(fnum[i]) + s.substring(3);
            }
        }

        if (s.length() > 1 && (s[0] == 'M' || s[0] == 'C')) {
            juce::String const rest = s.substring(1);
            static char const* const suf[] = {
                "AMP", "VS", "WID", "PAN", "DTN", "FIN", "PRD", "PRT",
                "VOL", "FLD", "WFM", "PW", "SUB", "N", "DL"
            };

            for (char const* const x : suf) {
                if (rest == x) {
                    return rest + juce::String(s[0] == 'M' ? "1" : "2");
                }
            }
        }

        return s;
    }
}

}

#endif

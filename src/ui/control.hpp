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

#ifndef JS80P__UI__CONTROL_HPP
#define JS80P__UI__CONTROL_HPP

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "js80p.hpp"
#include "synth.hpp"

#include "ui/modulation.hpp"
#include "ui/modulation_manager.hpp"
#include "ui/param_bridge.hpp"


namespace JS80P
{

class ModBadge;
class ValuePopover;

/**
 * \brief A resolution-independent value control bound to one Synth parameter,
 *        the single component behind every knob/slider/dot in the new GUI.
 *
 *        Four render \ref Style s share one interaction + modulation core:
 *        ROTARY (dial), H_SLIDER / V_SLIDER (track + handle), and DOT (pie fill).
 *        When a modulator is assigned, the value sets the source's per-
 *        destination base (envelope INI/SUS/FIN, or LFO/macro MIN); a filled
 *        handle/dot at the reach end sets the signed range/depth; an outer ring
 *        (rotary/dot) or middle line (slider) shows where the modulation
 *        reaches. Right-click assigns; the free-floating badge drags the amount.
 *
 *        Cross-cutting options: \ref Size tier, label placement, value display
 *        (always / hover popover / none), Alt-click typed entry, an optional
 *        unassigned-source placeholder badge, an owned sub-control (e.g. a
 *        distortion-curve square), and value read/write hooks for controls whose
 *        value folds several params (the WET/DRY MIX).
 */
class Control : public juce::Component, public juce::SettableTooltipClient
{
    public:
        enum class Style { ROTARY, H_SLIDER, V_SLIDER, DOT };

        /** Scale tier: dial/track pixel size and font. */
        enum class Size { NORMAL, LARGE, SMALL, TINY };

        enum class LabelPos { NONE, TOP, LEFT };

        /* NONE   - no reserved value strip, value only via popover (bare / harmonic).
         * ALWAYS - reserve the value strip so the cell layout is preserved; the value
         *          is still shown only in the hover / drag popover, not inline.
         * POPOVER- no reserved strip (dots / sliders / macro cells), popover value. */
        enum class ValueDisplay { NONE, ALWAYS, POPOVER };

        Control(
            ParamBridge& bridge,
            Synth::ParamId const param_id,
            juce::String const& label,
            Style const style = Style::ROTARY,
            Size const size = Size::NORMAL
        );

        ~Control() override;

        void set_style(Style const s);
        void set_size(Size const s);

        /** Turn this control into a performance-macro editor for macro \c slot:
         *  the value is the macro's MIN, the modulation depth its MAX, and the
         *  badge selects the macro's input source (a CC / wheel / note source).
         *  The modulation styling (reach ring, magenta fill highlight) appears
         *  only once an input source is actually assigned. */
        void set_macro(int const slot);

        /** Make this control a modulation destination (right-click to assign). */
        void set_manager(ModulationManager* const manager);

        /** Restrict which modulator kinds may be assigned (Modulation::Cap). */
        void set_mod_caps(int const caps);

        /** Other parameters (grouped copies) that must receive the same
         *  modulator when this control is assigned. */
        void set_mirrors(std::vector<Synth::ParamId> params);

        /** Other parameters (grouped copies) that receive the same *value* on
         *  every direct (unassigned) edit — used by the envelope inaccuracy dots,
         *  which write one shared value to every group member. */
        void set_value_mirrors(std::vector<Synth::ParamId> params);

        /** Parameters that receive an *inverse* (1 - value) copy of the
         *  modulation: assigning a source here also drives each of these through a
         *  parallel macro whose range is the complement of this control's, and the
         *  ranges stay in sync as the base/depth change. Used by the WET/DRY MIX so
         *  a modulator crossfades DRY down as it sweeps WET up. */
        void set_inverse_mirrors(std::vector<Synth::ParamId> params);

        void set_center_value(double const display_value);

        /** Restrict the control's travel to an exponential frequency sweep
         *  between \c lo and \c hi Hz, with \c center Hz at mid-travel. Overrides
         *  the parameter's native range/skew for this control. */
        void set_freq_range(double const lo, double const hi, double const center);

        void set_semitone_snap(bool const on);
        void set_min_ratio(double const r);

        /** Show these option names instead of the raw index for a discrete
         *  parameter (e.g. filter / distortion type). */
        void set_discrete_labels(juce::StringArray labels);

        /** Use smaller label/value text so the control reads cleanly in a
         *  narrower medium-tier cell. Shorthand for set_size(SMALL). */
        void set_compact(bool const on);

        /** Draw only the dial: no caption above and no value readout below, so
         *  the circle fills the whole height. Used where the caption sits beside
         *  the control instead (e.g. the header OUT knob). Shorthand for
         *  label placement NONE + value display NONE. */
        void set_bare(bool const on);

        void set_label_placement(LabelPos const p);
        void set_value_display(ValueDisplay const d);

        /** Draw an empty outline chip (that opens the assign menu) even when
         *  nothing is assigned, so the modulation slot is always discoverable. */
        void set_source_placeholder(bool const on);

        /** Anchor the modulation badge to the right of the dial, vertically
         *  centred on it (like a DOT), instead of the default top-right corner.
         *  Used by the header OUT knob so its badge lines up with the knob's
         *  centre. */
        void set_badge_centred(bool const on);

        /** Own a small child control anchored beside the dial / slider end
         *  (e.g. a distortion-curve selector). \c refresh, if given, is called
         *  every frame so the child can track external param changes. */
        void set_sub_control(
            std::unique_ptr<juce::Component> child,
            std::function<void()> refresh = {}
        );

        /** Drive the value through custom read/write instead of the bound param
         *  directly (e.g. the combined WET/DRY MIX). \c format is an optional
         *  readout formatter for the value (falls back to the param formatter). */
        void set_value_hooks(
            std::function<double()> read,
            std::function<void(double)> write,
            std::function<juce::String(double)> format = {}
        );

        /** Value that a double-click resets to when value hooks are in use (the
         *  bound param's default is meaningless then). */
        void set_hook_default(double const d);

        void refresh();

        /** True once a modulator is assigned to this control's parameter. */
        bool is_assigned() const { return assigned; }

        /** The dial / track bounds in this control's own coordinates — i.e.
         *  excluding the caption strip. Used by the synth page to align its
         *  signal-flow arrows with a mix knob's circle (centre / bottom). */
        juce::Rectangle<int> control_bounds() const;

        /** Vertical drag distance (px) that spans the full 0..1 range; shared
         *  with the other value controls so they all feel the same. */
        static constexpr double DRAG_PIXELS_FULL_RANGE = 220.0;

        void resized() override;
        void paint(juce::Graphics& g) override;
        bool hitTest(int x, int y) override;
        void mouseDown(juce::MouseEvent const& event) override;
        void mouseDrag(juce::MouseEvent const& event) override;
        void mouseUp(juce::MouseEvent const& event) override;
        void mouseDoubleClick(juce::MouseEvent const& event) override;
        void mouseEnter(juce::MouseEvent const& event) override;
        void mouseExit(juce::MouseEvent const& event) override;
        void mouseWheelMove(
            juce::MouseEvent const& event,
            juce::MouseWheelDetails const& wheel
        ) override;

    protected:
        /* Subclass hooks (used by HarmonicSlider, which renders a bipolar bar but
         * reuses the whole value / modulation / badge core). The two geometry
         * helpers are virtual so a subclass can relocate the track and badge. */
        virtual juce::Rectangle<float> track_rect() const;    /* sliders */
        virtual juce::Rectangle<float> badge_rect() const;

        juce::Rectangle<int> body_bounds() const;    /* dial/track area after strips */
        juce::Point<float> handle_at(double const visual) const;  /* sliders */

        double mod_base() const { return base; }
        double mod_depth() const { return depth; }
        double value_ratio() const { return current_ratio(); }
        double to_visual(double const r) const { return ratio_to_visual(r); }
        /* Base (unassigned) or modulation (assigned) accent colour. */
        juce::Colour active_colour() const { return assigned ? mod_colour : accent(); }

    private:
        friend class ModBadge;
        friend class ValuePopover;

        static constexpr double WHEEL_STEP = 0.04;
        static constexpr float RING_BAND = 10.0f;   /* clickable band outside the circle */

        bool is_slider() const { return style == Style::H_SLIDER || style == Style::V_SLIDER; }
        bool is_rotary() const { return style == Style::ROTARY; }

        float font_size() const;
        int strip_h() const;   /* reserved caption/value strip height (0 when off) */

        juce::Rectangle<int> label_strip() const;

        bool over_ring(juce::Point<float> const p) const;
        bool over_depth_zone(juce::Point<float> const p) const;

        /**
         * \brief Position the free-floating badge component (a sibling brought to
         *        front) in parent coordinates, or hide it when unassigned.
         */
        void update_badge();
        bool badge_shown() const;

        /** Position the free-floating value popover (a sibling brought to front)
         *  above the control's title / dial / badge, or hide it. */
        void update_popover();
        bool popover_shown() const;
        bool title_on_knob() const;   /* caption rendered on the control itself */

        juce::String format_ratio(double const r) const;
        /* Badge caption: the source label uppercased and cropped to 3 chars so a
         * long name (e.g. "Breath") never overhangs the neighbouring control. */
        juce::String badge_text() const;
        void commit(double const new_ratio);

        double current_ratio() const;   /* value hooks aware */

        double ratio_to_visual(double const r) const;
        double visual_to_ratio(double const v) const;
        double ratio_for_display(double const target) const;

        /* Semitone snapping (DTN/FIN); fine == Ctrl disables the snap. */
        double snap_ratio(double const raw_ratio, double const from_visual, bool const fine) const;
        double snap_depth(double const raw_depth, double const from_depth, bool const fine) const;

        void update_assignment();
        void update_macro_assignment();
        void open_macro_source_menu();
        juce::Colour accent() const;   /* base (unassigned) highlight colour */
        void assign_mirrors(Modulation::Type const type, int const slot);
        void clear_mirrors();
        /* Inverse (crossfade) mirrors: parallel complement-range macros. */
        void assign_inverse_mirrors(Synth::ControllerId const source, double const base);
        void clear_inverse_mirrors();
        void sync_inverse_mirrors(double const primary_min, double const primary_max);
        void read_base_depth();
        void apply_base(double const b);
        void apply_depth(double const d);
        void open_assign_menu();

        /* Geometry (style-aware). */
        juce::Rectangle<float> knob_circle() const;   /* rotary + dot */

        /* Per-style painters. */
        void paint_rotary(juce::Graphics& g);
        void paint_dot(juce::Graphics& g);
        void paint_slider(juce::Graphics& g);

        /* Typed value entry (Alt-click). */
        void open_value_editor();
        void close_editor(bool const apply);

        ParamBridge& bridge;
        Synth::ParamId const param_id;
        juce::String const label;
        ModulationManager* manager;
        int mod_caps;
        std::vector<Synth::ParamId> mirrors;
        std::vector<Synth::ParamId> value_mirrors;
        std::vector<Synth::ParamId> inverse_mirrors;
        juce::StringArray discrete_labels;

        Style style;
        Size size_tier;
        LabelPos label_pos;
        ValueDisplay value_display;
        bool source_placeholder;
        bool macro_mode;   /* performance-macro editor (see set_macro) */

        std::function<double()> read_value_hook;
        std::function<void(double)> write_value_hook;
        std::function<juce::String(double)> format_value_hook;
        double hook_default;
        bool has_value_hooks() const { return (bool)read_value_hook; }

        std::unique_ptr<juce::Component> sub_control;
        std::function<void()> sub_refresh;
        std::unique_ptr<juce::TextEditor> editor;

        bool has_freq_range() const { return freq_lo > 0.0 && freq_hi > freq_lo; }

        double ratio;
        double skew;
        double freq_lo;    /* exponential frequency sweep bounds (Hz); <=0 = off */
        double freq_hi;
        double freq_skew;  /* v-space exponent placing the center at mid-travel */
        double min_ratio;
        double drag_start_visual;
        bool dragging;
        bool dragging_depth;
        double drag_start_depth;
        bool semitone_snap;
        bool compact;
        bool bare;
        bool centred_badge;   /* badge to the right, vertically centred (see set_badge_centred) */
        bool hover;

        bool assigned;
        Modulation::Type mod_type;
        int mod_slot;
        juce::String mod_label;   /* badge text: source name or slot */
        juce::Colour mod_colour;  /* badge/ring colour by source type */
        double base;
        double depth;   /* signed: target = clamp(base + depth) */

        std::unique_ptr<ModBadge> badge;
        std::unique_ptr<ValuePopover> popover;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Control)
};

}

#endif

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

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "ui/control.hpp"

#include "ui/theme.hpp"
#include "ui/value_popover.hpp"


namespace JS80P
{

static constexpr float START_ANGLE = juce::MathConstants<float>::pi * 1.2f;
static constexpr float END_ANGLE = juce::MathConstants<float>::pi * 2.8f;

static constexpr int STRIP = 14;          /* caption / value strip height */
static constexpr int LEFT_GUTTER = 34;    /* left-caption gutter width */
static constexpr float BADGE_H = 12.0f;   /* modulation badge / slider handle size */

/* The oscillator section's dial box (cell 72 high, less the 14+14 caption/value
 * strips): macro cells reserve no strips, so their dial is capped to this to read
 * at the same size as an oscillator knob. */
static constexpr int OSC_DIAL_BOX = 40;
/* Every pie DOT renders at the same size (the inaccuracy dots'), no matter how
 * big a cell it is given (e.g. the captioned MIX pie in an effect header). */
static constexpr float DOT_BOX = 16.0f;

/* Global sources assignable through an intermediate macro (label + ControllerId). */
static struct { char const* name; int id; } const CONTROL_SOURCES[] = {
    { "Macro 1", 131 }, { "Macro 2", 132 }, { "Macro 3", 133 }, { "Macro 4", 134 },
    { "Macro 5", 135 }, { "Macro 6", 136 }, { "Macro 7", 137 }, { "Macro 8", 138 },
    { "Mod wheel", 1 }, { "Breath", 2 }, { "Foot", 4 }, { "Volume", 7 },
    { "Pan", 10 }, { "Expression", 11 }, { "Sustain", 64 }, { "Cutoff CC74", 74 },
    { "Pitch wheel", 128 }, { "Note", 129 }, { "Velocity", 130 }, { "Aftertouch", 155 },
    { "MIDI Learn", 156 },
};
static constexpr int CONTROL_SOURCE_COUNT = (int)(sizeof(CONTROL_SOURCES) / sizeof(CONTROL_SOURCES[0]));

/* Macro-cell input sources (label + ControllerId), written straight to macro_in:
 * Learn arms MIDI-learn; the well-known CCs and non-CC sources follow. */
static struct { char const* name; int id; } const MACRO_SOURCES[] = {
    { "Learn", 156 }, { "-", 0 },
    { "Mod", 1 }, { "Breath", 2 }, { "Foot", 4 }, { "Vol", 7 }, { "Pan", 10 },
    { "Expr", 11 }, { "Sus", 64 }, { "Reso", 71 }, { "Cut", 74 }, { "Rev", 91 },
    { "Cho", 93 }, { "Pitch", 128 }, { "Note", 129 }, { "Vel", 130 }, { "AT", 155 },
};
static constexpr int MACRO_SOURCE_COUNT = (int)(sizeof(MACRO_SOURCES) / sizeof(MACRO_SOURCES[0]));

static float angle_of(double const r)
{
    return START_ANGLE + (float)r * (END_ANGLE - START_ANGLE);
}


/**
 * \brief The modulation badge, a free-floating sibling of the control kept in
 *        front of the other controls. Being its own component with real bounds,
 *        it is fully clickable and repaints cleanly even where it overhangs a
 *        narrow cell. Dragging it sets the modulation amount; right-click (or a
 *        click while it is an unassigned placeholder) opens the assign menu.
 */
class ModBadge : public juce::Component
{
    public:
        explicit ModBadge(Control& owner) : owner(owner), dragging(false), drag_start_depth(0.0)
        {
            setWantsKeyboardFocus(false);
        }

        void paint(juce::Graphics& g) override
        {
            juce::Rectangle<float> const r = getLocalBounds().toFloat();

            if (!owner.assigned) {
                /* Unassigned placeholder: a barely-there white fill chip (2.5%
                 * opacity, no outline) filling the same box an assigned badge
                 * would - just visible enough to reveal the modulation slot on
                 * every modulatable control. */
                g.setColour(juce::Colours::white.withAlpha(0.04f));
                g.fillRoundedRectangle(r, 3.0f);
                return;
            }

            juce::Colour const c = owner.mod_colour;
            g.setColour(c.withAlpha(dragging ? 0.35f : 0.18f));
            g.fillRoundedRectangle(r, 3.0f);
            g.setColour(c);
            g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f).withStyle("Bold")));
            g.drawText(owner.badge_text(), r, juce::Justification::centred, false);
        }

        void mouseDown(juce::MouseEvent const& event) override
        {
            if (event.mods.isPopupMenu() || !owner.assigned) {
                owner.open_assign_menu();
                return;
            }

            dragging = true;
            drag_start_depth = owner.depth;
            owner.dragging_depth = true;
            owner.repaint();
            owner.update_popover();
            repaint();
        }

        void mouseDrag(juce::MouseEvent const& event) override
        {
            if (!dragging) {
                return;
            }

            bool const fine = event.mods.isCtrlDown();
            double const sensitivity = fine
                ? Control::DRAG_PIXELS_FULL_RANGE * 5.0 : Control::DRAG_PIXELS_FULL_RANGE;
            double const raw = drag_start_depth
                - (double)event.getDistanceFromDragStartY() / sensitivity;
            owner.apply_depth(owner.snap_depth(raw, drag_start_depth, fine));
            owner.update_popover();
        }

        void mouseUp(juce::MouseEvent const& /* event */) override
        {
            dragging = false;
            owner.dragging_depth = false;
            owner.repaint();
            owner.update_popover();
            repaint();
        }

        void mouseDoubleClick(juce::MouseEvent const& /* event */) override
        {
            if (owner.assigned) {
                owner.apply_depth(0.0);
            }
        }

    private:
        Control& owner;
        bool dragging;
        double drag_start_depth;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModBadge)
};


Control::Control(
        ParamBridge& bridge,
        Synth::ParamId const param_id,
        juce::String const& label,
        Style const style,
        Size const size
) : bridge(bridge),
    param_id(param_id),
    label(label),
    manager(nullptr),
    mod_caps(Modulation::CAP_ALL),
    style(style),
    size_tier(size),
    label_pos(style == Style::DOT ? LabelPos::NONE : LabelPos::TOP),
    /* No control paints its value on the dial any more - the hover / drag popover
     * surfaces it on demand. ALWAYS still *reserves* the value strip, though, so a
     * captioned knob keeps the caption + dial + (now blank) strip layout its cell
     * was sized for; removing that reservation is what dropped and un-centred every
     * knob. The value-less styles (dots, sliders, macro cells, bare header knobs)
     * opt down to POPOVER / NONE and reclaim the strip. */
    value_display(style == Style::DOT ? ValueDisplay::NONE : ValueDisplay::ALWAYS),
    source_placeholder(false),
    macro_mode(false),
    hook_default(0.5),
    ratio(bridge.get_ratio(param_id)),
    skew(1.0),
    freq_lo(0.0),
    freq_hi(0.0),
    freq_skew(1.0),
    min_ratio(0.0),
    drag_start_visual(0.0),
    dragging(false),
    dragging_depth(false),
    drag_start_depth(0.0),
    semitone_snap(false),
    compact(size == Size::SMALL),
    bare(false),
    centred_badge(false),
    hover(false),
    assigned(false),
    mod_type(Modulation::LFO),
    mod_slot(0),
    base(0.0),
    depth(0.5)
{
    setWantsKeyboardFocus(false);
}


Control::~Control() = default;


void Control::set_style(Style const s) { style = s; }
void Control::set_size(Size const s) { size_tier = s; compact = (s == Size::SMALL); }
void Control::set_manager(ModulationManager* const m) { manager = m; }
void Control::set_mod_caps(int const c) { mod_caps = c; }
void Control::set_mirrors(std::vector<Synth::ParamId> m) { mirrors = std::move(m); }
void Control::set_value_mirrors(std::vector<Synth::ParamId> m) { value_mirrors = std::move(m); }
void Control::set_inverse_mirrors(std::vector<Synth::ParamId> m) { inverse_mirrors = std::move(m); }
void Control::set_discrete_labels(juce::StringArray labels) { discrete_labels = std::move(labels); }
void Control::set_semitone_snap(bool const on) { semitone_snap = on; }
void Control::set_min_ratio(double const r) { min_ratio = juce::jlimit(0.0, 1.0, r); }
void Control::set_label_placement(LabelPos const p) { label_pos = p; resized(); repaint(); }
void Control::set_value_display(ValueDisplay const d) { value_display = d; resized(); repaint(); }
void Control::set_source_placeholder(bool const on) { source_placeholder = on; update_badge(); }
void Control::set_badge_centred(bool const on) { centred_badge = on; update_badge(); }


void Control::set_macro(int const slot)
{
    macro_mode = true;
    mod_type = Modulation::MACRO;
    mod_slot = slot;
    mod_colour = Theme::MIDI;
    source_placeholder = true;   /* the badge is always present as a source picker */
    update_assignment();
}


juce::Colour Control::accent() const
{
    return macro_mode ? Theme::MIDI : Theme::ACCENT;
}


void Control::set_compact(bool const on)
{
    compact = on;
    if (on) {
        size_tier = Size::SMALL;
    }
}


void Control::set_bare(bool const on)
{
    bare = on;
    if (on) {
        label_pos = LabelPos::NONE;
        value_display = ValueDisplay::NONE;
    }
}


void Control::set_sub_control(
        std::unique_ptr<juce::Component> child,
        std::function<void()> refresh
) {
    sub_control = std::move(child);
    sub_refresh = std::move(refresh);
    if (sub_control != nullptr) {
        addAndMakeVisible(*sub_control);
        resized();
    }
}


void Control::set_value_hooks(
        std::function<double()> read,
        std::function<void(double)> write,
        std::function<juce::String(double)> format
) {
    read_value_hook = std::move(read);
    write_value_hook = std::move(write);
    format_value_hook = std::move(format);
    if (read_value_hook) {
        ratio = read_value_hook();
    }
}


void Control::set_hook_default(double const d) { hook_default = juce::jlimit(0.0, 1.0, d); }


/* One text size for every control's caption and value readout: the oscillator
 * knob size. Size tiers scale the dial/track, not the text. */
float Control::font_size() const { return 11.0f; }
int Control::strip_h() const { return STRIP; }


void Control::assign_mirrors(Modulation::Type const type, int const slot)
{
    if (slot == 0) {
        return;
    }

    /* Point every grouped copy's matching param at the same modulator slot. */
    Synth::ControllerId const cid = Modulation::controller_id(type, slot);
    for (Synth::ParamId const m : mirrors) {
        bridge.assign_controller(m, cid);
    }
}


void Control::clear_mirrors()
{
    for (Synth::ParamId const m : mirrors) {
        bridge.assign_controller(m, Synth::ControllerId::NONE);
    }
}


void Control::assign_inverse_mirrors(Synth::ControllerId const source, double const base)
{
    /* Each inverse mirror gets its own parallel macro fed by the same source; the
     * complement range is then written by sync_inverse_mirrors. */
    if (manager == nullptr) {
        return;
    }

    for (Synth::ParamId const inv : inverse_mirrors) {
        manager->assign_source(inv, source, base);
    }

    sync_inverse_mirrors(base, juce::jlimit(0.0, 1.0, base + 0.5));
}


void Control::clear_inverse_mirrors()
{
    if (manager == nullptr) {
        return;
    }

    for (Synth::ParamId const inv : inverse_mirrors) {
        manager->unassign(inv);
    }
}


void Control::sync_inverse_mirrors(double const primary_min, double const primary_max)
{
    /* Mirror each inverse destination's macro range to the complement of the
     * primary's, so the source drives it the opposite way (a crossfade). */
    for (Synth::ParamId const inv : inverse_mirrors) {
        Modulation::Type t;
        int slot;

        if (Modulation::decode(bridge.controller(inv), t, slot) && t == Modulation::MACRO) {
            bridge.set_ratio(Modulation::macro_min(slot), juce::jlimit(0.0, 1.0, 1.0 - primary_min));
            bridge.set_ratio(Modulation::macro_max(slot), juce::jlimit(0.0, 1.0, 1.0 - primary_max));
        }
    }
}


double Control::ratio_to_visual(double const r) const
{
    if (has_freq_range()) {
        double const d = juce::jlimit(freq_lo, freq_hi, bridge.display_value(param_id, r));
        double const frac = std::log(d / freq_lo) / std::log(freq_hi / freq_lo);
        return std::pow(juce::jlimit(0.0, 1.0, frac), 1.0 / freq_skew);
    }

    return (skew == 1.0 || r <= 0.0) ? r : std::pow(r, 1.0 / skew);
}


double Control::visual_to_ratio(double const v) const
{
    if (has_freq_range()) {
        double const vv = juce::jlimit(0.0, 1.0, v);
        double const d = freq_lo * std::pow(freq_hi / freq_lo, std::pow(vv, freq_skew));
        return ratio_for_display(d);
    }

    return (skew == 1.0 || v <= 0.0) ? v : std::pow(v, skew);
}


double Control::ratio_for_display(double const target) const
{
    double const at_min = bridge.display_value(param_id, 0.0);
    double const at_max = bridge.display_value(param_id, 1.0);
    bool const increasing = at_max >= at_min;
    double lo = 0.0;
    double hi = 1.0;

    for (int i = 0; i != 40; ++i) {
        double const mid = 0.5 * (lo + hi);
        if ((bridge.display_value(param_id, mid) < target) == increasing) lo = mid;
        else hi = mid;
    }

    return 0.5 * (lo + hi);
}


double Control::snap_ratio(double const raw_ratio, double const from_visual, bool const fine) const
{
    if (!semitone_snap || fine) {
        return raw_ratio;
    }

    /* Snap the movement to whole semitones, keeping any fine offset. */
    double const start_cents = bridge.display_value(param_id, visual_to_ratio(from_visual));
    double const raw_cents = bridge.display_value(param_id, raw_ratio);
    return ratio_for_display(start_cents + std::round((raw_cents - start_cents) / 100.0) * 100.0);
}


double Control::snap_depth(double const raw_depth, double const from_depth, bool const fine) const
{
    if (!semitone_snap || fine) {
        return raw_depth;
    }

    /* Snap the modulation target to the same semitone grid as the base, in
     * visual space, then express it back as a visual depth. */
    double const bvis = ratio_to_visual(base);
    double const start_cents =
        bridge.display_value(param_id, visual_to_ratio(juce::jlimit(0.0, 1.0, bvis + from_depth)));
    double const raw_cents =
        bridge.display_value(param_id, visual_to_ratio(juce::jlimit(0.0, 1.0, bvis + raw_depth)));
    double const snapped = start_cents + std::round((raw_cents - start_cents) / 100.0) * 100.0;
    return ratio_to_visual(ratio_for_display(snapped)) - bvis;
}


void Control::set_center_value(double const display_value)
{
    double const at_min = bridge.display_value(param_id, 0.0);
    double const at_max = bridge.display_value(param_id, 1.0);

    if (display_value <= juce::jmin(at_min, at_max) || display_value >= juce::jmax(at_min, at_max)) {
        return;
    }

    double const center_ratio = ratio_for_display(display_value);

    if (center_ratio > 1.0e-4 && center_ratio < 1.0 - 1.0e-4) {
        skew = juce::jlimit(0.1, 10.0, std::log(center_ratio) / std::log(0.5));
    }
}


void Control::set_freq_range(double const lo, double const hi, double const center)
{
    if (lo <= 0.0 || hi <= lo) {
        return;
    }

    freq_lo = lo;
    freq_hi = hi;

    /* The sweep is display = lo * (hi/lo)^(v^s); pick s so v = 0.5 lands on the
     * requested center frequency. */
    double const frac = std::log(center / lo) / std::log(hi / lo);
    freq_skew = juce::jlimit(0.1, 10.0,
        std::log(juce::jlimit(1.0e-4, 1.0 - 1.0e-4, frac)) / std::log(0.5));
}


void Control::update_macro_assignment()
{
    /* A macro cell edits its own MIN (base) / MAX (depth) directly; the "assigned"
     * (modulated) styling appears only when an input source drives the macro -
     * i.e. macro_in is set, or it is a full-randomness source. */
    Synth::ControllerId const src = bridge.controller(Modulation::macro_in(mod_slot));
    bool const random = bridge.get_ratio(Modulation::macro_rnd(mod_slot)) >= 0.99;
    bool const now = src != Synth::ControllerId::NONE || random;

    juce::String new_label;
    if (random) {
        new_label = "Rnd";   /* a random macro reads as "Rnd" even with a peak input */
    } else if (src != Synth::ControllerId::NONE) {
        new_label = Modulation::source_short_name(src);
    }

    bool const changed = now != assigned || new_label != mod_label;
    assigned = now;
    mod_label = new_label;
    mod_colour = Theme::MIDI;

    if (changed) {
        /* Only pull the base/depth here when the *assignment* itself changed (a
         * source was just picked). Reading it on every frame would pre-empt the
         * change detection in refresh()'s assigned branch (which captures the old
         * base before calling read_base_depth), so an external reset of macro
         * MIN/MAX - e.g. INIT, which preserves the macro's CC input so the cell
         * stays assigned - would never repaint. That was the macro cells "not
         * resetting on INIT" bug. */
        if (now) {
            read_base_depth();
        }
        update_badge();
        if (badge != nullptr) {
            badge->repaint();
        }
        repaint();
    }
}


void Control::update_assignment()
{
    if (macro_mode) {
        update_macro_assignment();
        return;
    }

    Modulation::Type type;
    int slot;
    bool const now = manager != nullptr
        && Modulation::decode(bridge.controller(param_id), type, slot);

    if (now != assigned || (now && (type != mod_type || slot != mod_slot))) {
        assigned = now;

        if (now) {
            mod_type = type;
            mod_slot = slot;

            /* An intermediate macro (input driven by a global source) shows the
             * source's name and colour, not its pool slot. */
            mod_label = juce::String(Modulation::prefix(type)) + juce::String(slot);
            Synth::ControllerId src = Synth::ControllerId::NONE;
            bool is_random = false;

            if (type == Modulation::MACRO) {
                src = bridge.controller(Modulation::macro_in(slot));

                /* A fully-random intermediate macro is a "Random" source, even
                 * though its input follows Osc 2's peak (so it re-draws per note). */
                if (bridge.get_ratio(Modulation::macro_rnd(slot)) >= 0.99) {
                    mod_label = "Rnd";
                    is_random = true;
                } else if (src != Synth::ControllerId::NONE) {
                    mod_label = Modulation::source_short_name(src);
                }
            }

            mod_colour = is_random ? Theme::MACRO : Modulation::assigned_colour(type, src);
            read_base_depth();
        }

        update_badge();
        if (badge != nullptr) {
            badge->repaint();
        }
        repaint();
    }
}


bool Control::badge_shown() const
{
    /* Every modulatable control shows its badge across the whole plugin: an empty
     * (barely-visible) placeholder when unassigned, the source chip once assigned.
     * A control is modulatable when it has a manager or is a macro cell. */
    return assigned || manager != nullptr || macro_mode;
}


void Control::update_badge()
{
    juce::Component* const parent = getParentComponent();
    bool const show = badge_shown() && parent != nullptr && isVisible();

    if (!show) {
        if (badge != nullptr) {
            badge->setVisible(false);
        }
        return;
    }

    if (badge == nullptr) {
        badge = std::make_unique<ModBadge>(*this);
    }

    if (badge->getParentComponent() != parent) {
        parent->addChildComponent(badge.get());
        badge->toFront(false);
    }

    /* The empty placeholder occupies the same box an assigned badge would (it is
     * not shrunk), so the modulation slot reads at a consistent size everywhere. */
    juce::Rectangle<float> const br = badge_rect();
    badge->setBounds(
        getX() + juce::roundToInt(br.getX()),
        getY() + juce::roundToInt(br.getY()),
        juce::roundToInt(br.getWidth()),
        juce::roundToInt(br.getHeight())
    );
    badge->setVisible(true);
}


bool Control::title_on_knob() const
{
    return label_pos != LabelPos::NONE && label.isNotEmpty();
}


bool Control::popover_shown() const
{
    if (editor != nullptr) {
        return true;   /* the edit bubble reuses the popover as its container */
    }

    /* No control shows its value inline any more, so the popover surfaces it on
     * every hover and drag (the title, if hidden, rides along above it). */
    return hover || dragging || dragging_depth;
}


void Control::update_popover()
{
    /* The popover lives on the top-level component, not the immediate parent, so
     * it can float above a control's title even when that control sits in a thin
     * strip / header row that would otherwise clip it. */
    juce::Component* const top = getTopLevelComponent();
    bool const show = popover_shown() && top != nullptr && top != this && isVisible();

    if (!show) {
        if (popover != nullptr) {
            popover->setVisible(false);
        }
        return;
    }

    bool const editing = editor != nullptr;

    double const shown = assigned
        ? (dragging_depth
            ? visual_to_ratio(juce::jlimit(0.0, 1.0, ratio_to_visual(base) + depth)) : base)
        : current_ratio();

    /* Carry the title into the popover only when the knob itself does not show
     * one (e.g. the inaccuracy dots, or any control being edited without a
     * visible caption). */
    bool const include_title = !title_on_knob() && label.isNotEmpty();

    if (popover == nullptr) {
        popover = std::make_unique<ValuePopover>();
    }
    popover->set_content(
        include_title ? label : juce::String(),
        editing ? juce::String() : format_ratio(shown),
        assigned ? mod_colour : accent()
    );
    popover->set_editing(editing);

    /* Anchor (in this control's coordinates): above the modulator badge while
     * dragging the amount; otherwise above the title (so it never covers it)
     * when one shows on top, else above the dial / track. */
    float centre_x;
    float top_ref;
    if (dragging_depth && badge_shown()) {
        juce::Rectangle<float> const br = badge_rect();
        centre_x = br.getCentreX();
        top_ref = br.getY();
    } else {
        juce::Rectangle<float> const el = is_slider() ? track_rect() : knob_circle();
        centre_x = el.getCentreX();
        top_ref = label_pos == LabelPos::TOP ? (float)label_strip().getY() : el.getY();
    }

    /* Convert the anchor into top-level coordinates and place the bubble there. */
    juce::Point<int> const anchor = top->getLocalPoint(
        this, juce::Point<int>(juce::roundToInt(centre_x), juce::roundToInt(top_ref))
    );

    juce::Rectangle<int> box = popover->measure();
    if (editing) {
        box.setWidth(juce::jmax(box.getWidth(), 56));   /* room to type */
    }
    int const w = box.getWidth();
    int const h = box.getHeight();
    int px = juce::jlimit(0, juce::jmax(0, top->getWidth() - w), anchor.x - w / 2);
    int py = juce::jmax(0, anchor.y - 2 - h);

    if (popover->getParentComponent() != top) {
        top->addChildComponent(popover.get());
    }
    popover->set_arrow_x((float)(anchor.x - px));
    popover->setBounds(px, py, w, h);
    popover->toFront(false);
    popover->setVisible(true);
    popover->repaint();

    /* Host the text editor inside the popover's value line while editing. */
    if (editing) {
        if (editor->getParentComponent() != popover.get()) {
            popover->addAndMakeVisible(*editor);
        }
        editor->setBounds(popover->value_area());
        editor->toFront(true);
    }
}


void Control::read_base_depth()
{
    Synth::ParamId const base_param =
        mod_type == Modulation::ENVELOPE ? Modulation::env_ini(mod_slot)
      : mod_type == Modulation::LFO ? Modulation::lfo_min(mod_slot)
      : Modulation::macro_min(mod_slot);

    /* depth is a delta in *visual* (skewed) space, so the modulation reach keeps
     * a constant visual distance on exponential knobs regardless of the base. */
    base = bridge.get_ratio(base_param);
    depth = ratio_to_visual(bridge.get_ratio(Modulation::depth_param(mod_type, mod_slot)))
        - ratio_to_visual(base);
}


void Control::apply_base(double const b)
{
    base = juce::jlimit(0.0, 1.0, b);

    if (mod_type == Modulation::ENVELOPE) {
        /* INI and FIN follow the base; SUS is owned by the envelope card's
         * sustain fraction, so it is left alone here. */
        bridge.set_ratio(Modulation::env_ini(mod_slot), base);
        bridge.set_ratio(Modulation::env_fin(mod_slot), base);
    } else if (mod_type == Modulation::LFO) {
        bridge.set_ratio(Modulation::lfo_min(mod_slot), base);
    } else {
        bridge.set_ratio(Modulation::macro_min(mod_slot), base);
    }

    /* Keep the destination param's own (unassigned) value in lock-step with the
     * base, so removing the modulator leaves the knob exactly where it sits now
     * instead of snapping back to the value it held when the modulator was first
     * assigned. In macro-cell mode the destination *is* the macro MIN written
     * above, so that write already covers it. */
    if (!macro_mode) {
        bridge.set_ratio(param_id, base);
        for (Synth::ParamId const m : mirrors) {
            bridge.set_ratio(m, base);
        }
    }

    double const target = juce::jlimit(0.0, 1.0, ratio_to_visual(base) + depth);
    double const target_ratio = visual_to_ratio(target);
    bridge.set_ratio(Modulation::depth_param(mod_type, mod_slot), target_ratio);

    if (!inverse_mirrors.empty()) {
        sync_inverse_mirrors(base, target_ratio);
    }
    repaint();
}


void Control::apply_depth(double const d)
{
    depth = juce::jlimit(-1.0, 1.0, d);
    double const target = juce::jlimit(0.0, 1.0, ratio_to_visual(base) + depth);
    double const target_ratio = visual_to_ratio(target);
    bridge.set_ratio(Modulation::depth_param(mod_type, mod_slot), target_ratio);

    if (!inverse_mirrors.empty()) {
        sync_inverse_mirrors(base, target_ratio);
    }
    repaint();
}


double Control::current_ratio() const
{
    return has_value_hooks() ? juce::jlimit(0.0, 1.0, read_value_hook()) : ratio;
}


void Control::refresh()
{
    if (sub_refresh) {
        sub_refresh();
    }

    update_assignment();

    if (dragging) {
        return;
    }

    if (assigned) {
        double const b0 = base;
        double const d0 = depth;
        read_base_depth();

        if (std::fabs(b0 - base) > 1.0e-6 || std::fabs(d0 - depth) > 1.0e-6) {
            repaint();
        }
    } else {
        /* Re-read the live value from the bridge (not the cached ratio) so an
         * external change - INIT, a loaded preset, host automation - actually
         * moves the knob. Reading current_ratio() here would compare the cache
         * against itself and never update, which is why macro-modulated controls
         * (whose values differ from the default) visibly failed to reset on INIT
         * while untouched knobs, already at their default, looked fine. */
        double const live = has_value_hooks()
            ? juce::jlimit(0.0, 1.0, read_value_hook())
            : bridge.get_ratio(param_id);

        if (std::fabs(live - ratio) > 1.0e-6) {
            ratio = live;
            repaint();
        }
    }

    update_badge();
    update_popover();
}


juce::String Control::format_ratio(double const r) const
{
    if (format_value_hook) {
        return format_value_hook(r);
    }

    if (!discrete_labels.isEmpty() && bridge.is_discrete(param_id)) {
        int const idx = (int)std::lround(bridge.display_value(param_id, r));

        if (idx >= 0 && idx < discrete_labels.size()) {
            return discrete_labels[idx];
        }
    }

    double const value = bridge.display_value(param_id, r);

    /* Stepped values read as a whole number when they land exactly on a step
     * (e.g. the pitch knobs show "0" / "12", not "0.000"); otherwise fall through
     * to three decimals. */
    if (semitone_snap) {
        double const semitones = value / 100.0;
        return std::fabs(semitones - std::round(semitones)) < 0.005
            ? juce::String((int)std::round(semitones))
            : juce::String(semitones, 3);
    }

    if (bridge.is_discrete(param_id)) {
        return juce::String((int)std::round(value));
    }

    /* Continuous values always show three decimals; very large readouts (e.g. a
     * frequency in Hz) drop them to stay legible. */
    if (std::fabs(value) >= 1000.0) {
        return juce::String((int)std::round(value));
    }

    return juce::String(value, 3);
}


/* ---- Geometry ---- */

juce::Rectangle<int> Control::control_bounds() const
{
    return (is_slider() ? track_rect() : knob_circle()).toNearestInt();
}


juce::Rectangle<int> Control::body_bounds() const
{
    juce::Rectangle<int> b = getLocalBounds().reduced(2);

    if (label_pos == LabelPos::LEFT) {
        b.removeFromLeft(LEFT_GUTTER);
    } else if (label_pos == LabelPos::TOP) {
        b.removeFromTop(STRIP);
    }

    /* The value readout is never painted inline any more (it lives in the hover /
     * drag popover), so no bottom strip is reserved: the dial / track fills the
     * body right down to the cell's bottom edge. Cell heights were trimmed to
     * match, keeping the dial the same size and position under its caption. */
    return b;
}


juce::Rectangle<int> Control::label_strip() const
{
    juce::Rectangle<int> b = getLocalBounds().reduced(2);

    if (label_pos == LabelPos::LEFT) {
        return b.removeFromLeft(LEFT_GUTTER);
    }
    if (label_pos == LabelPos::TOP) {
        return b.removeFromTop(STRIP);
    }
    return {};
}


juce::Rectangle<float> Control::knob_circle() const
{
    if (style == Style::DOT) {
        /* A capped small pie. With a caption it is left-aligned right after the
         * gutter (so a badge fits to its right); otherwise centred in its cell. */
        juce::Rectangle<int> b = getLocalBounds();
        if (label_pos == LabelPos::TOP) {
            b.removeFromTop(STRIP);
        }
        if (label_pos == LabelPos::LEFT) {
            /* An extra 2px past the caption gutter keeps a small breathing gap
             * between a left-hand title (e.g. "MIX") and the pie. */
            b.removeFromLeft(LEFT_GUTTER + 2);
            float const s = juce::jmin((float)b.getWidth(), (float)b.getHeight(), DOT_BOX);
            return juce::Rectangle<float>((float)b.getX(), b.getCentreY() - s * 0.5f, s, s).reduced(1.0f);
        }
        float const s = juce::jmin((float)juce::jmin(b.getWidth(), b.getHeight()), DOT_BOX);
        return b.toFloat().withSizeKeepingCentre(s, s).reduced(1.0f);
    }

    juce::Rectangle<int> const b = body_bounds();
    float size = (float)juce::jmin(b.getWidth(), b.getHeight());
    if (macro_mode) {
        size = juce::jmin(size, (float)OSC_DIAL_BOX);   /* match the oscillator dial */
    }
    return b.toFloat().withSizeKeepingCentre(size, size).reduced(3.0f);
}


juce::Rectangle<float> Control::track_rect() const
{
    juce::Rectangle<float> b = body_bounds().toFloat();

    /* The groove is 2px thinner than the badge. */
    float const thickness = BADGE_H - 2.0f;

    if (style == Style::H_SLIDER) {
        /* A horizontal groove, leaving extra room at the right end so the badge
         * (modulation box) does not overlap the track. */
        b = b.withSizeKeepingCentre(b.getWidth(), thickness);
        if (badge_shown()) {
            b.removeFromRight(BADGE_H + 8.0f);
        }
        return b;
    }

    /* V_SLIDER: a vertical groove using the full body height - the badge now sits
     * to the right of the groove (like a knob), not above it. */
    b = b.withSizeKeepingCentre(thickness, b.getHeight());
    return b;
}


juce::Point<float> Control::handle_at(double const visual) const
{
    juce::Rectangle<float> const t = track_rect();
    float const v = (float)juce::jlimit(0.0, 1.0, visual);

    if (style == Style::H_SLIDER) {
        return { t.getX() + v * t.getWidth(), t.getCentreY() };
    }
    /* V_SLIDER: 0 at the bottom, 1 at the top. */
    return { t.getCentreX(), t.getBottom() - v * t.getHeight() };
}


juce::String Control::badge_text() const
{
    return mod_label.toUpperCase().substring(0, 3);
}


juce::Rectangle<float> Control::badge_rect() const
{
    juce::Font const bf(juce::FontOptions().withHeight(10.0f).withStyle("Bold"));
    juce::String const txt = assigned ? badge_text() : juce::String();
    float const tw = juce::GlyphArrangement::getStringWidth(bf, txt);
    float const w = juce::jmax(16.0f, tw + 8.0f);
    float const h = BADGE_H;

    if (style == Style::H_SLIDER) {
        /* Anchored 4px to the right of the track, growing rightward. */
        juce::Rectangle<float> const t = track_rect();
        return juce::Rectangle<float>(t.getRight() + 4.0f, t.getCentreY() - h * 0.5f, w, h);
    }

    if (style == Style::V_SLIDER) {
        /* To the right of the groove, aligned to its top - mirrors a knob's
         * top-right badge and the horizontal slider's right-hand badge. */
        juce::Rectangle<float> const t = track_rect();
        return juce::Rectangle<float>(t.getRight() + 4.0f, t.getY(), w, h);
    }

    if (style == Style::DOT) {
        /* Directly to the right of the pie, vertically centred on it (header
         * pies read as one horizontal "MIX o [box]" row). */
        juce::Rectangle<float> const kb = knob_circle();
        return juce::Rectangle<float>(kb.getRight() + 4.0f, kb.getCentreY() - h * 0.5f, w, h);
    }

    if (centred_badge) {
        /* Directly to the right of the dial, vertically centred on it (mirrors the
         * DOT layout) - used by the header OUT knob. */
        juce::Rectangle<float> const kb = knob_circle();
        return juce::Rectangle<float>(kb.getRight() + 4.0f, kb.getCentreY() - h * 0.5f, w, h);
    }

    /* ROTARY: lower-left corner sits on the reach ring at the top-right (45 deg),
     * so the badge meets the modulation-amount curve regardless of cell width. */
    juce::Rectangle<float> const kb = knob_circle();
    float const rr = kb.getWidth() * 0.5f + 3.0f;
    float const dx = std::sqrt(juce::jmax(0.0f, h * (2.0f * rr - h)));
    float x = juce::jmax(0.0f, kb.getCentreX() + dx + 2.0f);
    float y = juce::jmax(0.0f, kb.getCentreY() - rr);
    return juce::Rectangle<float>(x, y, w, h);
}


/* ---- Painting ---- */

void Control::paint(juce::Graphics& g)
{
    /* Label. */
    if (label_pos != LabelPos::NONE && !label.isEmpty()) {
        g.setColour(Theme::TEXT_DIM);
        g.setFont(font_size());
        /* LEFT captions keep a 4px gap from the dial / track. */
        juce::Rectangle<int> strip = label_pos == LabelPos::LEFT
            ? label_strip().withTrimmedRight(4) : label_strip();
        g.drawText(
            label, strip,
            label_pos == LabelPos::LEFT ? juce::Justification::centredRight
                                        : juce::Justification::centred,
            false
        );
    }

    if (style == Style::DOT) {
        paint_dot(g);
    } else if (is_slider()) {
        paint_slider(g);
    } else {
        paint_rotary(g);
    }
}


void Control::paint_rotary(juce::Graphics& g)
{
    juce::Rectangle<float> const kb = knob_circle();
    float const cx = kb.getCentreX();
    float const cy = kb.getCentreY();
    float const r = kb.getWidth() * 0.5f;

    juce::PathStrokeType const stroke(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc(cx, cy, r, r, 0.0f, START_ANGLE, END_ANGLE, true);
    g.setColour(Theme::TRACK);
    g.strokePath(track, stroke);

    juce::Colour const active = assigned ? mod_colour : accent();
    double const position = ratio_to_visual(assigned ? base : current_ratio());
    float const pos_angle = angle_of(position);

    juce::Path value_arc;
    value_arc.addCentredArc(cx, cy, r, r, 0.0f, START_ANGLE, pos_angle, true);
    g.setColour(assigned ? active.withAlpha(0.55f) : active);
    g.strokePath(value_arc, stroke);

    float const body = r - 4.0f;
    g.setColour(Theme::PANEL_2);
    g.fillEllipse(cx - body, cy - body, body * 2.0f, body * 2.0f);
    /* The knob body border always keeps the normal (unmodulated) edge colour and
     * weight even when assigned - only the position line and value arc take on the
     * modulation colour. */
    g.setColour(Theme::EDGE);
    g.drawEllipse(cx - body, cy - body, body * 2.0f, body * 2.0f, 1.0f);

    g.setColour(active);
    g.drawLine(cx, cy + 0.0f,
               cx + std::sin(pos_angle) * body, cy - std::cos(pos_angle) * body, 2.0f);

    if (!assigned) {
        return;
    }

    /* Outer reach ring: base -> base+depth in visual space, so the reach length
     * is constant for a given amount even on exponential knobs. */
    float const target = (float)juce::jlimit(0.0, 1.0, position + depth);
    float const target_angle = angle_of(target);
    float const rr = r + 3.0f;
    float const a0 = juce::jmin(pos_angle, target_angle);
    float const a1 = juce::jmax(pos_angle, target_angle);

    juce::Path reach;
    reach.addCentredArc(cx, cy, rr, rr, 0.0f, a0, a1, true);
    g.setColour(active);
    g.strokePath(reach, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    float const tx = cx + std::sin(target_angle) * rr;
    float const ty = cy - std::cos(target_angle) * rr;
    g.fillEllipse(tx - 2.0f, ty - 2.0f, 4.0f, 4.0f);
}


void Control::paint_dot(juce::Graphics& g)
{
    juce::Rectangle<float> const circle = knob_circle();
    float const cx = circle.getCentreX();
    float const cy = circle.getCentreY();
    float const d = circle.getWidth();
    float const r = d * 0.5f;

    double const v = juce::jlimit(0.0, 1.0, assigned ? base : current_ratio());
    juce::Colour const active = assigned ? mod_colour : accent();

    /* Radial pie fill, sweeping clockwise from 6 o'clock (bottom); a solid disc
     * at 1. */
    if (v > 1.0e-3) {
        float const start = juce::MathConstants<float>::pi;
        juce::Path pie;
        pie.addPieSegment(
            cx - r, cy - r, d, d,
            start, start + juce::MathConstants<float>::twoPi * (float)v, 0.0f
        );
        g.setColour(assigned ? active.withAlpha(0.7f) : active);
        g.fillPath(pie);
    }

    g.setColour(active);
    g.drawEllipse(circle, 1.0f);

    /* Assigned: a thin outer reach ring showing base -> base+depth. */
    if (assigned) {
        float const rr = r + 2.5f;
        float const p0 = angle_of(v);
        float const p1 = angle_of((float)juce::jlimit(0.0, 1.0, v + depth));
        juce::Path reach;
        reach.addCentredArc(cx, cy, rr, rr, 0.0f, juce::jmin(p0, p1), juce::jmax(p0, p1), true);
        g.setColour(active);
        g.strokePath(reach, juce::PathStrokeType(2.0f));
    }
}


void Control::paint_slider(juce::Graphics& g)
{
    juce::Rectangle<float> const t = track_rect();
    juce::Colour const active = assigned ? mod_colour : accent();
    double const position = ratio_to_visual(assigned ? base : current_ratio());

    /* Groove. */
    g.setColour(Theme::INSET);
    g.fillRoundedRectangle(t, 2.0f);

    /* Base fill from the zero end up to the handle. */
    juce::Point<float> const h = handle_at(position);
    g.setColour(assigned ? active.withAlpha(0.55f) : active);

    if (style == Style::H_SLIDER) {
        g.fillRoundedRectangle(juce::Rectangle<float>(t.getX(), t.getY(), h.x - t.getX(), t.getHeight()), 2.0f);
    } else {
        g.fillRoundedRectangle(juce::Rectangle<float>(t.getX(), h.y, t.getWidth(), t.getBottom() - h.y), 2.0f);
    }

    /* Handle marker. */
    g.setColour(active);
    if (style == Style::H_SLIDER) {
        g.fillRoundedRectangle(juce::Rectangle<float>(h.x - 1.5f, t.getY() - 1.5f, 3.0f, t.getHeight() + 3.0f), 1.5f);
    } else {
        g.fillRoundedRectangle(juce::Rectangle<float>(t.getX() - 1.5f, h.y - 1.5f, t.getWidth() + 3.0f, 3.0f), 1.5f);
    }

    /* Modulation reach: a thin 2px line running from the base handle to the mod
     * target along the slider's axis, with a 4px circle marking the target. */
    if (assigned) {
        double const target = juce::jlimit(0.0, 1.0, position + depth);
        juce::Point<float> const m = handle_at(target);
        g.setColour(active);

        if (style == Style::H_SLIDER) {
            float const x0 = juce::jmin(h.x, m.x);
            float const x1 = juce::jmax(h.x, m.x);
            g.fillRect(juce::Rectangle<float>(x0, t.getCentreY() - 1.0f, x1 - x0, 2.0f));
        } else {
            float const y0 = juce::jmin(h.y, m.y);
            float const y1 = juce::jmax(h.y, m.y);
            g.fillRect(juce::Rectangle<float>(t.getCentreX() - 1.0f, y0, 2.0f, y1 - y0));
        }

        g.fillEllipse(m.x - 2.0f, m.y - 2.0f, 4.0f, 4.0f);
    }
}


void Control::commit(double const new_ratio)
{
    ratio = juce::jlimit(min_ratio, 1.0, new_ratio);
    if (has_value_hooks()) {
        write_value_hook(ratio);
    } else {
        bridge.set_ratio(param_id, ratio);
        for (Synth::ParamId const m : value_mirrors) {
            bridge.set_ratio(m, ratio);
        }
    }
    repaint();
}


void Control::open_macro_source_menu()
{
    juce::PopupMenu menu;
    int const current = (int)bridge.controller(Modulation::macro_in(mod_slot));

    for (int i = 0; i != MACRO_SOURCE_COUNT; ++i) {
        menu.addItem(i + 1, MACRO_SOURCES[i].name, true, MACRO_SOURCES[i].id == current);
        if (i == 0 || i == 1) {
            menu.addSeparator();
        }
    }

    Control* const self = this;
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(this),
        [self](int const result) {
            if (result <= 0) {
                return;
            }

            self->bridge.assign_controller(
                Modulation::macro_in(self->mod_slot),
                (Synth::ControllerId)MACRO_SOURCES[result - 1].id
            );
            self->update_assignment();
        }
    );
}


void Control::open_assign_menu()
{
    if (macro_mode) {
        open_macro_source_menu();
        return;
    }

    juce::PopupMenu menu;

    if (assigned) {
        menu.addSectionHeader(
            juce::String(Modulation::prefix(mod_type)) + juce::String(mod_slot)
        );
        menu.addItem(4, "Remove modulation");
        menu.addSeparator();
    }

    /* Copy an existing envelope/LFO group (never a macro - macros are only
     * reached through "Modulate by"), and only kinds this destination accepts. */
    std::shared_ptr<std::vector<std::pair<Modulation::Type, int>>> const clones =
        std::make_shared<std::vector<std::pair<Modulation::Type, int>>>();

    for (ModulationManager::Group const& grp : manager->groups()) {
        bool const allowed =
            (grp.type == Modulation::ENVELOPE && (mod_caps & Modulation::CAP_ENV))
            || (grp.type == Modulation::LFO && (mod_caps & Modulation::CAP_LFO));

        if (!allowed) {
            continue;
        }

        int const id = 100 + (int)clones->size();
        clones->push_back({ grp.type, grp.rep });
        menu.addItem(id,
            "Copy " + juce::String(Modulation::prefix(grp.type)) + juce::String(grp.rep));
    }

    menu.addSeparator();

    if (mod_caps & Modulation::CAP_ENV) {
        menu.addItem(1, "New envelope  (" + juce::String(manager->free_count(Modulation::ENVELOPE)) + ")",
                     manager->free_count(Modulation::ENVELOPE) > 0);
    }
    if (mod_caps & Modulation::CAP_LFO) {
        menu.addItem(2, "New LFO  (" + juce::String(manager->free_count(Modulation::LFO)) + ")",
                     manager->free_count(Modulation::LFO) > 0);
    }

    /* Global sources route through an intermediate macro carrying this
     * destination's unique range. */
    if (mod_caps & Modulation::CAP_MACRO) {
        bool const have_macro = manager->free_count(Modulation::MACRO) > 0;
        juce::PopupMenu sources;
        for (int i = 0; i != CONTROL_SOURCE_COUNT; ++i) {
            sources.addItem(200 + i, CONTROL_SOURCES[i].name, have_macro);
        }
        sources.addSeparator();
        sources.addItem(300, "Random", have_macro);
        menu.addSubMenu("Modulate by  (" + juce::String(manager->free_count(Modulation::MACRO)) + ")",
                        sources);
    }

    Control* const self = this;
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(this),
        [self, clones](int const result) {
            double const b = self->bridge.get_ratio(self->param_id);
            Modulation::Type type = Modulation::ENVELOPE;
            int slot = 0;
            Synth::ControllerId inverse_source = Synth::ControllerId::NONE;

            if (result == 4) {
                self->manager->unassign(self->param_id);
                self->clear_mirrors();
                self->clear_inverse_mirrors();
                self->update_assignment();
                return;
            }

            if (result == 1 || result == 2) {
                Modulation::Type const nt =
                    result == 1 ? Modulation::ENVELOPE : Modulation::LFO;

                /* "New" on a currently-mirrored source of the same kind splits it
                 * off so it becomes separate, rather than allocating a fresh one. */
                if (self->assigned && self->mod_type == nt
                        && self->manager->is_mirrored(self->param_id)) {
                    self->manager->split(self->param_id);
                    self->update_assignment();
                    return;
                }

                type = nt;
                slot = self->manager->assign(self->param_id, nt, b, 0);
            } else if (result >= 100 && result < 200 && result - 100 < (int)clones->size()) {
                std::pair<Modulation::Type, int> const& gp = (*clones)[result - 100];

                /* Copy/mirror another source while already holding one of the same
                 * kind: copy its values onto this slot and merge into its group,
                 * instead of allocating a separate copy. */
                if (self->assigned && self->mod_type == gp.first) {
                    self->manager->merge(self->param_id, gp.first, gp.second);
                    self->update_assignment();
                    return;
                }

                type = gp.first;
                slot = self->manager->assign(self->param_id, gp.first, b, gp.second);
            } else if (result >= 200 && result - 200 < CONTROL_SOURCE_COUNT) {
                type = Modulation::MACRO;
                inverse_source = (Synth::ControllerId)CONTROL_SOURCES[result - 200].id;
                slot = self->manager->assign_source(self->param_id, inverse_source, b);
            } else if (result == 300) {
                type = Modulation::MACRO;
                slot = self->manager->assign_random(self->param_id, b);
            }

            self->assign_mirrors(type, slot);
            /* A source-fed assignment also drives any inverse (crossfade) mirrors
             * from the same source through complement-range macros. */
            if (slot != 0 && inverse_source != Synth::ControllerId::NONE) {
                self->assign_inverse_mirrors(inverse_source, b);
            }
            self->update_assignment();
        }
    );
}


/* ---- Hit testing ---- */

bool Control::hitTest(int x, int y)
{
    juce::Point<float> const p((float)x, (float)y);

    /* The sub-control (e.g. the macro curve square) is a child; JUCE won't route
     * events to it at points where the parent hitTest is false, so its box must
     * report a hit even though it sits outside the dial's ring. */
    if (sub_control != nullptr && sub_control->getBounds().contains(x, y)) {
        return true;
    }

    /* The caption drags the value too, so grabbing a header knob by its label
     * feels the same as grabbing its dial. */
    if (label_pos != LabelPos::NONE && !label.isEmpty()
            && label_strip().toFloat().contains(p)) {
        return true;
    }

    if (is_slider()) {
        return track_rect().expanded(RING_BAND * 0.5f).contains(p)
            || (badge_shown() && badge_rect().contains(p));
    }

    /* ROTARY / DOT: circular + a ring band, so cell corners fall through. */
    juce::Rectangle<float> const kb = knob_circle();
    float const reach = kb.getWidth() * 0.5f + RING_BAND;
    return kb.getCentre().getDistanceFrom(p) <= reach;
}


bool Control::over_ring(juce::Point<float> const p) const
{
    juce::Rectangle<float> const kb = knob_circle();
    return kb.getCentre().getDistanceFrom(p) > kb.getWidth() * 0.5f;
}


bool Control::over_depth_zone(juce::Point<float> const p) const
{
    if (!assigned) {
        return false;
    }

    if (is_slider()) {
        /* The region on the far side of the base handle, toward the mod line. */
        double const position = ratio_to_visual(base);
        juce::Point<float> const h = handle_at(position);
        return style == Style::H_SLIDER ? p.x > h.x : p.y < h.y;
    }

    return over_ring(p);
}


/* ---- Mouse ---- */

void Control::mouseDown(juce::MouseEvent const& event)
{
    if (event.mods.isPopupMenu() && manager != nullptr) {
        open_assign_menu();
        return;
    }

    /* Alt-click: typed value entry. */
    if (event.mods.isAltDown()) {
        open_value_editor();
        return;
    }

    dragging = true;

    if (over_depth_zone(event.position)) {
        dragging_depth = true;
        drag_start_depth = depth;
    } else {
        dragging_depth = false;
        drag_start_visual = ratio_to_visual(assigned ? base : current_ratio());
    }

    update_popover();
}


void Control::mouseDrag(juce::MouseEvent const& event)
{
    if (!dragging) {
        return;
    }

    bool const fine = event.mods.isCtrlDown();
    double const sensitivity = fine ? DRAG_PIXELS_FULL_RANGE * 5.0 : DRAG_PIXELS_FULL_RANGE;

    /* Horizontal sliders add their X travel to the (inverted) Y travel, so they
     * respond to both axes: drag right or up to increase, down to decrease, just
     * like a vertical slider or knob. Everything else tracks the inverted Y. */
    double const travel = style == Style::H_SLIDER
        ? (double)(event.getDistanceFromDragStartX() - event.getDistanceFromDragStartY())
        : -(double)event.getDistanceFromDragStartY();
    double const delta = travel / sensitivity;

    if (dragging_depth) {
        apply_depth(snap_depth(drag_start_depth + delta, drag_start_depth, fine));
        update_popover();
        return;
    }

    double const new_ratio = snap_ratio(
        visual_to_ratio(juce::jlimit(0.0, 1.0, drag_start_visual + delta)), drag_start_visual, fine
    );

    if (assigned) {
        apply_base(new_ratio);
    } else {
        commit(new_ratio);
    }

    update_popover();
}


void Control::mouseUp(juce::MouseEvent const& /* event */)
{
    bool const was_depth = dragging_depth;
    dragging = false;
    dragging_depth = false;
    update_popover();

    if (was_depth) {
        repaint();   /* revert the below-knob readout from amount to line value */
    }
}


void Control::mouseDoubleClick(juce::MouseEvent const& event)
{
    if (has_value_hooks()) {
        commit(hook_default);
        return;
    }

    if (assigned) {
        if (over_depth_zone(event.position)) {
            apply_depth(0.0);   /* depth zone: reset the modulation amount */
        } else {
            apply_base(bridge.get_default_ratio(param_id));   /* body: reset base */
        }
        return;
    }

    commit(bridge.get_default_ratio(param_id));
}


void Control::mouseEnter(juce::MouseEvent const& /* event */)
{
    hover = true;
    update_popover();
}


void Control::mouseExit(juce::MouseEvent const& /* event */)
{
    hover = false;
    update_popover();
}


void Control::mouseWheelMove(
        juce::MouseEvent const& event,
        juce::MouseWheelDetails const& wheel
) {
    bool const fine = event.mods.isCtrlDown();

    if (assigned) {
        if (semitone_snap && !fine) {
            /* step the modulation target by one semitone */
            double const bvis = ratio_to_visual(base);
            double const cents = bridge.display_value(
                param_id, visual_to_ratio(juce::jlimit(0.0, 1.0, bvis + depth))
            );
            double const target = ratio_for_display(cents + (wheel.deltaY > 0.0f ? 100.0 : -100.0));
            apply_depth(ratio_to_visual(target) - bvis);
        } else {
            apply_depth(depth + (double)wheel.deltaY * WHEEL_STEP);
        }
        return;
    }

    if (semitone_snap && !fine) {
        double const cents = bridge.display_value(param_id, current_ratio());
        commit(ratio_for_display(cents + (wheel.deltaY > 0.0f ? 100.0 : -100.0)));
        return;
    }

    double const v = ratio_to_visual(current_ratio()) + (double)wheel.deltaY * WHEEL_STEP;
    commit(visual_to_ratio(juce::jlimit(0.0, 1.0, v)));
}


/* ---- Typed value entry ---- */

void Control::open_value_editor()
{
    if (editor != nullptr) {
        return;
    }

    editor = std::make_unique<juce::TextEditor>();
    editor->setWantsKeyboardFocus(true);
    editor->setMultiLine(false);
    editor->setJustification(juce::Justification::centred);
    editor->setBorder(juce::BorderSize<int>(0));
    editor->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    editor->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    editor->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    editor->setColour(juce::TextEditor::textColourId, Theme::TEXT);
    editor->setFont(ValuePopover::value_font());

    double const shown = assigned ? base : current_ratio();
    editor->setText(juce::String(bridge.display_value(param_id, shown),
                                 bridge.is_discrete(param_id) ? 0 : 3), false);

    Control* const self = this;
    editor->onReturnKey = [self]() { self->close_editor(true); };
    editor->onEscapeKey = [self]() { self->close_editor(false); };
    editor->onFocusLost = [self]() { self->close_editor(true); };

    /* update_popover() hosts the editor inside the popover bubble (positioned like
     * the value readout) and shows the title above it when the knob has none. */
    update_popover();
    editor->grabKeyboardFocus();
    editor->selectAll();
    repaint();
}


void Control::close_editor(bool const apply)
{
    if (editor == nullptr) {
        return;
    }

    juce::String const text = editor->getText().trim();

    /* Detach ownership and callbacks *before* applying / deleting, so the focus-
     * loss triggered by teardown cannot re-enter this path (the ESC / return /
     * focus-lost handlers all funnel here). The editor is deleted asynchronously
     * because destroying a component from within its own callback crashes. */
    std::unique_ptr<juce::TextEditor> dead = std::move(editor);
    dead->onReturnKey = nullptr;
    dead->onEscapeKey = nullptr;
    dead->onFocusLost = nullptr;
    dead->setVisible(false);

    if (apply && text.isNotEmpty()) {
        double const new_ratio = ratio_for_display(text.getDoubleValue());
        if (assigned) {
            apply_base(new_ratio);
        } else {
            commit(juce::jlimit(min_ratio, 1.0, new_ratio));
        }
    }

    juce::TextEditor* const raw = dead.release();
    juce::MessageManager::callAsync([raw]() { delete raw; });

    update_popover();
    repaint();
}


void Control::resized()
{
    if (sub_control != nullptr) {
        /* Anchored at the dial's bottom-right (rotary) or the track end, clear of
         * the modulation reach ring / badge. */
        int const sz = 16;
        juce::Rectangle<float> const kb = is_slider() ? track_rect() : knob_circle();
        int const x = juce::roundToInt(kb.getRight()) + 8;
        int const y = juce::roundToInt(kb.getBottom()) - sz;
        sub_control->setBounds(x, y, sz, sz);
    }

    /* The text editor is positioned by update_popover() (it lives in the popover
     * bubble), so nothing to place here. */
}

}

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

#include <utility>

#include "ui/mini_button.hpp"

#include "ui/theme.hpp"

#include "BinaryData.h"


namespace JS80P
{

namespace {

/* Fixed glyph height for icon buttons: the icon draws at this size and is centred
 * in whatever (taller) button height it is given, so enlarging the click target
 * does not enlarge the icon. Paired with generous horizontal padding on each side
 * of the glyph so the whole button is a large click target. */
constexpr int ICON_GLYPH_H = 18;   /* enlarged for readability (nearest even) */
constexpr int ICON_H_PAD = 6;


/* Crop a sub-rectangle of the embedded synth.png sprite and turn it into a white
 * icon whose alpha channel is the sprite's luminance, so it can be tinted to any
 * colour (the sprite's glyphs are light line-art on a near-black background). */
juce::Image extract_glyph(int const x, int const y, int const w, int const h)
{
    static juce::Image const sheet = juce::ImageFileFormat::loadFrom(
        (void const*)BinaryData::synth_png, (size_t)BinaryData::synth_pngSize
    );

    if (!sheet.isValid()) {
        return juce::Image();
    }

    juce::Image glyph(juce::Image::ARGB, w, h, true);

    for (int j = 0; j != h; ++j) {
        for (int i = 0; i != w; ++i) {
            juce::Colour const src = sheet.getPixelAt(x + i, y + j);
            float const lum = (
                0.299f * src.getFloatRed()
                + 0.587f * src.getFloatGreen()
                + 0.114f * src.getFloatBlue()
            );
            /* Lift the near-black floor and clip at the glyph's peak brightness so
             * the background goes fully transparent and the strokes fully opaque. */
            float const a = juce::jlimit(0.0f, 1.0f, (lum - 0.05f) / 0.60f);
            glyph.setPixelAt(i, j, juce::Colours::white.withAlpha(a));
        }
    }

    return glyph;
}

}

MiniButton::MiniButton(
        ParamBridge& bridge,
        Synth::ParamId const param_id,
        juce::String label
) : bridge(&bridge),
    param_id(param_id),
    label(std::move(label)),
    action(false),
    value(bridge.get_discrete(param_id)),
    hover(false)
{
    setWantsKeyboardFocus(false);
}


MiniButton::MiniButton(juce::String label, std::function<void()> on_click)
    : bridge(nullptr),
    param_id(Synth::ParamId::INVALID_PARAM_ID),
    label(std::move(label)),
    on_click(std::move(on_click)),
    action(true),
    value(0),
    hover(false)
{
    setWantsKeyboardFocus(false);
}


MiniButton::MiniButton(juce::Image icon, std::function<void()> on_click)
    : bridge(nullptr),
    param_id(Synth::ParamId::INVALID_PARAM_ID),
    on_click(std::move(on_click)),
    icon(std::move(icon)),
    action(true),
    value(0),
    hover(false)
{
    setWantsKeyboardFocus(false);
}


juce::Image MiniButton::preset_icon(Icon const which)
{
    /* Bounding boxes (in synth.png pixels) of the folder / dice / download glyphs
     * that sit at the top-left of the legacy Synth panel, with a 1px margin so the
     * antialiased edges are not clipped when the icon is scaled down. */
    switch (which) {
        case Icon::OPEN:      return extract_glyph(19, 72, 61-19, 111-72);
        case Icon::RANDOMIZE: return extract_glyph(64, 72, 108-64, 114-72);
        case Icon::SAVE:      return extract_glyph(114, 72, 150-114, 111-72);
        default:              return juce::Image();
    }
}


void MiniButton::set_option_labels(juce::StringArray labels)
{
    option_labels = std::move(labels);
}


int MiniButton::preferred_width() const
{
    if (icon.isValid()) {
        /* Glyph drawn at the fixed glyph height, wrapped in generous side padding. */
        int const glyph_w = juce::roundToInt(
            (float)icon.getWidth() * (float)ICON_GLYPH_H / (float)icon.getHeight()
        );
        return glyph_w + 2 * ICON_H_PAD;
    }

    juce::Font const f(juce::FontOptions().withHeight(action ? 10.0f : 9.0f));
    float w = juce::GlyphArrangement::getStringWidth(f, label);

    for (juce::String const& s : option_labels) {
        w = juce::jmax(w, juce::GlyphArrangement::getStringWidth(f, s));
    }

    return juce::jmax(28, (int)w + 12);
}


void MiniButton::refresh()
{
    if (action) {
        return;
    }

    int const live = bridge->get_discrete(param_id);

    if (live != value) {
        value = live;
        repaint();
    }
}


void MiniButton::paint(juce::Graphics& g)
{
    juce::Colour const c = Theme::ACCENT;

    /* Icon action buttons: no outline box, just the glyph tinted to the accent
     * colour (white on hover), centred within a wide (padded) click target. */
    if (icon.isValid()) {
        float const glyph_h = (float)ICON_GLYPH_H;
        float const scale = glyph_h / (float)icon.getHeight();
        float const glyph_w = (float)icon.getWidth() * scale;
        float const gx = ((float)getWidth() - glyph_w) * 0.5f;
        float const gy = ((float)getHeight() - glyph_h) * 0.5f;

        /* Fill the glyph's alpha channel: accent normally, white on hover. */
        g.setColour(hover ? juce::Colours::white : c);
        g.drawImageTransformed(
            icon,
            juce::AffineTransform::scale(scale).translated(gx, gy),
            true
        );
        return;
    }

    /* Action buttons always render in the outline (off) state. */
    bool const on = !action && value != 0;
    juce::Rectangle<float> const r = getLocalBounds().toFloat().reduced(0.5f);
    float const radius = 2.0f;

    if (on) {
        g.setColour(hover ? c.brighter(0.15f) : c);
        g.fillRoundedRectangle(r, radius);
    } else if (hover) {
        g.setColour(c.withAlpha(0.18f));
        g.fillRoundedRectangle(r, radius);
    }

    g.setColour(on || !hover ? c : c.brighter(0.2f));
    g.drawRoundedRectangle(r, radius, 1.0f);

    juce::String text = label;
    if (!option_labels.isEmpty() && value >= 0 && value < option_labels.size()) {
        text = option_labels[value];
    }

    /* Solid fill needs dark text for contrast; the outline state keeps it orange.
     * Header action buttons (INIT) render ~110% larger for readability; the
     * effects-page param toggles stay at the original 9pt. */
    g.setColour(on ? Theme::BG : c);
    g.setFont(action ? 10.0f : 9.0f);
    g.drawText(text, getLocalBounds(), juce::Justification::centred, false);
}


void MiniButton::mouseUp(juce::MouseEvent const& event)
{
    if (!getLocalBounds().contains(event.getPosition())) {
        return;
    }

    if (action) {
        if (on_click) {
            on_click();
        }
        return;
    }

    int const count = juce::jmax(2, bridge->option_count(param_id));
    value = (bridge->get_discrete(param_id) + 1) % count;
    bridge->set_discrete(param_id, value);
    repaint();
}


void MiniButton::mouseEnter(juce::MouseEvent const& /* event */)
{
    hover = true;
    repaint();
}


void MiniButton::mouseExit(juce::MouseEvent const& /* event */)
{
    hover = false;
    repaint();
}

}

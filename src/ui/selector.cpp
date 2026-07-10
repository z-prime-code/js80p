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

#include "ui/selector.hpp"

#include "ui/theme.hpp"


namespace JS80P
{

Selector::Selector(
        ParamBridge& bridge,
        Synth::ParamId const param_id,
        juce::StringArray options,
        juce::String caption
) : bridge(bridge),
    param_id(param_id),
    mirror_param_id(NO_MIRROR),
    options(std::move(options)),
    caption(std::move(caption)),
    selected(bridge.get_discrete(param_id))
{
    setWantsKeyboardFocus(false);
}


void Selector::set_mirror(Synth::ParamId const mirror_param_id)
{
    this->mirror_param_id = mirror_param_id;
}


juce::String Selector::option_label(int const index) const
{
    if (index >= 0 && index < options.size()) {
        return options[index];
    }

    return juce::String(index);
}


void Selector::refresh()
{
    int const live = bridge.get_discrete(param_id);

    if (live != selected) {
        selected = live;
        repaint();
    }
}


void Selector::paint(juce::Graphics& g)
{
    juce::Rectangle<int> b = getLocalBounds();

    if (caption.isNotEmpty()) {
        g.setColour(Theme::TEXT_DIM);
        g.setFont(11.0f);
        g.drawText(caption, b.removeFromTop(14), juce::Justification::centredLeft, false);
    }

    juce::Rectangle<float> const box = b.toFloat();
    g.setColour(Theme::INSET);
    g.fillRoundedRectangle(box, 2.0f);
    g.setColour(Theme::EDGE);
    g.drawRoundedRectangle(box.reduced(0.5f), 2.0f, 1.0f);

    juce::Rectangle<int> const text_area = b.reduced(7, 0);
    g.setColour(Theme::TEXT);
    g.setFont(12.0f);
    g.drawText(
        option_label(selected),
        text_area.withTrimmedRight(12),
        juce::Justification::centredLeft,
        true
    );

    /* Chevron. */
    float const cx = (float)text_area.getRight() - 4.0f;
    float const cy = (float)text_area.getCentreY();
    juce::Path chevron;
    chevron.startNewSubPath(cx - 5.0f, cy - 2.0f);
    chevron.lineTo(cx - 2.5f, cy + 2.0f);
    chevron.lineTo(cx, cy - 2.0f);
    g.setColour(Theme::TEXT_DIM);
    g.strokePath(chevron, juce::PathStrokeType(1.3f));
}


void Selector::mouseDown(juce::MouseEvent const& /* event */)
{
    juce::PopupMenu menu;
    int const count = bridge.option_count(param_id);

    for (int i = 0; i != count; ++i) {
        menu.addItem(i + 1, option_label(i), true, i == selected);
    }

    Selector* const self = this;
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(this),
        [self](int const result) {
            if (result > 0) {
                self->selected = result - 1;
                self->bridge.set_discrete(self->param_id, self->selected);

                if (self->mirror_param_id != NO_MIRROR) {
                    self->bridge.set_discrete(self->mirror_param_id, self->selected);
                }

                self->repaint();
            }
        }
    );
}

}

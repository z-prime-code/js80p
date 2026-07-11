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

#include "plugin/juce/PluginEditor.hpp"


namespace JS80P
{

/* Session-only user scale, shared across instances. See header. */
int JS80PEditor::shared_width = 0;
int JS80PEditor::shared_height = 0;
std::vector<JS80PEditor*> JS80PEditor::instances;
bool JS80PEditor::syncing = false;

JS80PEditor::JS80PEditor(JS80PProcessor& processor)
    : juce::AudioProcessorEditor(&processor),
    processor(processor),
    gui(nullptr),
    matrix_active(false),
    in_resize(false)
{
    setResizable(true, true);

    juce::ComponentBoundsConstrainer* const constrainer = getConstrainer();

    /* The new GUI is laid out once at a fixed base resolution and drawn through a
     * uniform scale transform in resized(); the window is aspect-locked to that
     * ratio, so resizing zooms the whole UI instead of reflowing it. */
    int const bw = base_width();
    int const bh = base_height();

    if (constrainer != nullptr) {
        constrainer->setFixedAspectRatio((double)bw / (double)bh);
        /* 0.5x .. 2.0x scale range; the default below is 1.0x. */
        constrainer->setSizeLimits(bw / 2, bh / 2, bw * 2, bh * 2);
    }

    /* Start at the size last chosen this session (shared across instances), or the
     * 1.0x default if unset. JUCE clamps it to the constraints above. */
    if (shared_width > 0 && shared_height > 0) {
        setSize(shared_width, shared_height);
    } else {
        setSize(bw, bh);
    }

    instances.push_back(this);

    /* Original GUI (JUCE Widget backend). Its root is reparented into matrix_host
     * so the new GUI can embed it, contained, under its MATRIX tab. */
    gui = new JS80P::GUI(
        JucePlugin_VersionString,
        nullptr,
        (JS80P::GUI::PlatformWidget)(juce::Component*)this,
        processor.get_synth(),
        true,
        this
    );
    gui->show();

    /* Host for the embedded legacy GUI (hidden until the MATRIX tab is active). */
    addChildComponent(matrix_host);

    if (juce::Component* const legacy_root =
            (juce::Component*)gui->get_root_platform_widget()) {
        matrix_host.addChildComponent(legacy_root);
    }

    /* New simplified GUI — opaque, on top, shown by default. */
    new_gui = std::make_unique<NewGui>(processor.get_synth());
    new_gui->on_matrix = [this](bool const active) { set_matrix(active); };
    addAndMakeVisible(*new_gui);

    startTimerHz((int)JS80P::GUI::REFRESH_RATE);

    resized();
}


JS80PEditor::~JS80PEditor()
{
    stopTimer();

    instances.erase(
        std::remove(instances.begin(), instances.end(), this), instances.end()
    );

    new_gui = nullptr;

    delete gui;
    gui = nullptr;
}


void JS80PEditor::sync_all_to_shared(JS80PEditor const* const source)
{
    if (syncing || shared_width <= 0 || shared_height <= 0) {
        return;
    }

    syncing = true;

    for (JS80PEditor* const editor : instances) {
        if (editor == source) {
            continue;
        }

        bool const needs_resize =
            editor->getWidth() != shared_width || editor->getHeight() != shared_height;

        if (needs_resize) {
            editor->setSize(shared_width, shared_height);
        }
    }

    syncing = false;
}


void JS80PEditor::set_matrix(bool const active)
{
    matrix_active = active;
    matrix_host.setVisible(active);

    if (active) {
        gui->show();
        layout_matrix();
        matrix_host.toFront(false);   /* over the new GUI's body, below its header */
    }
}


void JS80PEditor::layout_matrix()
{
    if (gui == nullptr || in_resize) {
        return;
    }

    /* The body sits below the header, which is 46 px at base resolution but drawn
     * scaled (see resized()); matrix_host is an untransformed sibling, so clear
     * the scaled header height. Fit the legacy GUI within the body, aspect-locked,
     * then centre it. */
    double const scale = (double)getWidth() / (double)base_width();
    int const header_h = juce::roundToInt(46.0 * scale);
    juce::Rectangle<int> const body(
        0, header_h, getWidth(), juce::jmax(0, getHeight() - header_h)
    );

    in_resize = true;
    gui->resize(body.getWidth(), body.getHeight());
    in_resize = false;

    juce::Rectangle<int> scaled(0, 0, gui->get_width(), gui->get_height());
    scaled.setCentre(body.getCentre());
    matrix_host.setBounds(scaled);   /* the legacy root fills this from (0, 0) */
}


void JS80PEditor::resized()
{
    if (new_gui == nullptr || !new_gui->has_painted) {
        /* Before first paint, a size differing from the shared scale is the host
         * restoring its stale saved size — override it back, don't record it. */
        if (shared_width > 0
                && (getWidth() != shared_width || getHeight() != shared_height)) {
            setSize(shared_width, shared_height);
            return;   /* setSize re-enters resized(); layout runs there. */
        }
    }
    else if (!syncing && getWidth() > 0 && getHeight() > 0) {
        /* After first paint, any resize is a user rescale (aspect-locked): record
         * it and push it onto the other live editors, hidden ones included, so
         * they show at this size when revealed. Skipped while we drive them. */
        shared_width = getWidth();
        shared_height = getHeight();
        sync_all_to_shared(this);
    }

    if (new_gui != nullptr) {
        /* Lay the new GUI out at the base resolution and scale it as a unit. */
        double const scale = (double)getWidth() / (double)base_width();

        new_gui->setBounds(0, 0, base_width(), base_height());
        new_gui->setTransform(juce::AffineTransform::scale((float)scale));
    }

    if (matrix_active) {
        layout_matrix();
        matrix_host.toFront(false);
    }
}


void JS80PEditor::timerCallback()
{
    if (gui != nullptr) {
        gui->idle();
    }
}


void JS80PEditor::handle_resize_request(
        int const new_width, int const new_height
) {
    setSize(new_width, new_height);
}

}

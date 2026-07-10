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

JS80PEditor::JS80PEditor(JS80PProcessor& processor)
    : juce::AudioProcessorEditor(&processor),
    processor(processor),
    gui(nullptr),
    matrix_active(false),
    in_resize(false)
{
    setResizable(true, true);

    juce::ComponentBoundsConstrainer* const constrainer = getConstrainer();

    /* The new GUI no longer reserves a blank value strip under each knob, so its
     * content is shorter than the legacy WIDTH:HEIGHT ratio. Lock the window to a
     * design height trimmed by 53 px (~28 px at the default scale below) so the
     * plugin is that much shorter at the same width. */
    int const design_width = JS80P::GUI::WIDTH;
    int const design_height = JS80P::GUI::HEIGHT - 53;

    if (constrainer != nullptr) {
        constrainer->setFixedAspectRatio(
            (double)design_width / (double)design_height
        );
        constrainer->setSizeLimits(
            design_width / 4,
            design_height / 4,
            design_width,
            design_height
        );
    }

    /* Larger default than the legacy INIT_SCALE (0.48) so the new GUI's
     * per-oscillator pulse-width / harmonics section is visible without
     * resizing. The scale (0.527) keeps the same width as before; the trimmed
     * design height above makes the window ~28px shorter. */
    setSize((int)(0.527 * (double)design_width), (int)(0.527 * (double)design_height));

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

    new_gui = nullptr;

    delete gui;
    gui = nullptr;
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

    /* The body area sits below the new GUI's header (46 px). Scale the legacy GUI
     * to fit within it, aspect-locked (contain), then centre it — the surrounding
     * area is left empty. */
    int const header_h = 46;
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
    if (new_gui != nullptr) {
        new_gui->setBounds(getLocalBounds());
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

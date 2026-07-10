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

#ifndef JS80P__PLUGIN__JUCE__PLUGIN_EDITOR_HPP
#define JS80P__PLUGIN__JUCE__PLUGIN_EDITOR_HPP

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "gui/gui.hpp"

#include "plugin/juce/PluginProcessor.hpp"

#include "ui/new_gui.hpp"


namespace JS80P
{

/**
 * \brief Hosts both editors in one window: the new simplified GUI (NewGui) on
 *        top, and the original toolkit-agnostic JS80P::GUI (JUCE Widget backend)
 *        behind it, switched by an always-on-top toggle. Drives GUI::idle() from
 *        a juce::Timer and relays resize both ways.
 */
class JS80PEditor : public juce::AudioProcessorEditor,
                    public juce::Timer,
                    public JS80P::GUI::EventHandler
{
    public:
        explicit JS80PEditor(JS80PProcessor& processor);
        ~JS80PEditor() override;

        void resized() override;
        void timerCallback() override;

        void handle_resize_request(
            int const new_width, int const new_height
        ) override;

    private:
        /* Show / hide + size the embedded legacy GUI when the MATRIX tab of the
         * new GUI is entered / left. */
        void set_matrix(bool const active);
        void layout_matrix();

        JS80PProcessor& processor;
        JS80P::GUI* gui;
        std::unique_ptr<NewGui> new_gui;
        /* Container that hosts the (reparented) legacy GUI root under the MATRIX
         * tab, sized to fit the body area below the header (background-size:
         * contain). */
        juce::Component matrix_host;
        bool matrix_active;
        bool in_resize;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JS80PEditor)
};

}

#endif

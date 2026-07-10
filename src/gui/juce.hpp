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

#ifndef JS80P__GUI__JUCE_HPP
#define JS80P__GUI__JUCE_HPP

#include <map>
#include <string>
#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

#include "js80p.hpp"
#include "serializer.hpp"
#include "synth.hpp"

#include "gui/gui.hpp"


namespace JS80P
{

/**
 * \brief Per-plugin platform object for the JUCE GUI backend: a font cache and
 *        the name -> embedded-PNG lookup table. Analogous to \c Win32Platform.
 *        Stored in \c GUI::platform_data.
 */
class JucePlatform
{
    public:
        JucePlatform();

        juce::Font get_font(
            int const size_px,
            WidgetBase::FontWeight const weight
        );

        /**
         * \brief Decode the embedded PNG registered under \c name (one of the
         *        uppercase keys from \c gui.rc) into a freshly allocated
         *        \c juce::Image. Returns \c nullptr when unknown / undecodable.
         */
        juce::Image* load_image(char const* const name) const;

    private:
        typedef std::pair<int, int> FontCacheKey;

        std::map<FontCacheKey, juce::Font> font_cache;
        std::map<std::string, std::pair<char const*, int> > images;
};


/**
 * \brief The single JUCE backend for the toolkit-agnostic GUI.
 *
 * Each \c Widget owns an inner \c juce::Component (composition, not inheritance,
 * to avoid \c paint()/name clashes between the two frameworks). The inner
 * component forwards its paint/mouse callbacks to the public \c on_* / \c render
 * forwarders below, which dispatch to the \c WidgetBase virtual handlers; the
 * drawing primitives are routed to the live \c juce::Graphics. Replaces the
 * win32/xcb/macos backends.
 */
class Widget : public WidgetBase
{
    public:
        static juce::Colour to_colour(GUI::Color const color);

        explicit Widget(char const* const text);
        virtual ~Widget();

        virtual GUI::Image load_image(
            GUI::PlatformData platform_data,
            char const* const name
        ) override;

        virtual GUI::Image copy_image_region(
            GUI::Image source,
            int const left,
            int const top,
            int const width,
            int const height
        ) override;

        virtual GUI::Image downscale_image(
            GUI::Image source,
            int const old_width,
            int const old_height,
            int const new_width,
            int const new_height
        ) override;

        virtual void delete_image(GUI::Image image) override;

        virtual uint64_t monotonic_clock_ms() override;

        virtual void show() override;
        virtual void hide() override;
        virtual void focus() override;
        virtual void bring_to_top() override;
        virtual void redraw() override;

        virtual void set_scale(Number const new_scale) override;

        /* Forwarders invoked by the inner juce::Component (see juce.cpp). */
        void render(juce::Graphics& g);
        void on_mouse_down(int const x, int const y);
        void on_mouse_up(int const x, int const y);
        void on_mouse_move(int const x, int const y, bool const modifier);
        void on_mouse_leave(int const x, int const y);
        void on_double_click();
        void on_mouse_wheel(Number const delta, bool const modifier);

    protected:
        Widget(
            char const* const text,
            int const left,
            int const top,
            int const width,
            int const height,
            Type const type
        );

        Widget(
            GUI::PlatformData platform_data,
            GUI::PlatformWidget platform_widget,
            Type const type
        );

        virtual void set_up(
            GUI::PlatformData platform_data,
            WidgetBase* const parent
        ) override;

        virtual void fill_rectangle(
            int const left,
            int const top,
            int const width,
            int const height,
            GUI::Color const color
        ) override;

        virtual void draw_text(
            char const* const text,
            int const font_size_px,
            int const left,
            int const top,
            int const width,
            int const height,
            GUI::Color const color,
            GUI::Color const background,
            FontWeight const font_weight = FontWeight::NORMAL,
            int const padding = 0,
            TextAlignment const alignment = TextAlignment::CENTER
        ) override;

        virtual void draw_image(
            GUI::Image image,
            int const left,
            int const top,
            int const width,
            int const height
        ) override;

        /* The inner JUCE component (== platform_widget). */
        juce::Component* component;

    private:
        /* false only for the adopt-ctor wrapping the host-owned editor window. */
        bool owns_component;

        /* Valid only for the duration of a render() callback (the hdc analog). */
        juce::Graphics* graphics;
};

}

#include "gui/widgets.hpp"

#endif

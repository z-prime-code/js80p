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

#ifndef JS80P__GUI__JUCE_CPP
#define JS80P__GUI__JUCE_CPP

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#include "gui/juce.hpp"

#include "BinaryData.h"

#include "gui/gui.cpp"


namespace JS80P
{

/* Uppercase resource keys (see gui.rc) -> the embedded PNG's original file. */
static struct { char const* key; char const* file; } const JUCE_IMAGE_TABLE[] = {
    {"ABOUT",                "about.png"},
    {"EFFECTS",              "effects.png"},
    {"ENVELOPES1",           "envelopes1.png"},
    {"ENVELOPES2",           "envelopes2.png"},
    {"ENVSHAPES01",          "env_shapes-01.png"},
    {"ENVSHAPES10",          "env_shapes-10.png"},
    {"KNOBSTATESCONTROLLED", "knob_states-controlled.png"},
    {"KNOBSTATESFREE",       "knob_states-free.png"},
    {"KNOBSTATESNONE",       "knob_states-none.png"},
    {"KNOBSTATESRED",        "knob_states-red.png"},
    {"LFOS",                 "lfos.png"},
    {"MACROS1",              "macros1.png"},
    {"MACROS2",              "macros2.png"},
    {"MACROS3",              "macros3.png"},
    {"MACRODIST",            "macro_distortions.png"},
    {"MACROMID",             "macro_midpoint_states.png"},
    {"REVERSED",             "reversed.png"},
    {"SCREWSTATES",          "screw_states.png"},
    {"SCREWSTATESSYNCED",    "screw_states_synced.png"},
    {"SYNTH",                "synth.png"},
    {"VSTLOGO",              "vst_logo.png"},
};


/**
 * \brief Inner juce::Component owned by a Widget; forwards paint/mouse events
 *        back to its owner.
 */
class WidgetComponent : public juce::Component
{
    public:
        explicit WidgetComponent(Widget& owner) noexcept
            : owner(owner)
        {
            setWantsKeyboardFocus(false);
            setInterceptsMouseClicks(true, true);
        }

        void paint(juce::Graphics& g) override
        {
            owner.render(g);
        }

        void mouseDown(juce::MouseEvent const& event) override
        {
            owner.on_mouse_down(event.x, event.y);
        }

        void mouseUp(juce::MouseEvent const& event) override
        {
            owner.on_mouse_up(event.x, event.y);
        }

        void mouseMove(juce::MouseEvent const& event) override
        {
            owner.on_mouse_move(event.x, event.y, event.mods.isCtrlDown());
        }

        void mouseDrag(juce::MouseEvent const& event) override
        {
            owner.on_mouse_move(event.x, event.y, event.mods.isCtrlDown());
        }

        void mouseExit(juce::MouseEvent const& event) override
        {
            owner.on_mouse_leave(event.x, event.y);
        }

        void mouseDoubleClick(juce::MouseEvent const& event) override
        {
            owner.on_double_click();
        }

        void mouseWheelMove(
                juce::MouseEvent const& event,
                juce::MouseWheelDetails const& wheel
        ) override {
            owner.on_mouse_wheel((Number)wheel.deltaY, event.mods.isCtrlDown());
        }

    private:
        Widget& owner;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WidgetComponent)
};


JucePlatform::JucePlatform()
{
    for (auto const& entry : JUCE_IMAGE_TABLE) {
        for (int i = 0; i != BinaryData::namedResourceListSize; ++i) {
            if (std::strcmp(BinaryData::originalFilenames[i], entry.file) != 0) {
                continue;
            }

            int size = 0;
            char const* const data = BinaryData::getNamedResource(
                BinaryData::namedResourceList[i], size
            );

            if (data != nullptr) {
                images[entry.key] = std::make_pair(data, size);
            }

            break;
        }
    }
}


juce::Font JucePlatform::get_font(
        int const size_px,
        WidgetBase::FontWeight const weight
) {
    FontCacheKey const key(size_px, (int)weight);
    std::map<FontCacheKey, juce::Font>::const_iterator const it = (
        font_cache.find(key)
    );

    if (it != font_cache.end()) {
        return it->second;
    }

    /* 1.36 matches the win32 backend's Arial pixel-height metric. */
    float const height = (float)size_px * 1.36f;
    int const style = (
        weight == WidgetBase::FontWeight::BOLD
            ? juce::Font::bold
            : juce::Font::plain
    );
    juce::Font const font(juce::FontOptions("Arial", height, style));

    font_cache[key] = font;

    return font;
}


juce::Image* JucePlatform::load_image(char const* const name) const
{
    std::map<std::string, std::pair<char const*, int> >::const_iterator const it = (
        images.find(name)
    );

    if (it == images.end()) {
        return nullptr;
    }

    juce::Image const image = juce::ImageFileFormat::loadFrom(
        (void const*)it->second.first, (size_t)it->second.second
    );

    if (!image.isValid()) {
        return nullptr;
    }

    return new juce::Image(image);
}


juce::Colour Widget::to_colour(GUI::Color const color)
{
    return juce::Colour(
        (juce::uint8)GUI::red(color),
        (juce::uint8)GUI::green(color),
        (juce::uint8)GUI::blue(color)
    );
}


Widget::Widget(char const* const text)
    : WidgetBase(text),
    component(new WidgetComponent(*this)),
    owns_component(true),
    graphics(nullptr)
{
}


Widget::Widget(
        char const* const text,
        int const left,
        int const top,
        int const width,
        int const height,
        Type const type
) : WidgetBase(text, left, top, width, height, type),
    component(new WidgetComponent(*this)),
    owns_component(true),
    graphics(nullptr)
{
}


Widget::Widget(
        GUI::PlatformData platform_data,
        GUI::PlatformWidget platform_widget,
        Type const type
) : WidgetBase(platform_data, platform_widget, type),
    component((juce::Component*)platform_widget),
    owns_component(false),
    graphics(nullptr)
{
}


Widget::~Widget()
{
    destroy_children();

    if (owns_component) {
        delete component;
    }

    component = nullptr;
}


void Widget::set_up(GUI::PlatformData platform_data, WidgetBase* const parent)
{
    WidgetBase::set_up(platform_data, parent);

    component->setBounds(
        scale_value(left),
        scale_value(top),
        scale_value(width),
        scale_value(height)
    );

    platform_widget = (GUI::PlatformWidget)component;

    juce::Component* const parent_component = (
        (juce::Component*)parent->get_platform_widget()
    );

    if (parent_component != nullptr) {
        parent_component->addAndMakeVisible(component);
    }
}


GUI::Image Widget::load_image(
        GUI::PlatformData platform_data,
        char const* const name
) {
    JucePlatform const* const platform = (JucePlatform const*)platform_data;

    if (platform == nullptr) {
        return nullptr;
    }

    return (GUI::Image)platform->load_image(name);
}


GUI::Image Widget::copy_image_region(
        GUI::Image source,
        int const left,
        int const top,
        int const width,
        int const height
) {
    if (source == nullptr) {
        return nullptr;
    }

    juce::Image const* const src = (juce::Image const*)source;
    juce::Image const region = src->getClippedImage(
        juce::Rectangle<int>(left, top, width, height)
    ).createCopy();

    return (GUI::Image)new juce::Image(region);
}


GUI::Image Widget::downscale_image(
        GUI::Image source,
        int const old_width,
        int const old_height,
        int const new_width,
        int const new_height
) {
    if (source == nullptr) {
        return nullptr;
    }

    juce::Image const* const src = (juce::Image const*)source;

    return (GUI::Image)new juce::Image(
        src->rescaled(new_width, new_height, juce::Graphics::highResamplingQuality)
    );
}


void Widget::delete_image(GUI::Image image)
{
    if (image != nullptr) {
        delete (juce::Image*)image;
    }
}


uint64_t Widget::monotonic_clock_ms()
{
    return (uint64_t)juce::Time::getMillisecondCounter();
}


void Widget::show()
{
    WidgetBase::show();

    if (component != nullptr) {
        component->setVisible(true);
    }
}


void Widget::hide()
{
    WidgetBase::hide();

    if (component != nullptr) {
        component->setVisible(false);
    }
}


void Widget::focus()
{
    /*
     * Deliberately does NOT grab keyboard focus so host keyboard shortcuts
     * (e.g. the spacebar transport) keep working.
     */
    WidgetBase::focus();
}


void Widget::bring_to_top()
{
    WidgetBase::bring_to_top();

    if (component != nullptr) {
        component->toFront(false);
    }
}


void Widget::redraw()
{
    WidgetBase::redraw();

    if (component != nullptr) {
        component->repaint();
    }
}


void Widget::set_scale(Number const new_scale)
{
    WidgetBase::set_scale(new_scale);

    if (component != nullptr) {
        component->setBounds(
            scale_value(left),
            scale_value(top),
            scale_value(width),
            scale_value(height)
        );
    }
}


void Widget::render(juce::Graphics& g)
{
    graphics = &g;
    paint();
    graphics = nullptr;
}


void Widget::on_mouse_down(int const x, int const y)
{
    is_clicking = true;
    mouse_down(x, y);
}


void Widget::on_mouse_up(int const x, int const y)
{
    mouse_up(x, y);

    if (is_clicking) {
        is_clicking = false;
        click();
    }
}


void Widget::on_mouse_move(int const x, int const y, bool const modifier)
{
    mouse_move(x, y, modifier);
}


void Widget::on_mouse_leave(int const x, int const y)
{
    mouse_leave(x, y);
}


void Widget::on_double_click()
{
    double_click();
}


void Widget::on_mouse_wheel(Number const delta, bool const modifier)
{
    mouse_wheel(delta, modifier);
}


void Widget::fill_rectangle(
        int const left,
        int const top,
        int const width,
        int const height,
        GUI::Color const color
) {
    if (graphics == nullptr) {
        return;
    }

    graphics->setColour(to_colour(color));
    graphics->fillRect(left, top, width, height);
}


void Widget::draw_text(
        char const* const text,
        int const font_size_px,
        int const left,
        int const top,
        int const width,
        int const height,
        GUI::Color const color,
        GUI::Color const background,
        FontWeight const font_weight,
        int const padding,
        TextAlignment const alignment
) {
    if (graphics == nullptr) {
        return;
    }

    JucePlatform* const platform = (JucePlatform*)platform_data;

    graphics->setColour(to_colour(background));
    graphics->fillRect(left, top, width, height);

    if (platform != nullptr) {
        graphics->setFont(platform->get_font(font_size_px, font_weight));
    }

    graphics->setColour(to_colour(color));

    juce::Justification const justification = (
        alignment == TextAlignment::RIGHT
            ? juce::Justification::centredRight
            : alignment == TextAlignment::LEFT
                ? juce::Justification::centredLeft
                : juce::Justification::centred
    );

    graphics->drawText(
        juce::String::fromUTF8(text),
        left + padding,
        top,
        width - 2 * padding,
        height,
        justification,
        false
    );
}


void Widget::draw_image(
        GUI::Image image,
        int const left,
        int const top,
        int const width,
        int const height
) {
    if (graphics == nullptr || image == nullptr) {
        return;
    }

    juce::Image const* const img = (juce::Image const*)image;

    graphics->drawImage(
        *img,
        left, top, width, height,
        0, 0, img->getWidth(), img->getHeight()
    );
}


void GUI::initialize()
{
    platform_data = (PlatformData)(new JucePlatform());
}


void GUI::destroy()
{
    delete (JucePlatform*)platform_data;
    platform_data = NULL;
}

}


#include "gui/widgets.hpp"


namespace JS80P
{

void ImportPatchButton::click()
{
    std::shared_ptr<juce::FileChooser> const chooser = (
        std::make_shared<juce::FileChooser>(
            "Import patch", juce::File(), "*.js80p"
        )
    );

    ImportPatchButton* const self = this;

    chooser->launchAsync(
        juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles,
        [self, chooser](juce::FileChooser const& fc) {
            juce::File const file = fc.getResult();

            if (file == juce::File()) {
                return;
            }

            juce::MemoryBlock block;

            if (!file.loadFileAsData(block)) {
                return;
            }

            Integer const size = (Integer)std::min(
                (size_t)Serializer::MAX_SIZE, block.getSize()
            );

            char* const buffer = new char[Serializer::MAX_SIZE];
            std::fill_n(buffer, Serializer::MAX_SIZE, '\x00');
            std::memcpy(buffer, block.getData(), (size_t)size);

            self->import_patch(buffer, size);

            delete[] buffer;
        }
    );
}


void ExportPatchButton::click()
{
    std::shared_ptr<juce::FileChooser> const chooser = (
        std::make_shared<juce::FileChooser>(
            "Export patch", juce::File(), "*.js80p"
        )
    );

    ExportPatchButton* const self = this;

    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [self, chooser](juce::FileChooser const& fc) {
            juce::File file = fc.getResult();

            if (file == juce::File()) {
                return;
            }

            if (file.getFileExtension().isEmpty()) {
                file = file.withFileExtension("js80p");
            }

            std::string const patch = Serializer::serialize(self->synth);

            file.replaceWithText(
                juce::String::fromUTF8(patch.data(), (int)patch.size())
            );
        }
    );
}


void GUI::idle()
{
    handle_scheduled_resize();

    if (background != NULL) {
        background->refresh();
    }
}

}


#include "gui/widgets.cpp"

#endif

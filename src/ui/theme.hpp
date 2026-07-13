#ifndef JS80P__UI__THEME_HPP
#define JS80P__UI__THEME_HPP

#include <juce_graphics/juce_graphics.h>


namespace JS80P
{

/**
 * \brief Colour tokens for the new GUI (dark theme), matching the tokens in
 *        gui/mockup-new.html. Light theme + a runtime toggle come later.
 */
namespace Theme
{
    inline juce::Colour const BG          { 0xff0e0e15 };
    inline juce::Colour const PANEL       { 0xff191922 };
    inline juce::Colour const PANEL_2     { 0xff21212d };
    inline juce::Colour const INSET       { 0xff131319 };
    inline juce::Colour const EDGE        { 0xff32323e };
    inline juce::Colour const EDGE_SOFT   { 0xff26262f };
    inline juce::Colour const TEXT        { 0xffc8c8d4 };
    inline juce::Colour const TEXT_DIM    { 0xff7d7d90 };
    inline juce::Colour const TEXT_FAINT  { 0xff56566a };
    inline juce::Colour const ACCENT      { 0xffeab364 };
    inline juce::Colour const TRACK       { 0xff34343f };

    inline juce::Colour const SHADOW      { 0xff000000 };

    /* Modulation source colours (by type). */
    inline juce::Colour const ENV         { 0xff5bc79a };
    inline juce::Colour const LFO         { 0xff5a9fe6 };
    inline juce::Colour const MACRO       { 0xffe8944a };
    inline juce::Colour const MIDI        { 0xffe070a8 };
    inline juce::Colour const EXPR        { 0xffa888e4 };

    inline constexpr float RADIUS = 4.0f;   /* outer panels */
}

}

#endif

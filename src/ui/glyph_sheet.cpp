#include "ui/glyph_sheet.hpp"

#include "BinaryData.h"


namespace JS80P
{

juce::Image const& GlyphSheet::sheet()
{
    if (!sheet_loaded) {
        sheet_image = juce::ImageFileFormat::loadFrom(
            (void const*)BinaryData::synth_png, (size_t)BinaryData::synth_pngSize
        );
        sheet_loaded = true;
    }

    return sheet_image;
}


juce::Image GlyphSheet::glyph(int const x, int const y, int const w, int const h)
{
    std::array<int, 4> const key { x, y, w, h };

    auto const cached = glyphs.find(key);

    if (cached != glyphs.end()) {
        return cached->second;
    }

    juce::Image const& src_sheet = sheet();

    if (!src_sheet.isValid()) {
        return juce::Image();
    }

    juce::Image glyph(juce::Image::ARGB, w, h, true);

    for (int j = 0; j != h; ++j) {
        for (int i = 0; i != w; ++i) {
            juce::Colour const src = src_sheet.getPixelAt(x + i, y + j);
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

    glyphs.emplace(key, glyph);

    return glyph;
}

}

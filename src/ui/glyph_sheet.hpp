#ifndef JS80P__UI__GLYPH_SHEET_HPP
#define JS80P__UI__GLYPH_SHEET_HPP

#include <array>
#include <map>

#include <juce_graphics/juce_graphics.h>


namespace JS80P
{

/**
 * \brief Decodes the embedded \c synth.png sprite once and caches the tinted
 *        icon glyphs cropped from it, so repeatedly opening the editor does not
 *        re-decode the sprite on every GUI construction.
 *
 *        Held through a \c juce::SharedResourcePointer<GlyphSheet>: the first
 *        holder constructs it, the last holder destroys it. \c JS80PProcessor
 *        keeps one for its whole lifetime (see PluginProcessor.hpp), so the
 *        cache is shared across all plugin instances, survives editor
 *        open/close/reopen, and — critically — is released when the *last*
 *        plugin instance is destroyed, i.e. while JUCE's graphics runtime is
 *        still alive. A plain function-local \c static would instead be
 *        destroyed at DLL-unload / \c atexit, where releasing the (Direct2D /
 *        D3D11-backed) images hangs the host.
 *
 *        Not thread safe: the lazy decode and the cache are only touched from
 *        the message thread (GUI construction).
 */
class GlyphSheet
{
    public:
        /**
         * Crop a sub-rectangle of \c synth.png and turn it into a white icon
         * whose alpha channel is the sprite's luminance, so it can be tinted to
         * any colour (the sprite's glyphs are light line-art on a near-black
         * background). Decodes the sprite on first use and caches the result
         * per crop rectangle; later calls with the same rectangle return the
         * cached (shallow-copied) image.
         */
        juce::Image glyph(int const x, int const y, int const w, int const h);

    private:
        /* Lazily-decoded full sprite; invalid if decoding failed. */
        juce::Image const& sheet();

        juce::Image sheet_image;
        bool sheet_loaded = false;

        /* Extracted glyphs keyed by their {x, y, w, h} crop rectangle. */
        std::map<std::array<int, 4>, juce::Image> glyphs;
};

}

#endif

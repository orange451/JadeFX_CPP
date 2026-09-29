#pragma once

#include "internal/Subpixel.hpp"

namespace jadefx {

struct FontFace;

// Coverage for one glyph at pixelSize, with its pen phase / kSubpixelPhases
// pixels right of a whole pixel. rgb holds three bytes per pixel: red, green, and blue stripe
// coverage for lcd, or the same gray value three times otherwise. xoff and yoff
// place the top left pixel relative to the pen pixel and the baseline.
//
// Windows rasterizes with DirectWrite, which applies the font's vertical hints
// and its own ClearType filter, as Edge and Chrome do. Linux and Android use
// FreeType with light hinting, as Chrome does there. Elsewhere, or when the
// platform rasterizer cannot read the font, stb_truetype draws an unhinted outline.
SubpixelBitmap RasterizeGlyph(const FontFace& face, int codepoint, int pixelSize, int phase, bool lcd);

}  // namespace jadefx

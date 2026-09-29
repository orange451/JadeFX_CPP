#include "GlyphRaster.hpp"

#include "FontInternal.hpp"

#include <algorithm>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwrite_3.h>
#include <wrl/client.h>
#endif

namespace jadefx {

#if defined(_WIN32)

using Microsoft::WRL::ComPtr;

struct NativeFace {
    ComPtr<IDWriteFontFace> face;
};

namespace {

struct DirectWrite {
    ComPtr<IDWriteFactory5> factory;
    ComPtr<IDWriteInMemoryFontFileLoader> loader;
};

// The in-memory loader needs Windows 10 1703. Older systems keep stb_truetype.
DirectWrite* SharedDirectWrite() {
    static DirectWrite* shared = [] {
        auto* dw = new DirectWrite();
        ComPtr<IUnknown> unknown;
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory5), &unknown)) ||
            FAILED(unknown.As(&dw->factory)) || FAILED(dw->factory->CreateInMemoryFontFileLoader(&dw->loader)) ||
            FAILED(dw->factory->RegisterFontFileLoader(dw->loader.Get()))) {
            delete dw;
            return static_cast<DirectWrite*>(nullptr);
        }
        return dw;
    }();
    return shared;
}

IDWriteFontFace* NativeFontFace(const FontFace& face) {
    if (face.native) {
        return face.native->face.Get();
    }
    face.native = std::make_shared<NativeFace>();
    DirectWrite* dw = SharedDirectWrite();
    if (dw == nullptr || face.bytes.empty()) {
        return nullptr;
    }
    // A null owner makes DirectWrite keep its own copy of the bytes.
    ComPtr<IDWriteFontFile> file;
    if (FAILED(dw->loader->CreateInMemoryFontFileReference(dw->factory.Get(), face.bytes.data(),
                                                           static_cast<UINT32>(face.bytes.size()), nullptr, &file))) {
        return nullptr;
    }
    BOOL supported = FALSE;
    DWRITE_FONT_FILE_TYPE fileType = DWRITE_FONT_FILE_TYPE_UNKNOWN;
    DWRITE_FONT_FACE_TYPE faceType = DWRITE_FONT_FACE_TYPE_UNKNOWN;
    UINT32 faceCount = 0;
    if (FAILED(file->Analyze(&supported, &fileType, &faceType, &faceCount)) || !supported) {
        return nullptr;
    }
    IDWriteFontFile* files[] = {file.Get()};
    dw->factory->CreateFontFace(faceType, 1, files, 0, DWRITE_FONT_SIMULATIONS_NONE, &face.native->face);
    return face.native->face.Get();
}

bool RasterizeDirectWrite(const FontFace& face, int codepoint, int pixelSize, int phase, bool lcd,
                          SubpixelBitmap& image) {
    IDWriteFontFace* fontFace = NativeFontFace(face);
    if (fontFace == nullptr) {
        return false;
    }
    const UINT32 point = static_cast<UINT32>(codepoint);
    UINT16 index = 0;
    if (FAILED(fontFace->GetGlyphIndices(&point, 1, &index))) {
        return false;
    }
    const FLOAT advance = 0.f;
    const DWRITE_GLYPH_OFFSET offset{};
    DWRITE_GLYPH_RUN run{};
    run.fontFace = fontFace;
    run.fontEmSize = static_cast<FLOAT>(pixelSize);
    run.glyphCount = 1;
    run.glyphIndices = &index;
    run.glyphAdvances = &advance;
    run.glyphOffsets = &offset;

    // Chrome's choice for LCD text: outlines keep their natural width, while
    // the font's hints still snap baselines, x-heights, and crossbars in y.
    ComPtr<IDWriteGlyphRunAnalysis> analysis;
    const HRESULT made = SharedDirectWrite()->factory->CreateGlyphRunAnalysis(
        &run, nullptr, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, DWRITE_MEASURING_MODE_NATURAL,
        DWRITE_GRID_FIT_MODE_DEFAULT, lcd ? DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE : DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE,
        static_cast<FLOAT>(phase) / static_cast<FLOAT>(kSubpixelPhases), 0.f, &analysis);
    if (FAILED(made)) {
        return false;
    }
    // Grayscale analysis returns its 0 to 255 coverage as the aliased texture.
    const DWRITE_TEXTURE_TYPE texture = lcd ? DWRITE_TEXTURE_CLEARTYPE_3x1 : DWRITE_TEXTURE_ALIASED_1x1;
    RECT bounds{};
    if (FAILED(analysis->GetAlphaTextureBounds(texture, &bounds))) {
        return false;
    }
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0) {
        return true;
    }
    const int channels = lcd ? 3 : 1;
    std::vector<BYTE> alpha(static_cast<std::size_t>(width * height * channels));
    if (FAILED(analysis->CreateAlphaTexture(texture, &bounds, alpha.data(), static_cast<UINT32>(alpha.size())))) {
        return false;
    }
    image.xoff = bounds.left;
    image.yoff = bounds.top;
    image.width = width;
    image.height = height;
    if (lcd) {
        image.rgb.assign(alpha.begin(), alpha.end());
    } else {
        image.rgb.resize(static_cast<std::size_t>(width * height * 3));
        for (std::size_t i = 0; i < alpha.size(); ++i) {
            std::fill_n(image.rgb.begin() + static_cast<std::ptrdiff_t>(i * 3), 3, alpha[i]);
        }
    }
    return true;
}

}  // namespace

#endif

namespace {

SubpixelBitmap RasterizeStb(const FontFace& face, int codepoint, int pixelSize, int phase, bool lcd) {
    const float raster = stbtt_ScaleForMappingEmToPixels(&face.info, static_cast<float>(pixelSize));
    // Where this glyph sits inside its pixel.
    const float shift = static_cast<float>(phase) / static_cast<float>(kSubpixelPhases);
    if (lcd) {
        // Rasterize one sample per stripe.
        int ix0 = 0;
        int iy0 = 0;
        int ix1 = 0;
        int iy1 = 0;
        stbtt_GetCodepointBitmapBoxSubpixel(&face.info, codepoint, raster * 3.f, raster, shift * 3.f, 0.f, &ix0, &iy0,
                                            &ix1, &iy1);
        const int sampleWidth = std::max(0, ix1 - ix0);
        const int sampleHeight = std::max(0, iy1 - iy0);
        std::vector<unsigned char> samples;
        if (sampleWidth > 0 && sampleHeight > 0) {
            samples.assign(static_cast<std::size_t>(sampleWidth * sampleHeight), 0);
            stbtt_MakeCodepointBitmapSubpixel(&face.info, samples.data(), sampleWidth, sampleHeight, sampleWidth,
                                              raster * 3.f, raster, shift * 3.f, 0.f, codepoint);
        }
        return PackSubpixelCoverage(samples.empty() ? nullptr : samples.data(), sampleWidth, sampleHeight,
                                    sampleWidth, ix0, iy0);
    }
    SubpixelBitmap image;
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    stbtt_GetCodepointBitmapBoxSubpixel(&face.info, codepoint, raster, raster, shift, 0.f, &x0, &y0, &x1, &y1);
    const int width = std::max(0, x1 - x0);
    const int height = std::max(0, y1 - y0);
    image.xoff = x0;
    image.yoff = y0;
    if (width <= 0 || height <= 0) {
        return image;
    }
    std::vector<unsigned char> coverage(static_cast<std::size_t>(width * height));
    stbtt_MakeCodepointBitmapSubpixel(&face.info, coverage.data(), width, height, width, raster, raster, shift, 0.f,
                                      codepoint);
    image.width = width;
    image.height = height;
    image.rgb.resize(static_cast<std::size_t>(width * height * 3));
    for (std::size_t i = 0; i < coverage.size(); ++i) {
        std::fill_n(image.rgb.begin() + static_cast<std::ptrdiff_t>(i * 3), 3, coverage[i]);
    }
    return image;
}

}  // namespace

SubpixelBitmap RasterizeGlyph(const FontFace& face, int codepoint, int pixelSize, int phase, bool lcd) {
#if defined(_WIN32)
    SubpixelBitmap image;
    if (RasterizeDirectWrite(face, codepoint, pixelSize, phase, lcd, image)) {
        return image;
    }
#endif
    return RasterizeStb(face, codepoint, pixelSize, phase, lcd);
}

}  // namespace jadefx

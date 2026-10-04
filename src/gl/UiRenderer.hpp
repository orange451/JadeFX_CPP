#pragma once

#include "gl/UniformCache.hpp"
#include "jadefx/paint/Color.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace jadefx {

struct FontFace;
struct ImageData;

// Draws rounded rectangles, text, and bitmaps in window points. The GL context is current.
class UiRenderer {
public:
    bool initialize();
    void shutdown();

    // clearColor false leaves the framebuffer contents in place.
    void begin(int framebufferWidth, int framebufferHeight, float pixelsPerPoint, const Color& clear, bool clearColor);
    // stops are along the gradient. One stop draws a solid color. stopAt is 0 at the start.
    void fillRounded(float x, float y, float width, float height, const float radius[4], const Color* stops,
                     const float* stopAt, int stopCount, float angleDeg);
    // A solid rectangle with exact edges: sharp on whole pixels, so one beside
    // it on the same pixel edge meets it without a seam, for grids of cells.
    void fillRect(float x, float y, float width, float height, const Color& color);
    // sides are top, right, bottom, left, in points.
    void strokeRounded(float x, float y, float width, float height, const float radius[4], const float sides[4],
                       const Color& color);
    void outerShadow(float x, float y, float width, float height, const float radius[4], float offsetX, float offsetY,
                     float blur, float spread, const Color& color);
    void innerShadow(float x, float y, float width, float height, const float radius[4], float offsetX, float offsetY,
                     float blur, float spread, const Color& color);
    // subpixel uses stripe coverage when the context can blend it. Otherwise the glyph is grayscale.
    void text(float x, float y, const std::string& utf8, const std::string& family, float fontSize, const Color& color,
              bool subpixel);
    // Straight-alpha RGBA, top row first. x and y are the top left in window points.
    // A tint draws only the image's alpha, filled with that color.
    void drawImage(const std::shared_ptr<ImageData>& image, float x, float y, float width, float height, float opacity,
                   const Color* tint = nullptr);
    // Clip later draws to this rectangle in window points, origin top left.
    // Clips nest by intersection. popClip restores the previous one.
    void pushClip(float x, float y, float width, float height);
    void popClip();
    // Device pixels in one point, as begin set it.
    float pixelsPerPoint() const { return scale_; }
    void end();
    bool writePpm(const char* path) const;

private:
    struct Glyph;

    void drawBox(float x, float y, float width, float height, float boxX, float boxY, float boxW, float boxH,
                 const float radius[4], const Color* stops, const float* stopAt, int stopCount, float mode,
                 const float sides[4], float blur, float angleDeg, const float* clip = nullptr,
                 const float* clipRadii = nullptr, bool exact = false);
    const Glyph* glyphFor(int codepoint, int pixelSize, int phase, const struct FontFace* face, bool wantSubpixel);
    unsigned imageTexture(const std::shared_ptr<ImageData>& image);

    unsigned boxProgram_ = 0;
    unsigned textProgram_ = 0;
    unsigned boxVao_ = 0;
    unsigned boxVbo_ = 0;
    unsigned textVao_ = 0;
    unsigned textVbo_ = 0;
    unsigned atlas_ = 0;
    int viewportW_ = 0;
    int viewportH_ = 0;
    float scale_ = 1.f;
    bool ready_ = false;
    bool subpixel_ = false;

    int boxRect_ = -1;
    int boxViewport_ = -1;
    int boxBox_ = -1;
    int boxRadii_ = -1;
    int boxParams_ = -1;
    int boxBorder_ = -1;
    int boxClip_ = -1;
    int boxClipRadii_ = -1;
    int boxStopCount_ = -1;
    int boxStops_[8] = {};
    int boxStopAt_[8] = {};
    // The box program's uniforms as last set: a run of plain rectangles changes
    // only their place and color, so the rest are skipped.
    enum BoxSlot { kBoxRect, kBoxViewport, kBoxBox, kBoxRadii, kBoxParams, kBoxBorder, kBoxClip, kBoxClipRadii,
                   kBoxStopCount, kBoxStop0, kBoxStopAt0 = kBoxStop0 + 8, kBoxSlots = kBoxStopAt0 + 8 };
    UniformCache boxUniforms_{kBoxSlots};
    int textViewport_ = -1;
    int textColor_ = -1;
    int textGamma_ = -1;
    int textSampler_ = -1;
    unsigned imageProgram_ = 0;
    int imageViewport_ = -1;
    int imageOpacity_ = -1;
    int imageSampler_ = -1;
    int imageTint_ = -1;
    int imageTinted_ = -1;
    struct GpuImage {
        std::weak_ptr<ImageData> data;
        unsigned texture = 0;
    };
    std::vector<GpuImage> gpuImages_;

    struct Glyph {
        float u0 = 0;
        float v0 = 0;
        float u1 = 0;
        float v1 = 0;
        float xoff = 0;
        float yoff = 0;
        float width = 0;
        float height = 0;
        bool empty = true;
    };
    struct GlyphKey {
        const FontFace* face = nullptr;
        int pixelSize = 0;
        int codepoint = 0;
        int phase = 0;
        bool subpixel = false;
        bool operator<(const GlyphKey& other) const {
            if (face != other.face) {
                return std::less<const FontFace*>()(face, other.face);
            }
            if (pixelSize != other.pixelSize) {
                return pixelSize < other.pixelSize;
            }
            if (codepoint != other.codepoint) {
                return codepoint < other.codepoint;
            }
            if (phase != other.phase) {
                return phase < other.phase;
            }
            return subpixel < other.subpixel;
        }
    };
    struct Clip {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };
    std::vector<Clip> clips_;
    std::map<GlyphKey, Glyph> glyphs_;
    int atlasPenX_ = 1;
    int atlasPenY_ = 1;
    int atlasRowHeight_ = 0;
    bool atlasFull_ = false;
    static constexpr int kAtlas = 1024;
};

}  // namespace jadefx

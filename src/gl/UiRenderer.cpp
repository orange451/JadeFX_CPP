#include "UiRenderer.hpp"

#include "Resources.hpp"
#include "gl.hpp"
#include "internal/Subpixel.hpp"
#include "platform/ErrorDialog.hpp"
#include "scene/image/ImageData.hpp"
#include "scene/text/FontInternal.hpp"
#include "scene/text/GlyphRaster.hpp"
#include "jadefx/scene/text/Font.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace jadefx {
namespace {

// Skia's mask gamma as Chromium builds it for each platform (skia/BUILD.gn), so
// text weighs what it does in the browser there. Gamma 0 is the sRGB curve.
struct TextGamma {
    float gamma;
    float contrast;
};
#if defined(_WIN32)
constexpr TextGamma kStripeGamma{0.f, 1.f};
#elif defined(__APPLE__)
constexpr TextGamma kStripeGamma{0.f, 0.f};
#elif defined(__ANDROID__)
constexpr TextGamma kStripeGamma{1.4f, 0.f};
#else
constexpr TextGamma kStripeGamma{1.2f, 0.2f};
#endif
// Skia leaves grayscale masks uncorrected unless built with SK_GAMMA_APPLY_TO_A8, as Android is.
#if defined(__ANDROID__)
constexpr TextGamma kGrayGamma = kStripeGamma;
#else
constexpr TextGamma kGrayGamma{1.f, 0.f};
#endif

int Location(GLuint program, const char* name) {
    return glGetUniformLocation(program, name);
}

void ReportMissingShaders(const std::string& missing) {
    const bool one = missing.find('\n') == std::string::npos;
    const std::string message =
        std::string(one ? "A required shader file is missing:\n\n" : "Required shader files are missing:\n\n") +
        missing + "\n\nExpected in " + ShaderFileDirectory() + "\n\nJadeFX will quit.";
    std::fprintf(stderr, "%s\n", message.c_str());
    std::fflush(stderr);
    ShowErrorDialog("Cannot start", message);
}

}  // namespace

bool UiRenderer::initialize() {
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    if (version == nullptr) {
        std::fprintf(stderr, "No current OpenGL context.\n");
        return false;
    }
    const char* rendererName = renderer != nullptr ? reinterpret_cast<const char*>(renderer) : "(unknown renderer)";
    std::printf("OpenGL %s\n%s\n", reinterpret_cast<const char*>(version), rendererName);
    std::fflush(stdout);

    const std::string boxVertex = LoadShaderSource("box.vert");
    const std::string boxFragment = LoadShaderSource("box.frag");
    const std::string textVertex = LoadShaderSource("text.vert");
    const std::string textFragment = LoadShaderSource("text.frag");
    const std::string textGray = LoadShaderSource("text_gray.frag");
    const std::string imageVertex = LoadShaderSource("image.vert");
    const std::string imageFragment = LoadShaderSource("image.frag");
    std::string missing;
    auto note = [&](const char* name, const std::string& source) {
        if (!source.empty()) {
            return;
        }
        if (!missing.empty()) {
            missing.push_back('\n');
        }
        missing += name;
    };
    note("box.vert", boxVertex);
    note("box.frag", boxFragment);
    note("text.vert", textVertex);
    note("text.frag", textFragment);
    note("text_gray.frag", textGray);
    note("image.vert", imageVertex);
    note("image.frag", imageFragment);
    if (!missing.empty()) {
        ReportMissingShaders(missing);
        return false;
    }

    boxProgram_ = jadefx_LinkShaderProgram(boxVertex, boxFragment, "Box");
    boxUniforms_.reset();
    textProgram_ = jadefx_LinkShaderProgram(textVertex, textFragment, "Text");
    subpixel_ = textProgram_ != 0;
    if (!subpixel_) {
        std::fprintf(stderr, "LCD subpixel text is unavailable. Using grayscale coverage.\n");
        textProgram_ = jadefx_LinkShaderProgram(textVertex, textGray, "Text");
    }
    imageProgram_ = jadefx_LinkShaderProgram(imageVertex, imageFragment, "Image");
    if (boxProgram_ == 0 || textProgram_ == 0 || imageProgram_ == 0) {
        shutdown();
        return false;
    }

    boxRect_ = Location(boxProgram_, "uRect");
    boxViewport_ = Location(boxProgram_, "uViewport");
    boxBox_ = Location(boxProgram_, "uBox");
    boxRadii_ = Location(boxProgram_, "uRadii");
    boxParams_ = Location(boxProgram_, "uParams");
    boxBorder_ = Location(boxProgram_, "uBorder");
    boxClip_ = Location(boxProgram_, "uClip");
    boxClipRadii_ = Location(boxProgram_, "uClipRadii");
    boxStopCount_ = Location(boxProgram_, "uStopCount");
    for (int i = 0; i < 8; ++i) {
        const std::string stop = "uStops[" + std::to_string(i) + "]";
        const std::string at = "uStopAt[" + std::to_string(i) + "]";
        boxStops_[i] = Location(boxProgram_, stop.c_str());
        boxStopAt_[i] = Location(boxProgram_, at.c_str());
    }
    textViewport_ = Location(textProgram_, "uViewport");
    textColor_ = Location(textProgram_, "uColor");
    textGamma_ = Location(textProgram_, "uGamma");
    textSampler_ = Location(textProgram_, "uTex");
    imageViewport_ = Location(imageProgram_, "uViewport");
    imageOpacity_ = Location(imageProgram_, "uOpacity");
    imageSampler_ = Location(imageProgram_, "uTex");
    imageTint_ = Location(imageProgram_, "uTint");
    imageTinted_ = Location(imageProgram_, "uTinted");

    const float quad[] = {0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f};
    glGenVertexArrays(1, &boxVao_);
    glGenBuffers(1, &boxVbo_);
    glBindVertexArray(boxVao_);
    glBindBuffer(GL_ARRAY_BUFFER, boxVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * static_cast<GLsizei>(sizeof(float)), nullptr);

    glGenVertexArrays(1, &textVao_);
    glGenBuffers(1, &textVbo_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
    const GLsizei stride = 4 * static_cast<GLsizei>(sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);

    GLint maxTexture = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexture);
    atlasPacker_ = GlyphAtlasPacker(kAtlasStart, std::min(kAtlasMost, std::max(kAtlasStart, static_cast<int>(maxTexture))));
    glGenTextures(1, &atlas_);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    if (subpixel_) {
        // RGB coverage lives in rgb; stripes must not be filtered together.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    allocateAtlas(atlasPacker_.size());
    glBindTexture(GL_TEXTURE_2D, 0);

    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::fprintf(stderr, "OpenGL error during UI setup: 0x%x\n", error);
        shutdown();
        return false;
    }
    ready_ = true;
    return true;
}

void UiRenderer::allocateAtlas(int size) {
    // Blank, not left undefined: the texel between two glyphs must be empty, or
    // the grayscale atlas's linear filter would draw an edge of its neighbor.
    const int texelBytes = subpixel_ ? 4 : 1;
    const std::vector<unsigned char> blank(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) *
                                               static_cast<std::size_t>(texelBytes),
                                           0);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (subpixel_) {
        // RGBA keeps each row 4-byte aligned.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, blank.data());
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, size, size, 0, GL_RED, GL_UNSIGNED_BYTE, blank.data());
    }
}

void UiRenderer::resetAtlas(int size) {
    glyphs_.clear();
    atlasPacker_.clear(size);
    allocateAtlas(atlasPacker_.size());
    ++atlasGeneration_;
}

void UiRenderer::shutdown() {
    ready_ = false;
    glyphs_.clear();
    atlasPacker_ = GlyphAtlasPacker(kAtlasStart, kAtlasStart);
    atlasGaveUp_ = false;
    atlasWarned_ = false;
    if (atlas_ != 0) {
        glDeleteTextures(1, &atlas_);
        atlas_ = 0;
    }
    if (textVbo_ != 0) {
        glDeleteBuffers(1, &textVbo_);
        textVbo_ = 0;
    }
    if (textVao_ != 0) {
        glDeleteVertexArrays(1, &textVao_);
        textVao_ = 0;
    }
    if (boxVbo_ != 0) {
        glDeleteBuffers(1, &boxVbo_);
        boxVbo_ = 0;
    }
    if (boxVao_ != 0) {
        glDeleteVertexArrays(1, &boxVao_);
        boxVao_ = 0;
    }
    if (textProgram_ != 0) {
        glDeleteProgram(textProgram_);
        textProgram_ = 0;
    }
    if (imageProgram_ != 0) {
        glDeleteProgram(imageProgram_);
        imageProgram_ = 0;
    }
    if (boxProgram_ != 0) {
        glDeleteProgram(boxProgram_);
        boxProgram_ = 0;
        boxUniforms_.reset();
    }
    for (GpuImage& image : gpuImages_) {
        if (image.texture != 0) {
            glDeleteTextures(1, &image.texture);
        }
    }
    gpuImages_.clear();
}

void UiRenderer::begin(int framebufferWidth, int framebufferHeight, float pixelsPerPoint, const Color& clear,
                       bool clearColor) {
    viewportW_ = framebufferWidth;
    viewportH_ = framebufferHeight;
    scale_ = pixelsPerPoint > 0.f ? pixelsPerPoint : 1.f;
    atlasPacker_.beginFrame();
    atlasGaveUp_ = false;
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_SCISSOR_TEST);
    clips_.clear();
    if (clearColor) {
        glClearColor(clear.r, clear.g, clear.b, clear.a);
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

void UiRenderer::end() {
    clips_.clear();
    glDisable(GL_SCISSOR_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(0);
}

void UiRenderer::pushClip(float x, float y, float width, float height) {
    const float right = x + std::max(0.f, width);
    const float bottom = y + std::max(0.f, height);
    int sx = static_cast<int>(std::floor(x * scale_));
    int syTop = static_cast<int>(std::floor(y * scale_));
    int sx2 = static_cast<int>(std::ceil(right * scale_));
    int sy2 = static_cast<int>(std::ceil(bottom * scale_));
    Clip next;
    next.x = sx;
    next.width = std::max(0, sx2 - sx);
    next.height = std::max(0, sy2 - syTop);
    // GL scissor origin is the bottom left of the framebuffer.
    next.y = viewportH_ - (syTop + next.height);
    if (!clips_.empty()) {
        const Clip& outer = clips_.back();
        const int x1 = std::max(next.x, outer.x);
        const int y1 = std::max(next.y, outer.y);
        const int x2 = std::min(next.x + next.width, outer.x + outer.width);
        const int y2 = std::min(next.y + next.height, outer.y + outer.height);
        next.x = x1;
        next.y = y1;
        next.width = std::max(0, x2 - x1);
        next.height = std::max(0, y2 - y1);
    }
    clips_.push_back(next);
    glEnable(GL_SCISSOR_TEST);
    glScissor(next.x, next.y, next.width, next.height);
}

void UiRenderer::popClip() {
    if (clips_.empty()) {
        return;
    }
    clips_.pop_back();
    if (clips_.empty()) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    const Clip& clip = clips_.back();
    glScissor(clip.x, clip.y, clip.width, clip.height);
}

void UiRenderer::drawBox(float x, float y, float width, float height, float boxX, float boxY, float boxW, float boxH,
                         const float radius[4], const Color* stops, const float* stopAt, int stopCount, float mode,
                         const float sides[4], float blur, float angleDeg, const float* clip, const float* clipRadii,
                         bool exact) {
    if (!ready_ || width <= 0.f || height <= 0.f || viewportW_ <= 0 || viewportH_ <= 0 || stops == nullptr ||
        stopCount <= 0) {
        return;
    }
    const float s = scale_;
    const int count = std::min(stopCount, 8);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Always bound: other code, such as a 3D view drawn between UI draws, binds its own.
    glUseProgram(boxProgram_);
    // A uniform keeps its value in its program, so only what changed is sent.
    UniformCache& u = boxUniforms_;
    if (u.changed(kBoxRect, x * s, y * s, width * s, height * s)) {
        glUniform4f(boxRect_, x * s, y * s, width * s, height * s);
    }
    const float vw = static_cast<float>(viewportW_);
    const float vh = static_cast<float>(viewportH_);
    if (u.changed(kBoxViewport, vw, vh)) {
        glUniform2f(boxViewport_, vw, vh);
    }
    if (u.changed(kBoxBox, boxX * s, boxY * s, boxW * s, boxH * s)) {
        glUniform4f(boxBox_, boxX * s, boxY * s, boxW * s, boxH * s);
    }
    if (u.changed(kBoxRadii, radius[0] * s, radius[1] * s, radius[2] * s, radius[3] * s)) {
        glUniform4f(boxRadii_, radius[0] * s, radius[1] * s, radius[2] * s, radius[3] * s);
    }
    const float edge = exact ? 1.f : 0.f;
    const float soft = std::max(blur * s, 0.f);
    if (u.changed(kBoxParams, mode, edge, soft, angleDeg)) {
        glUniform4f(boxParams_, mode, edge, soft, angleDeg);
    }
    const float top = sides != nullptr ? sides[0] * s : 0.f;
    const float right = sides != nullptr ? sides[1] * s : 0.f;
    const float bottom = sides != nullptr ? sides[2] * s : 0.f;
    const float left = sides != nullptr ? sides[3] * s : 0.f;
    if (u.changed(kBoxBorder, top, right, bottom, left)) {
        glUniform4f(boxBorder_, top, right, bottom, left);
    }
    const float c0 = clip != nullptr ? clip[0] * s : 0.f;
    const float c1 = clip != nullptr ? clip[1] * s : 0.f;
    const float c2 = clip != nullptr ? clip[2] * s : 0.f;
    const float c3 = clip != nullptr ? clip[3] * s : 0.f;
    if (u.changed(kBoxClip, c0, c1, c2, c3)) {
        glUniform4f(boxClip_, c0, c1, c2, c3);
    }
    const float r0 = clipRadii != nullptr ? clipRadii[0] * s : 0.f;
    const float r1 = clipRadii != nullptr ? clipRadii[1] * s : 0.f;
    const float r2 = clipRadii != nullptr ? clipRadii[2] * s : 0.f;
    const float r3 = clipRadii != nullptr ? clipRadii[3] * s : 0.f;
    if (u.changed(kBoxClipRadii, r0, r1, r2, r3)) {
        glUniform4f(boxClipRadii_, r0, r1, r2, r3);
    }
    if (u.changed(kBoxStopCount, static_cast<float>(count))) {
        glUniform1f(boxStopCount_, static_cast<float>(count));
    }
    // box.frag reads only the first uStopCount stops, so the rest are left as they are.
    for (int i = 0; i < count; ++i) {
        const Color& color = stops[i];
        const float at = stopAt != nullptr ? stopAt[i] : 1.f;
        if (u.changed(kBoxStop0 + i, color.r, color.g, color.b, color.a)) {
            glUniform4f(boxStops_[i], color.r, color.g, color.b, color.a);
        }
        if (u.changed(kBoxStopAt0 + i, at)) {
            glUniform1f(boxStopAt_[i], at);
        }
    }
    glBindVertexArray(boxVao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void UiRenderer::fillRounded(float x, float y, float width, float height, const float radius[4], const Color* stops,
                             const float* stopAt, int stopCount, float angleDeg) {
    if (width <= 0.f || height <= 0.f || stops == nullptr || stopCount <= 0) {
        return;
    }
    bool visible = false;
    for (int i = 0; i < stopCount; ++i) {
        if (stops[i].a > 0.f) {
            visible = true;
        }
    }
    if (!visible) {
        return;
    }
    drawBox(x, y, width, height, 0.f, 0.f, width, height, radius, stops, stopAt, stopCount, 0.f, nullptr, 0.f,
            angleDeg);
}

void UiRenderer::fillRect(float x, float y, float width, float height, const Color& color) {
    if (width <= 0.f || height <= 0.f || color.a <= 0.f) {
        return;
    }
    // A pixel on each side, so a pixel an edge crosses is drawn whichever
    // box it belongs to, and the shader covers it by the part inside.
    const float pad = scale_ > 0.f ? 1.f / scale_ : 1.f;
    const float radius[4] = {};
    const float at = 0.f;
    drawBox(x - pad, y - pad, width + 2.f * pad, height + 2.f * pad, pad, pad, width, height, radius, &color, &at, 1,
            0.f, nullptr, 0.f, 0.f, nullptr, nullptr, true);
}

void UiRenderer::strokeRounded(float x, float y, float width, float height, const float radius[4], const float sides[4],
                               const Color& color) {
    if (sides == nullptr || color.a <= 0.f) {
        return;
    }
    if (sides[0] <= 0.f && sides[1] <= 0.f && sides[2] <= 0.f && sides[3] <= 0.f) {
        return;
    }
    const float at = 0.f;
    drawBox(x, y, width, height, 0.f, 0.f, width, height, radius, &color, &at, 1, 1.f, sides, 0.f, 0.f);
}

void UiRenderer::outerShadow(float x, float y, float width, float height, const float radius[4], float offsetX,
                             float offsetY, float blur, float spread, const Color& color) {
    if (color.a <= 0.f) {
        return;
    }
    const float blurRadius = std::max(blur, 0.f);
    const float boxX = x + offsetX - spread;
    const float boxY = y + offsetY - spread;
    const float boxW = width + spread * 2.f;
    const float boxH = height + spread * 2.f;
    if (boxW <= 0.5f || boxH <= 0.5f) {
        return;
    }
    // The kernel reaches 3 sigma, which is 1.5 blur radii, past the edge.
    const float pad = std::max(1.5f * blurRadius, 1.f);
    // Java raises each corner of the blurred shape to the standard deviation.
    const float cornerFloor = std::max(blurRadius, 0.5f) * 0.5f;
    float radii[4];
    for (int i = 0; i < 4; ++i) {
        radii[i] = std::max(radius[i] + spread, cornerFloor);
    }
    const float clip[4] = {pad - offsetX + spread, pad - offsetY + spread, width, height};
    const float at = 0.f;
    drawBox(boxX - pad, boxY - pad, boxW + pad * 2.f, boxH + pad * 2.f, pad, pad, boxW, boxH, radii, &color, &at, 1,
            2.f, nullptr, blurRadius, 0.f, clip, radius);
}

void UiRenderer::innerShadow(float x, float y, float width, float height, const float radius[4], float offsetX,
                             float offsetY, float blur, float spread, const Color& color) {
    if (color.a <= 0.f || width <= 0.f || height <= 0.f) {
        return;
    }
    const float blurRadius = std::max(blur, 0.f);
    const float cornerFloor = std::max(blurRadius, 0.5f) * 0.5f;
    float radii[4];
    for (int i = 0; i < 4; ++i) {
        radii[i] = std::max(radius[i] - spread, cornerFloor);
    }
    const float clip[4] = {0.f, 0.f, width, height};
    const float at = 0.f;
    drawBox(x, y, width, height, spread + offsetX, spread + offsetY, width - spread * 2.f, height - spread * 2.f,
            radii, &color, &at, 1, 3.f, nullptr, blurRadius, 0.f, clip, radius);
}

const UiRenderer::Glyph* UiRenderer::glyphFor(int codepoint, int pixelSize, int phase, const FontFace* face,
                                               bool wantSubpixel) {
    if (face == nullptr || !face->ready || pixelSize <= 0) {
        return nullptr;
    }
    const bool lcd = subpixel_ && wantSubpixel;
    if (!subpixel_) {
        // The grayscale-only atlas filters linearly, so its quads carry the fraction instead.
        phase = 0;
    }
    const GlyphKey key{face, pixelSize, codepoint, phase, lcd};
    const auto found = glyphs_.find(key);
    if (found != glyphs_.end()) {
        return &found->second;
    }
    if (atlasGaveUp_) {
        return nullptr;
    }

    // Grayscale glyphs share the stripe atlas and blend when there is one. Equal channels are plain coverage.
    const SubpixelBitmap raster = RasterizeGlyph(*face, codepoint, pixelSize, phase, lcd);
    const int width = raster.width;
    const int height = raster.height;
    Glyph glyph;
    glyph.xoff = static_cast<float>(raster.xoff);
    glyph.yoff = static_cast<float>(raster.yoff);
    glyph.width = static_cast<float>(width);
    glyph.height = static_cast<float>(height);
    glyph.empty = width <= 0 || height <= 0 || raster.rgb.empty();
    std::vector<unsigned char> upload;
    if (!glyph.empty) {
        const int texelBytes = subpixel_ ? 4 : 1;
        upload.resize(static_cast<std::size_t>(width * height * texelBytes));
        for (int i = 0; i < width * height; ++i) {
            const unsigned char* rgb = raster.rgb.data() + static_cast<std::size_t>(i * 3);
            unsigned char* texel = upload.data() + static_cast<std::size_t>(i * texelBytes);
            if (subpixel_) {
                texel[0] = rgb[0];
                texel[1] = rgb[1];
                texel[2] = rgb[2];
                texel[3] = 255;
            } else {
                texel[0] = rgb[0];
            }
        }
    }
    if (!glyph.empty) {
        // Each zoom caches a new size of every glyph, so the atlas fills.
        // Emptied, it refills with what is on screen now; grown when this
        // frame's own glyphs do not fit. text() redoes a string it was
        // placing when that happens. Draws already made keep the old texels:
        // GL runs them before the upload that replaces them.
        std::optional<GlyphAtlasPacker::Spot> spot = atlasPacker_.place(width, height);
        while (!spot) {
            const GlyphAtlasPacker::Overflow next = atlasPacker_.overflow();
            if (next == GlyphAtlasPacker::Overflow::GiveUp) {
                atlasGaveUp_ = true;
                if (!atlasWarned_) {
                    atlasWarned_ = true;
                    std::fprintf(stderr, "This frame's glyphs do not fit the largest font atlas. Some are skipped.\n");
                }
                return nullptr;
            }
            resetAtlas(next == GlyphAtlasPacker::Overflow::Grow ? atlasPacker_.size() * 2 : atlasPacker_.size());
            spot = atlasPacker_.place(width, height);
        }
        glBindTexture(GL_TEXTURE_2D, atlas_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (subpixel_) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, spot->x, spot->y, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                            upload.data());
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, spot->x, spot->y, width, height, GL_RED, GL_UNSIGNED_BYTE,
                            upload.data());
        }
        const float size = static_cast<float>(atlasPacker_.size());
        glyph.u0 = static_cast<float>(spot->x) / size;
        glyph.v0 = static_cast<float>(spot->y) / size;
        glyph.u1 = static_cast<float>(spot->x + width) / size;
        glyph.v1 = static_cast<float>(spot->y + height) / size;
    }
    const auto inserted = glyphs_.emplace(key, glyph);
    return &inserted.first->second;
}

void UiRenderer::text(float x, float y, const std::string& utf8, const std::string& family, float fontSize,
                      const Color& color, bool subpixel) {
    if (!ready_ || utf8.empty() || color.a <= 0.f || fontSize <= 0.f) {
        return;
    }
    const FontFace* face = LookupFace(family);
    if (face == nullptr) {
        return;
    }
    const Font font(family, fontSize);
    const ShapedText shaped = font.shape(utf8);
    const int pixelSize = std::max(1, static_cast<int>(std::lround(fontSize * scale_)));
    const bool lcd = subpixel_ && subpixel;
    std::vector<float> vertices;
    vertices.reserve(shaped.glyphs.size() * 24);
    // A glyph that empties or grows the atlas leaves the quads before it
    // pointing at texels that are gone, so the string is placed again. Each
    // frame allows one empty and a few doublings, so this ends.
    for (int pass = 0; pass < 8; ++pass) {
        const unsigned generation = atlasGeneration_;
        vertices.clear();
        for (const ShapedGlyph& placed : shaped.glyphs) {
            float left = 0.f;
            float top = 0.f;
            const Glyph* glyph = nullptr;
            if (subpixel_) {
                // The shared atlas is sampled at texel centers, so the pen's fraction of a pixel is
                // baked into the bitmap and the quad itself sits on a pixel. Stripe and grayscale alike.
                int penPixel = 0;
                const int phase = SubpixelPhase((x + placed.x) * scale_, penPixel);
                glyph = glyphFor(static_cast<int>(placed.codepoint), pixelSize, phase, face, lcd);
                if (atlasGeneration_ != generation) {
                    break;
                }
                if (glyph == nullptr || glyph->empty) {
                    continue;
                }
                const float baseline = std::round((y + placed.lineTop + font.ascent()) * scale_);
                left = static_cast<float>(penPixel) + glyph->xoff;
                top = baseline + glyph->yoff;
            } else {
                glyph = glyphFor(static_cast<int>(placed.codepoint), pixelSize, 0, face, false);
                if (atlasGeneration_ != generation) {
                    break;
                }
                if (glyph == nullptr || glyph->empty) {
                    continue;
                }
                left = (x + placed.x) * scale_ + glyph->xoff;
                top = (y + placed.lineTop) * scale_ + font.ascent() * scale_ + glyph->yoff;
            }
            const float right = left + glyph->width;
            const float bottom = top + glyph->height;
            const float quad[] = {
                left, top,    glyph->u0, glyph->v0, right, top,    glyph->u1, glyph->v0, right, bottom, glyph->u1, glyph->v1,
                left, top,    glyph->u0, glyph->v0, right, bottom, glyph->u1, glyph->v1, left,  bottom, glyph->u0, glyph->v1,
            };
            vertices.insert(vertices.end(), quad, quad + 24);
        }
        if (atlasGeneration_ == generation) {
            break;
        }
    }
    if (vertices.empty()) {
        return;
    }

    glUseProgram(textProgram_);
    if (subpixel_) {
        glBlendFunc(GL_SRC1_COLOR, GL_ONE_MINUS_SRC1_COLOR);
    } else {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    glUniform2f(textViewport_, static_cast<float>(viewportW_), static_cast<float>(viewportH_));
    glUniform4f(textColor_, color.r, color.g, color.b, color.a);
    const TextGamma& gamma = lcd ? kStripeGamma : kGrayGamma;
    glUniform3f(textGamma_, gamma.gamma, gamma.contrast, lcd ? 0.f : 1.f);
    glUniform1i(textSampler_, 0);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(),
                 GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 4));
}

unsigned UiRenderer::imageTexture(const std::shared_ptr<ImageData>& image) {
    if (!image || image->width <= 0 || image->height <= 0 || image->rgba.empty()) {
        return 0;
    }
    for (std::size_t i = 0; i < gpuImages_.size();) {
        const std::shared_ptr<ImageData> live = gpuImages_[i].data.lock();
        if (!live) {
            if (gpuImages_[i].texture != 0) {
                glDeleteTextures(1, &gpuImages_[i].texture);
            }
            gpuImages_[i] = std::move(gpuImages_.back());
            gpuImages_.pop_back();
            continue;
        }
        if (live == image) {
            return gpuImages_[i].texture;
        }
        ++i;
    }

    const std::size_t height = static_cast<std::size_t>(image->height);
    const std::size_t width = static_cast<std::size_t>(image->width);
    if (height == 0 || width > image->rgba.size() / 4 / height) {
        return 0;
    }

    unsigned texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) {
        return 0;
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image->width, image->height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 image->rgba.data());
    gpuImages_.push_back(GpuImage{image, texture});
    return texture;
}

void UiRenderer::drawImage(const std::shared_ptr<ImageData>& image, float x, float y, float width, float height,
                           float opacity, const Color* tint) {
    if (!ready_ || imageProgram_ == 0 || opacity <= 0.f || width <= 0.f || height <= 0.f || viewportW_ <= 0 ||
        viewportH_ <= 0) {
        return;
    }
    const unsigned texture = imageTexture(image);
    if (texture == 0) {
        return;
    }
    const float scale = scale_;
    const float left = x * scale;
    const float top = y * scale;
    const float right = (x + width) * scale;
    const float bottom = (y + height) * scale;
    // v 0 is the top row, the order stb_image decodes.
    const float quad[] = {
        left, top, 0.f, 0.f, right, top,    1.f, 0.f, right, bottom, 1.f, 1.f,
        left, top, 0.f, 0.f, right, bottom, 1.f, 1.f, left,  bottom, 0.f, 1.f,
    };

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(imageProgram_);
    glUniform2f(imageViewport_, static_cast<float>(viewportW_), static_cast<float>(viewportH_));
    glUniform1f(imageOpacity_, opacity);
    glUniform1i(imageSampler_, 0);
    if (tint != nullptr) {
        glUniform4f(imageTint_, tint->r, tint->g, tint->b, tint->a);
    }
    glUniform1f(imageTinted_, tint != nullptr ? 1.f : 0.f);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

bool UiRenderer::writePpm(const char* path) const {
    if (path == nullptr || viewportW_ <= 0 || viewportH_ <= 0) {
        return false;
    }
    std::vector<unsigned char> pixels(static_cast<std::size_t>(viewportW_ * viewportH_ * 4));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, viewportW_, viewportH_, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        std::fprintf(stderr, "Could not write %s\n", path);
        return false;
    }
    std::fprintf(file, "P6\n%d %d\n255\n", viewportW_, viewportH_);
    for (int row = viewportH_ - 1; row >= 0; --row) {
        for (int col = 0; col < viewportW_; ++col) {
            const unsigned char* pixel = pixels.data() + static_cast<std::size_t>((row * viewportW_ + col) * 4);
            std::fwrite(pixel, 1, 3, file);
        }
    }
    std::fclose(file);
    return true;
}

}  // namespace jadefx

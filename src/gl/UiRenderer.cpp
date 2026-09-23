#include "UiRenderer.hpp"

#include "Resources.hpp"
#include "gl.hpp"
#include "internal/Subpixel.hpp"
#include "platform/ErrorDialog.hpp"
#include "scene/FontInternal.hpp"
#include "jadefx/scene/text/Font.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace jadefx {
namespace {

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
    if (!missing.empty()) {
        ReportMissingShaders(missing);
        return false;
    }

    boxProgram_ = LinkShaderProgram(boxVertex, boxFragment, "Box");
    textProgram_ = LinkShaderProgram(textVertex, textFragment, "Text");
    subpixel_ = textProgram_ != 0;
    if (!subpixel_) {
        std::fprintf(stderr, "LCD subpixel text is unavailable. Using grayscale coverage.\n");
        textProgram_ = LinkShaderProgram(textVertex, textGray, "Text");
    }
    if (boxProgram_ == 0 || textProgram_ == 0) {
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
    textSampler_ = Location(textProgram_, "uTex");

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

    const int texelBytes = subpixel_ ? 4 : 1;
    std::vector<unsigned char> blank(static_cast<std::size_t>(kAtlas * kAtlas * texelBytes), 0);
    glGenTextures(1, &atlas_);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (subpixel_) {
        // RGBA keeps each row 4-byte aligned. RGB coverage lives in rgb; stripes must not be filtered together.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kAtlas, kAtlas, 0, GL_RGBA, GL_UNSIGNED_BYTE, blank.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kAtlas, kAtlas, 0, GL_RED, GL_UNSIGNED_BYTE, blank.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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

void UiRenderer::shutdown() {
    ready_ = false;
    glyphs_.clear();
    atlasPenX_ = 1;
    atlasPenY_ = 1;
    atlasRowHeight_ = 0;
    atlasFull_ = false;
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
    if (boxProgram_ != 0) {
        glDeleteProgram(boxProgram_);
        boxProgram_ = 0;
    }
}

void UiRenderer::begin(int framebufferWidth, int framebufferHeight, float pixelsPerPoint, const Color& clear) {
    viewportW_ = framebufferWidth;
    viewportH_ = framebufferHeight;
    scale_ = pixelsPerPoint > 0.f ? pixelsPerPoint : 1.f;
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_SCISSOR_TEST);
    clips_.clear();
    glClearColor(clear.r, clear.g, clear.b, clear.a);
    glClear(GL_COLOR_BUFFER_BIT);
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
                         const float sides[4], float blur, float angleDeg, const float* clip, const float* clipRadii) {
    if (!ready_ || width <= 0.f || height <= 0.f || viewportW_ <= 0 || viewportH_ <= 0 || stops == nullptr ||
        stopCount <= 0) {
        return;
    }
    const float s = scale_;
    const int count = std::min(stopCount, 8);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(boxProgram_);
    glUniform4f(boxRect_, x * s, y * s, width * s, height * s);
    glUniform2f(boxViewport_, static_cast<float>(viewportW_), static_cast<float>(viewportH_));
    glUniform4f(boxBox_, boxX * s, boxY * s, boxW * s, boxH * s);
    glUniform4f(boxRadii_, radius[0] * s, radius[1] * s, radius[2] * s, radius[3] * s);
    glUniform4f(boxParams_, mode, 0.f, std::max(blur * s, 0.f), angleDeg);
    const float top = sides != nullptr ? sides[0] : 0.f;
    const float right = sides != nullptr ? sides[1] : 0.f;
    const float bottom = sides != nullptr ? sides[2] : 0.f;
    const float left = sides != nullptr ? sides[3] : 0.f;
    glUniform4f(boxBorder_, top * s, right * s, bottom * s, left * s);
    if (clip != nullptr) {
        glUniform4f(boxClip_, clip[0] * s, clip[1] * s, clip[2] * s, clip[3] * s);
    } else {
        glUniform4f(boxClip_, 0.f, 0.f, 0.f, 0.f);
    }
    if (clipRadii != nullptr) {
        glUniform4f(boxClipRadii_, clipRadii[0] * s, clipRadii[1] * s, clipRadii[2] * s, clipRadii[3] * s);
    } else {
        glUniform4f(boxClipRadii_, 0.f, 0.f, 0.f, 0.f);
    }
    glUniform1f(boxStopCount_, static_cast<float>(count));
    for (int i = 0; i < 8; ++i) {
        const Color color = i < count ? stops[i] : stops[count - 1];
        const float at = i < count && stopAt != nullptr ? stopAt[i] : 1.f;
        glUniform4f(boxStops_[i], color.r, color.g, color.b, color.a);
        glUniform1f(boxStopAt_[i], at);
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
    if (!lcd) {
        phase = 0;
    }
    const GlyphKey key{pixelSize, codepoint, phase, lcd};
    const auto found = glyphs_.find(key);
    if (found != glyphs_.end()) {
        return &found->second;
    }
    if (atlasFull_) {
        return nullptr;
    }

    const float raster = stbtt_ScaleForMappingEmToPixels(&face->info, static_cast<float>(pixelSize));
    Glyph glyph;
    std::vector<unsigned char> upload;
    int width = 0;
    int height = 0;
    if (lcd) {
        // Rasterize one sample per stripe. phase/3 is where this glyph sits inside its pixel.
        const float shift = static_cast<float>(phase) / 3.f;
        int ix0 = 0;
        int iy0 = 0;
        int ix1 = 0;
        int iy1 = 0;
        stbtt_GetCodepointBitmapBoxSubpixel(&face->info, codepoint, raster * 3.f, raster, shift * 3.f, 0.f, &ix0, &iy0,
                                            &ix1, &iy1);
        const int sampleWidth = std::max(0, ix1 - ix0);
        const int sampleHeight = std::max(0, iy1 - iy0);
        std::vector<unsigned char> samples;
        if (sampleWidth > 0 && sampleHeight > 0) {
            samples.assign(static_cast<std::size_t>(sampleWidth * sampleHeight), 0);
            stbtt_MakeCodepointBitmapSubpixel(&face->info, samples.data(), sampleWidth, sampleHeight, sampleWidth,
                                              raster * 3.f, raster, shift * 3.f, 0.f, codepoint);
        }
        const SubpixelBitmap packed = PackSubpixelCoverage(samples.empty() ? nullptr : samples.data(), sampleWidth,
                                                           sampleHeight, sampleWidth, ix0, iy0);
        glyph.xoff = static_cast<float>(packed.xoff);
        glyph.yoff = static_cast<float>(packed.yoff);
        width = packed.width;
        height = packed.height;
        glyph.width = static_cast<float>(width);
        glyph.height = static_cast<float>(height);
        glyph.empty = width <= 0 || height <= 0 || packed.rgb.empty();
        if (!glyph.empty) {
            upload.resize(static_cast<std::size_t>(width * height * 4));
            for (int i = 0; i < width * height; ++i) {
                upload[static_cast<std::size_t>(i * 4)] = packed.rgb[static_cast<std::size_t>(i * 3)];
                upload[static_cast<std::size_t>(i * 4 + 1)] = packed.rgb[static_cast<std::size_t>(i * 3 + 1)];
                upload[static_cast<std::size_t>(i * 4 + 2)] = packed.rgb[static_cast<std::size_t>(i * 3 + 2)];
                upload[static_cast<std::size_t>(i * 4 + 3)] = 255;
            }
        }
    } else {
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        stbtt_GetCodepointBitmapBox(&face->info, codepoint, raster, raster, &x0, &y0, &x1, &y1);
        width = std::max(0, x1 - x0);
        height = std::max(0, y1 - y0);
        glyph.xoff = static_cast<float>(x0);
        glyph.yoff = static_cast<float>(y0);
        glyph.width = static_cast<float>(width);
        glyph.height = static_cast<float>(height);
        glyph.empty = width <= 0 || height <= 0;
        if (!glyph.empty) {
            std::vector<unsigned char> coverage(static_cast<std::size_t>(width * height));
            stbtt_MakeCodepointBitmap(&face->info, coverage.data(), width, height, width, raster, raster, codepoint);
            if (subpixel_) {
                // Same atlas and blend as stripe glyphs. Equal channels are ordinary grayscale coverage.
                upload.resize(static_cast<std::size_t>(width * height * 4));
                for (int i = 0; i < width * height; ++i) {
                    const unsigned char value = coverage[static_cast<std::size_t>(i)];
                    upload[static_cast<std::size_t>(i * 4)] = value;
                    upload[static_cast<std::size_t>(i * 4 + 1)] = value;
                    upload[static_cast<std::size_t>(i * 4 + 2)] = value;
                    upload[static_cast<std::size_t>(i * 4 + 3)] = 255;
                }
            } else {
                upload = std::move(coverage);
            }
        }
    }
    if (!glyph.empty) {
        if (atlasPenX_ + width + 1 >= kAtlas) {
            atlasPenX_ = 1;
            atlasPenY_ += atlasRowHeight_ + 1;
            atlasRowHeight_ = 0;
        }
        if (atlasPenY_ + height + 1 >= kAtlas) {
            atlasFull_ = true;
            std::fprintf(stderr, "The font atlas is full. Further glyphs are skipped.\n");
            return nullptr;
        }
        glBindTexture(GL_TEXTURE_2D, atlas_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (subpixel_) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, atlasPenX_, atlasPenY_, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                            upload.data());
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, atlasPenX_, atlasPenY_, width, height, GL_RED, GL_UNSIGNED_BYTE,
                            upload.data());
        }
        glyph.u0 = static_cast<float>(atlasPenX_) / static_cast<float>(kAtlas);
        glyph.v0 = static_cast<float>(atlasPenY_) / static_cast<float>(kAtlas);
        glyph.u1 = static_cast<float>(atlasPenX_ + width) / static_cast<float>(kAtlas);
        glyph.v1 = static_cast<float>(atlasPenY_ + height) / static_cast<float>(kAtlas);
        atlasPenX_ += width + 1;
        atlasRowHeight_ = std::max(atlasRowHeight_, height);
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
    for (const ShapedGlyph& placed : shaped.glyphs) {
        float left = 0.f;
        float top = 0.f;
        const Glyph* glyph = nullptr;
        if (lcd) {
            int penPixel = 0;
            const int phase = SubpixelPhase((x + placed.x) * scale_, penPixel);
            glyph = glyphFor(static_cast<int>(placed.codepoint), pixelSize, phase, face, true);
            if (glyph == nullptr || glyph->empty) {
                continue;
            }
            // The stripe phase is baked into the bitmap, so the quad itself sits on a pixel.
            const float baseline = std::round((y + placed.lineTop + font.ascent()) * scale_);
            left = static_cast<float>(penPixel) + glyph->xoff;
            top = baseline + glyph->yoff;
        } else {
            glyph = glyphFor(static_cast<int>(placed.codepoint), pixelSize, 0, face, false);
            if (glyph == nullptr || glyph->empty) {
                continue;
            }
            left = (x + placed.x) * scale_ + glyph->xoff;
            top = (y + placed.lineTop) * scale_ + font.ascent() * scale_ + glyph->yoff;
            if (subpixel_) {
                // The shared atlas is sampled at texel centers, so a grayscale quad has to sit on a pixel.
                left = std::round(left);
                top = std::round(top);
            }
        }
        const float right = left + glyph->width;
        const float bottom = top + glyph->height;
        const float quad[] = {
            left, top,    glyph->u0, glyph->v0, right, top,    glyph->u1, glyph->v0, right, bottom, glyph->u1, glyph->v1,
            left, top,    glyph->u0, glyph->v0, right, bottom, glyph->u1, glyph->v1, left,  bottom, glyph->u0, glyph->v1,
        };
        vertices.insert(vertices.end(), quad, quad + 24);
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
    glUniform1i(textSampler_, 0);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(),
                 GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 4));
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

#include "jadefx/scene/text/Font.hpp"

#include "FontInternal.hpp"
#include "internal/Text.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "OpenSansRegular.inl"

namespace jadefx {
namespace {

std::vector<std::unique_ptr<FontFace>>& Faces() {
    // unique_ptr keeps each face put, so stbtt_fontinfo's pointer into its bytes stays valid.
    static std::vector<std::unique_ptr<FontFace>> faces;
    return faces;
}

bool SameFamily(const std::string& a, const std::string& b) {
    return lowerCopy(a) == lowerCopy(b);
}

bool NextCodepoint(const std::string& text, std::size_t& index, char32_t& codepoint) {
    if (index >= text.size()) {
        return false;
    }
    const unsigned char lead = static_cast<unsigned char>(text[index++]);
    if (lead < 0x80) {
        codepoint = lead;
        return true;
    }
    int need = 0;
    char32_t value = 0;
    if ((lead & 0xE0) == 0xC0) {
        need = 1;
        value = lead & 0x1F;
    } else if ((lead & 0xF0) == 0xE0) {
        need = 2;
        value = lead & 0x0F;
    } else if ((lead & 0xF8) == 0xF0) {
        need = 3;
        value = lead & 0x07;
    } else {
        codepoint = 0xFFFD;
        return true;
    }
    for (int i = 0; i < need; ++i) {
        if (index >= text.size()) {
            codepoint = 0xFFFD;
            return true;
        }
        const unsigned char next = static_cast<unsigned char>(text[index++]);
        if ((next & 0xC0) != 0x80) {
            codepoint = 0xFFFD;
            return true;
        }
        value = (value << 6) | (next & 0x3F);
    }
    codepoint = value;
    return true;
}

const FontFace* ReadyFace(const std::string& family) {
    Font::loadDefault();
    const FontFace* fallback = Faces().empty() ? nullptr : Faces().front().get();
    for (const std::unique_ptr<FontFace>& face : Faces()) {
        if (face && face->ready && SameFamily(face->family, family)) {
            return face.get();
        }
    }
    return fallback;
}

float EmScale(const FontFace& face, float size) {
    return stbtt_ScaleForMappingEmToPixels(&face.info, size > 0.f ? size : 1.f);
}

}  // namespace

const FontFace* LookupFace(const std::string& family) {
    return ReadyFace(family.empty() ? "Open Sans" : family);
}

Font::Font() = default;

Font::Font(std::string family, float size) : family_(std::move(family)), size_(size) {}

void Font::loadDefault() {
    static bool loaded = false;
    if (loaded) {
        return;
    }
    loaded = true;
    loadBytes("Open Sans", kOpenSansRegular, static_cast<int>(kOpenSansRegularLength));
    loadBytes("Google Sans", kOpenSansRegular, static_cast<int>(kOpenSansRegularLength));
}

bool Font::loadBytes(const std::string& family, const unsigned char* data, int size) {
    if (data == nullptr || size <= 0) {
        return false;
    }
    auto face = std::make_unique<FontFace>();
    face->family = family;
    face->bytes.assign(data, data + size);
    const int offset = stbtt_GetFontOffsetForIndex(face->bytes.data(), 0);
    face->ready = stbtt_InitFont(&face->info, face->bytes.data(), offset < 0 ? 0 : offset) != 0;
    if (!face->ready) {
        std::fprintf(stderr, "Could not read font \"%s\".\n", family.c_str());
        return false;
    }
    for (std::unique_ptr<FontFace>& existing : Faces()) {
        if (existing && SameFamily(existing->family, family)) {
            existing = std::move(face);
            return true;
        }
    }
    Faces().push_back(std::move(face));
    return true;
}

bool Font::loadFile(const std::string& family, const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff length = file.tellg();
    if (length <= 0) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    file.read(reinterpret_cast<char*>(bytes.data()), length);
    if (!file) {
        return false;
    }
    return loadBytes(family, bytes.data(), static_cast<int>(bytes.size()));
}

float Font::ascent() const {
    const FontFace* face = ReadyFace(family_);
    if (face == nullptr || !face->ready) {
        return size_;
    }
    int ascent = 0;
    int descent = 0;
    int gap = 0;
    stbtt_GetFontVMetrics(&face->info, &ascent, &descent, &gap);
    return static_cast<float>(ascent) * EmScale(*face, size_);
}

float Font::lineHeight() const {
    const FontFace* face = ReadyFace(family_);
    if (face == nullptr || !face->ready) {
        return size_;
    }
    int ascent = 0;
    int descent = 0;
    int gap = 0;
    stbtt_GetFontVMetrics(&face->info, &ascent, &descent, &gap);
    const float height = static_cast<float>(ascent - descent + gap) * EmScale(*face, size_);
    return height > 0.f ? height : size_;
}

ShapedText Font::shape(const std::string& text) const {
    ShapedText shaped;
    const FontFace* face = ReadyFace(family_);
    const float line = lineHeight();
    if (text.empty()) {
        return shaped;
    }
    if (face == nullptr || !face->ready) {
        shaped.width = size_ * 0.5f * static_cast<float>(text.size());
        shaped.height = line > 0.f ? line : size_;
        shaped.lines = 1;
        return shaped;
    }
    const float scale = EmScale(*face, size_);
    float pen = 0.f;
    float lineTop = 0.f;
    float maxWidth = 0.f;
    int lines = 1;
    char32_t previous = 0;
    std::size_t index = 0;
    char32_t codepoint = 0;
    while (NextCodepoint(text, index, codepoint)) {
        if (codepoint == '\n') {
            maxWidth = std::max(maxWidth, pen);
            pen = 0.f;
            lineTop += line;
            ++lines;
            previous = 0;
            continue;
        }
        if (previous != 0) {
            pen += static_cast<float>(stbtt_GetCodepointKernAdvance(&face->info, static_cast<int>(previous),
                                                                    static_cast<int>(codepoint))) *
                   scale;
        }
        ShapedGlyph glyph;
        glyph.codepoint = codepoint;
        glyph.x = pen;
        glyph.lineTop = lineTop;
        shaped.glyphs.push_back(glyph);
        int advance = 0;
        int bearing = 0;
        stbtt_GetCodepointHMetrics(&face->info, static_cast<int>(codepoint), &advance, &bearing);
        pen += static_cast<float>(advance) * scale;
        previous = codepoint;
    }
    maxWidth = std::max(maxWidth, pen);
    shaped.width = maxWidth;
    shaped.height = line * static_cast<float>(lines);
    shaped.lines = lines;
    return shaped;
}

float Font::measureWidth(const std::string& text) const {
    return shape(text).width;
}

}  // namespace jadefx

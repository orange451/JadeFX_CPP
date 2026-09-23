#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace jadefx {

struct ShapedGlyph {
    char32_t codepoint = 0;
    float x = 0.f;
    float lineTop = 0.f;
};

struct ShapedText {
    std::vector<ShapedGlyph> glyphs;
    float width = 0.f;
    float height = 0.f;
    int lines = 0;
};

// A typeface size. Measuring does not need an OpenGL context.
// The default face is Open Sans, also registered as "Google Sans" because
// the JadeFX examples ask for that family.
class Font {
public:
    Font();
    Font(std::string family, float size);

    static void loadDefault();
    static bool loadFile(const std::string& family, const std::string& path);
    static bool loadBytes(const std::string& family, const unsigned char* data, int size);

    const std::string& family() const { return family_; }
    float size() const { return size_; }

    float ascent() const;
    float lineHeight() const;
    float measureWidth(const std::string& text) const;
    ShapedText shape(const std::string& text) const;

private:
    std::string family_ = "Open Sans";
    float size_ = 16.f;
};

}  // namespace jadefx

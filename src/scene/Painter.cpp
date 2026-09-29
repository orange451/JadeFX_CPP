#include "jadefx/scene/Painter.hpp"

#include "gl/UiRenderer.hpp"

namespace jadefx {

void Painter::fillRect(float x, float y, float width, float height, const Color& color) {
    renderer_.fillRect(x, y, width, height, color);
}

void Painter::text(float x, float y, const std::string& utf8, const std::string& family, float size,
                   const Color& color, bool subpixel) {
    renderer_.text(x, y, utf8, family, size, color, subpixel);
}

void Painter::pushClip(float x, float y, float width, float height) { renderer_.pushClip(x, y, width, height); }

void Painter::popClip() { renderer_.popClip(); }

float Painter::pixelsPerPoint() const { return renderer_.pixelsPerPoint(); }

void Painter::strokeRounded(float x, float y, float width, float height, const float radius[4], const float sides[4],
                            const Color& color) {
    renderer_.strokeRounded(x, y, width, height, radius, sides, color);
}

}  // namespace jadefx

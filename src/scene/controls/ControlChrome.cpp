#include "ControlChrome.hpp"

#include "gl/UiRenderer.hpp"

#include <algorithm>
#include <cmath>

namespace jadefx::chrome {
namespace {

struct Box {
    float x = 0.f;
    float y = 0.f;
    float width = 0.f;
    float height = 0.f;
    bool empty() const { return width <= 0.f || height <= 0.f; }
};

Box BoxOf(const Node& node) {
    return {static_cast<float>(node.getAbsoluteX()), static_cast<float>(node.getAbsoluteY()),
            static_cast<float>(node.getWidth()), static_cast<float>(node.getHeight())};
}

void Stroke(UiRenderer& renderer, const Box& box, float corner, float thickness, const Color& color) {
    const float radius[4] = {corner, corner, corner, corner};
    const float sides[4] = {thickness, thickness, thickness, thickness};
    renderer.strokeRounded(box.x, box.y, box.width, box.height, radius, sides, color);
}

}  // namespace

Font FontOf(const Node& node) {
    const ComputedStyle& style = node.computedStyle();
    const float size = style.fontSize > 0.f ? style.fontSize : 16.f;
    return Font(style.fontFamily.empty() ? std::string("Open Sans") : style.fontFamily, size);
}

Color Themed(const Node& node, ThemeColor color, float opacity) {
    Color out = node.themeColor(color);
    out.a *= opacity;
    return out;
}

void DrawBorder(UiRenderer& renderer, const Node& node, float opacity, float corner) {
    const Box box = BoxOf(node);
    const ComputedStyle& style = node.computedStyle();
    const bool cssBorder = style.borderStyle == BorderStyle::Solid &&
                           (style.border.top > 0 || style.border.right > 0 || style.border.bottom > 0 || style.border.left > 0);
    if (!box.empty() && !cssBorder) {
        Stroke(renderer, box, corner, 1.f, Themed(node, ThemeColor::Border, opacity));
    }
}

void DrawFocusRing(UiRenderer& renderer, const Node& node, float opacity, float corner) {
    const Box box = BoxOf(node);
    if (!box.empty()) {
        Stroke(renderer, box, corner, 2.f, Themed(node, ThemeColor::Outline, opacity));
    }
}

void DrawArrowHead(UiRenderer& renderer, float centerX, float centerY, Side points, const Color& color) {
    if (color.a <= 0.f) {
        return;
    }
    constexpr float kWide = 8.f;
    constexpr float kTall = 5.f;
    const float radius[4] = {};
    const float at = 0.f;
    // The tip sits one sixth off the box center, so the mark is shifted back onto the midline.
    const bool vertical = points == Side::Top || points == Side::Bottom;
    const bool towardStart = points == Side::Top || points == Side::Left;
    const float center = vertical ? centerY : centerX;
    const float first = center - kTall * 0.5f + (towardStart ? -kTall / 6.f : kTall / 6.f);
    for (float step = 0.f; step < kTall; step += 1.f) {
        const float t = (step + 0.5f) / kTall;
        const float across = kWide * (towardStart ? t : 1.f - t);
        if (across < 0.4f) {
            continue;
        }
        if (vertical) {
            renderer.fillRounded(centerX - across * 0.5f, first + step, across, 1.f, radius, &color, &at, 1, 0.f);
        } else {
            renderer.fillRounded(first + step, centerY - across * 0.5f, 1.f, across, radius, &color, &at, 1, 0.f);
        }
    }
}

void DrawCheckerboard(UiRenderer& renderer, float x, float y, float width, float height, float opacity, float cell) {
    if (width <= 0.f || height <= 0.f || cell <= 0.f) {
        return;
    }
    const float radius[4] = {};
    const float at = 0.f;
    const Color light = Color::rgba(1.f, 1.f, 1.f, opacity);
    const Color dark = Color::rgba(0.8f, 0.8f, 0.8f, opacity);
    renderer.fillRounded(x, y, width, height, radius, &light, &at, 1, 0.f);
    renderer.pushClip(x, y, width, height);
    const int columns = static_cast<int>(std::ceil(width / cell));
    const int rows = static_cast<int>(std::ceil(height / cell));
    for (int row = 0; row < rows; ++row) {
        for (int column = row % 2; column < columns; column += 2) {
            renderer.fillRounded(x + static_cast<float>(column) * cell, y + static_cast<float>(row) * cell, cell, cell,
                                 radius, &dark, &at, 1, 0.f);
        }
    }
    renderer.popClip();
}

void DrawColorSwatch(UiRenderer& renderer, const Node& node, float x, float y, float width, float height,
                     const Color& color, float opacity, float corner) {
    if (width <= 0.f || height <= 0.f) {
        return;
    }
    const float radius[4] = {corner, corner, corner, corner};
    const float at = 0.f;
    if (color.a < 1.f) {
        DrawCheckerboard(renderer, x, y, width, height, opacity, std::min(6.f, std::max(3.f, height / 3.f)));
    }
    Color fill = color;
    fill.a *= opacity;
    renderer.fillRounded(x, y, width, height, radius, &fill, &at, 1, 0.f);
    Stroke(renderer, Box{x, y, width, height}, corner, 1.f, Themed(node, ThemeColor::Border, opacity));
}

void DrawWash(UiRenderer& renderer, const Node& node, float opacity, bool pressed, float corner) {
    const Box box = BoxOf(node);
    if (box.empty()) {
        return;
    }
    const float radius[4] = {corner, corner, corner, corner};
    const float at = 0.f;
    const Color wash = Themed(node, ThemeColor::Wash, pressed ? opacity * 2.f : opacity);
    renderer.fillRounded(box.x, box.y, box.width, box.height, radius, &wash, &at, 1, 0.f);
}

}  // namespace jadefx::chrome

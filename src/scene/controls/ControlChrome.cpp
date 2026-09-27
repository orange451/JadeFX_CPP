#include "ControlChrome.hpp"

#include "gl/UiRenderer.hpp"

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

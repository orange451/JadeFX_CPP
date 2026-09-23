#include "jadefx/scene/Controls/CheckBox.hpp"

#include "gl/UiRenderer.hpp"

#include <algorithm>
#include <cmath>

namespace jadefx {

namespace {

constexpr float kBox = 16.f;

void DrawStroke(UiRenderer& renderer, float x0, float y0, float x1, float y1, float thickness, const Color& color) {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float length = std::sqrt(dx * dx + dy * dy);
    const int steps = std::max(1, static_cast<int>(std::ceil(length * 2.f)));
    const float radius[4] = {thickness * 0.5f, thickness * 0.5f, thickness * 0.5f, thickness * 0.5f};
    const float at = 0.f;
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float x = x0 + dx * t - thickness * 0.5f;
        const float y = y0 + dy * t - thickness * 0.5f;
        renderer.fillRounded(x, y, thickness, thickness, radius, &color, &at, 1, 0.f);
    }
}

void DrawCheck(UiRenderer& renderer, float left, float top, const Color& color) {
    const float x0 = left + 4.0f;
    const float y0 = top + 8.4f;
    const float x1 = left + 6.7f;
    const float y1 = top + 11.2f;
    const float x2 = left + 12.2f;
    const float y2 = top + 4.6f;
    DrawStroke(renderer, x0, y0, x1, y1, 2.f, color);
    DrawStroke(renderer, x1, y1, x2, y2, 2.f, color);
}

void DrawDash(UiRenderer& renderer, float left, float top, const Color& color) {
    constexpr float kWidth = 8.f;
    constexpr float kHeight = 2.f;
    const float radius[4] = {1.f, 1.f, 1.f, 1.f};
    const float at = 0.f;
    renderer.fillRounded(left + (kBox - kWidth) * 0.5f, top + (kBox - kHeight) * 0.5f, kWidth, kHeight, radius, &color,
                         &at, 1, 0.f);
}

}  // namespace

CheckBox::CheckBox() : ButtonBase("") {
    setPadding(Insets{6, 14, 6, 30});
    setAlignment(Pos::CenterLeft);
    setBackground(Color::transparent());
    setPseudoState("determinate", true);
}

CheckBox::CheckBox(std::string text) : ButtonBase(std::move(text)) {
    setPadding(Insets{6, 14, 6, 30});
    setAlignment(Pos::CenterLeft);
    setBackground(Color::transparent());
    setPseudoState("determinate", true);
}

bool CheckBox::isSelected() const { return Node::isSelected(); }

void CheckBox::setSelected(bool value) { Node::setSelected(value); }

void CheckBox::setIndeterminate(bool value) {
    if (indeterminate_ == value) {
        return;
    }
    indeterminate_ = value;
    setPseudoState("indeterminate", value);
    setPseudoState("determinate", !value);
}

void CheckBox::fire() {
    if (isDisabled()) {
        return;
    }
    // OpenJFX advances unchecked -> indeterminate -> checked when all three states are allowed.
    if (isAllowIndeterminate()) {
        if (!isSelected() && !isIndeterminate()) {
            setIndeterminate(true);
        } else if (isSelected() && !isIndeterminate()) {
            setSelected(false);
        } else if (isIndeterminate()) {
            setSelected(true);
            setIndeterminate(false);
        }
    } else {
        setSelected(!isSelected());
        setIndeterminate(false);
    }
    ButtonBase::fire();
}

void CheckBox::renderContent(UiRenderer& renderer, float opacity) {
    const float x = static_cast<float>(getAbsoluteX());
    const float y = static_cast<float>(getAbsoluteY());
    const float height = static_cast<float>(getHeight());
    if (height > 0.f) {
        const float left = x + 8.f;
        const float top = y + (height - kBox) * 0.5f;
        const float radius[4] = {3.f, 3.f, 3.f, 3.f};
        const bool marked = isSelected() || isIndeterminate();
        Color fill = Color::white();
        if (marked) {
            fill = Color::rgb8(232, 240, 254);
        } else if (isPressed()) {
            fill = Color::rgb8(232, 234, 237);
        } else if (isHovered()) {
            fill = Color::rgb8(248, 249, 250);
        }
        fill.a *= opacity;
        const float at = 0.f;
        renderer.fillRounded(left, top, kBox, kBox, radius, &fill, &at, 1, 0.f);
        const float sides[4] = {2.f, 2.f, 2.f, 2.f};
        Color stroke = (marked || isFocused()) ? Color::rgb8(26, 115, 232) : Color::rgb8(95, 99, 104);
        stroke.a *= opacity;
        renderer.strokeRounded(left, top, kBox, kBox, radius, sides, stroke);
        if (marked) {
            Color mark = Color::rgb8(26, 115, 232);
            mark.a *= opacity;
            if (isIndeterminate()) {
                DrawDash(renderer, left, top, mark);
            } else {
                DrawCheck(renderer, left, top, mark);
            }
        }
    }
    Labeled::renderContent(renderer, opacity);
}

}  // namespace jadefx

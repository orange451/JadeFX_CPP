#include "jadefx/scene/Controls/RadioButton.hpp"

#include "gl/UiRenderer.hpp"

namespace jadefx {

namespace {

constexpr float kIndicator = 16.f;
constexpr float kDot = 8.f;

}  // namespace

RadioButton::RadioButton() : ToggleButton() {
    setPadding(Insets{6, 14, 6, 30});
    setAlignment(Pos::CenterLeft);
}

RadioButton::RadioButton(std::string text) : ToggleButton(std::move(text)) {
    setPadding(Insets{6, 14, 6, 30});
    setAlignment(Pos::CenterLeft);
}

void RadioButton::fire() {
    // A second activation of a selected radio does nothing, grouped or not.
    if (isDisabled() || isSelected()) {
        return;
    }
    setSelected(true);
    ButtonBase::fire();
}

void RadioButton::renderContent(UiRenderer& renderer, float opacity) {
    const float x = static_cast<float>(getAbsoluteX());
    const float y = static_cast<float>(getAbsoluteY());
    const float height = static_cast<float>(getHeight());
    if (height > 0.f) {
        const float left = x + 8.f;
        const float top = y + (height - kIndicator) * 0.5f;
        const float outerRadius[4] = {kIndicator * 0.5f, kIndicator * 0.5f, kIndicator * 0.5f, kIndicator * 0.5f};
        const float sides[4] = {2.f, 2.f, 2.f, 2.f};
        Color stroke = (isSelected() || isFocused()) ? Color::rgb8(26, 115, 232) : Color::rgb8(95, 99, 104);
        stroke.a *= opacity;
        renderer.strokeRounded(left, top, kIndicator, kIndicator, outerRadius, sides, stroke);
        if (isSelected()) {
            const float inset = (kIndicator - kDot) * 0.5f;
            const float dotRadius[4] = {kDot * 0.5f, kDot * 0.5f, kDot * 0.5f, kDot * 0.5f};
            const float at = 0.f;
            Color fill = Color::rgb8(26, 115, 232);
            fill.a *= opacity;
            renderer.fillRounded(left + inset, top + inset, kDot, kDot, dotRadius, &fill, &at, 1, 0.f);
        }
    }
    ButtonBase::renderContent(renderer, opacity);
}

}  // namespace jadefx

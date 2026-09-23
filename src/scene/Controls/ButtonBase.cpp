#include "jadefx/scene/Controls/ButtonBase.hpp"

#include "gl/UiRenderer.hpp"

namespace jadefx {

ButtonBase::ButtonBase(std::string text) : Labeled(std::move(text)) {
    setAlignment(Pos::Center);
    setPadding(Insets::axes(6, 14));
    setBackground(Color::white());
    setDefaultCursor(Cursor::Pointer);
}

void ButtonBase::fire() {
    if (!onAction_) {
        return;
    }
    ActionEvent event;
    event.source = this;
    onAction_(event);
}

void ButtonBase::handleMousePressed(const MouseEvent&) {
    if (isDisabled()) {
        return;
    }
    armed_ = true;
    setPressed(true);
}

void ButtonBase::handleMouseReleased(const MouseEvent& event) {
    const bool fireNow = armed_ && !isDisabled() && contains(event.x, event.y);
    armed_ = false;
    setPressed(false);
    if (fireNow) {
        fire();
    }
}

void ButtonBase::handleKey(KeyEvent& event) {
    if (isDisabled() || !event.pressed || event.repeat) {
        return;
    }
    if (event.key != Key::Space && event.key != Key::Enter && event.key != Key::KpEnter) {
        return;
    }
    fire();
    event.consume();
}

void ButtonBase::render(UiRenderer& renderer, float opacity) {
    Node::render(renderer, isDisabled() ? opacity * 0.45f : opacity);
}

void ButtonBase::renderContent(UiRenderer& renderer, float opacity) {
    const float x = static_cast<float>(getAbsoluteX());
    const float y = static_cast<float>(getAbsoluteY());
    const float width = static_cast<float>(getWidth());
    const float height = static_cast<float>(getHeight());
    const float radius[4] = {4.f, 4.f, 4.f, 4.f};
    if (width > 0.f && height > 0.f) {
        const ComputedStyle& style = computedStyle();
        const bool cssBorder = style.borderStyle == BorderStyle::Solid &&
                               (style.border.top > 0 || style.border.right > 0 || style.border.bottom > 0 || style.border.left > 0);
        if (!cssBorder) {
            const float sides[4] = {1.f, 1.f, 1.f, 1.f};
            Color line = Color::rgb8(218, 220, 224);
            line.a *= opacity;
            renderer.strokeRounded(x, y, width, height, radius, sides, line);
        }
        if (!isDisabled() && (isPressed() || isHovered())) {
            Color wash = Color::rgba(0.f, 0.f, 0.f, isPressed() ? 0.08f : 0.04f);
            wash.a *= opacity;
            const float at = 0.f;
            renderer.fillRounded(x, y, width, height, radius, &wash, &at, 1, 0.f);
        }
        if (isFocused()) {
            const float sides[4] = {2.f, 2.f, 2.f, 2.f};
            Color ring = Color::rgb8(26, 115, 232);
            ring.a *= opacity;
            renderer.strokeRounded(x, y, width, height, radius, sides, ring);
        }
    }
    Labeled::renderContent(renderer, opacity);
}

}  // namespace jadefx

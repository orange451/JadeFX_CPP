#include "jadefx/scene/controls/ButtonBase.hpp"

#include "ControlChrome.hpp"
#include "gl/UiRenderer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace jadefx {

ButtonBase::ButtonBase(std::string text) : Labeled(std::move(text)) {
    setAlignment(Pos::Center);
    setPadding(Insets::axes(6, 14));
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
    if (isDisabled()) {
        return;
    }
    if (event.key == Key::Space) {
        if (event.pressed && !event.repeat) {
            keyArmed_ = true;
            setPressed(true);
        } else if (!event.pressed && keyArmed_) {
            keyArmed_ = false;
            setPressed(false);
            fire();
        }
        event.consume();
        return;
    }
    if ((event.key == Key::Enter || event.key == Key::KpEnter) && event.pressed && !event.repeat) {
        fire();
        event.consume();
    }
}

void ButtonBase::handleFocusLost() {
    if (keyArmed_) {
        keyArmed_ = false;
        setPressed(false);
    }
}

void ButtonBase::render(UiRenderer& renderer, float opacity) {
    Node::render(renderer, isDisabled() ? opacity * 0.45f : opacity);
}

void ButtonBase::renderContent(UiRenderer& renderer, float opacity) {
    chrome::DrawBorder(renderer, *this, opacity);
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const double elapsed = washAt_ > 0.0 ? std::min(0.1, now - washAt_) : 0.1;
    washAt_ = now;
    const float target = isDisabled() ? 0.f : isPressed() ? 2.f : isHovered() ? 1.f : 0.f;
    wash_ += (target - wash_) * static_cast<float>(std::min(1.0, elapsed / 0.07));
    if (std::fabs(target - wash_) < 0.01f) {
        wash_ = target;
    }
    if (wash_ > 0.01f) {
        chrome::DrawWash(renderer, *this, opacity * wash_, false);
    }
    if (isFocused()) {
        chrome::DrawFocusRing(renderer, *this, opacity);
    }
    Labeled::renderContent(renderer, opacity);
}

}  // namespace jadefx

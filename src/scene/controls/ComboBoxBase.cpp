#include "jadefx/scene/controls/ComboBoxBase.hpp"

#include "ControlChrome.hpp"
#include "jadefx/scene/Scene.hpp"

namespace jadefx {
namespace {

constexpr double kPreferredHeight = 32.0;
constexpr float kCorner = 4.f;

}  // namespace

ComboBoxBase::ComboBoxBase() {
    setDefaultCursor(Cursor::Pointer);
    setPadding(Insets::axes(4, 8));
}

ComboBoxBase::~ComboBoxBase() = default;

void ComboBoxBase::show() {
    Scene* scene = getScene();
    if (scene == nullptr || isDisabled() || !canShowPopup()) {
        return;
    }
    if (popup_ == nullptr) {
        popup_ = createPopupContent();
        if (popup_ == nullptr) {
            return;
        }
    }
    if (!isShowing() && onShowing_) {
        onShowing_();
    }
    popupShowing();
    open_ = true;
    PopupOptions options;
    options.owner = this;
    options.autoHide = true;
    scene->showPopupNear(popup_, this, Side::Bottom, options);
}

void ComboBoxBase::hide() {
    if (hiding_ || !open_) {
        return;
    }
    hiding_ = true;
    Scene* scene = getScene();
    if (popup_ != nullptr && scene != nullptr && !scene->isTearingDown() && scene->isPopupShowing(popup_.get())) {
        scene->hidePopup(popup_.get());
    }
    open_ = false;
    if (scene == nullptr || !scene->isTearingDown()) {
        popupHidden();
        if (onHidden_) {
            onHidden_();
        }
    }
    hiding_ = false;
}

bool ComboBoxBase::isShowing() const {
    return popup_ != nullptr && getScene() != nullptr && getScene()->isPopupShowing(popup_.get());
}

void ComboBoxBase::setDisable(bool value) {
    Node::setDisable(value);
    if (value) {
        hide();
    }
}

void ComboBoxBase::fireAction() {
    if (!onAction_) {
        return;
    }
    ActionEvent event;
    event.source = this;
    onAction_(event);
}

void ComboBoxBase::layoutChildren() {
    Controls::layoutChildren();
    // Escape and an outside press close the popup in the scene, without calling hide().
    // The next layout is where the control learns of it.
    if (open_ && !isShowing()) {
        hide();
    }
}

void ComboBoxBase::render(UiRenderer& renderer, float opacity) {
    Node::render(renderer, isDisabled() ? opacity * 0.45f : opacity);
}

void ComboBoxBase::renderContent(UiRenderer& renderer, float opacity) {
    const float x = static_cast<float>(getAbsoluteX());
    const float y = static_cast<float>(getAbsoluteY());
    const float width = static_cast<float>(getWidth());
    const float height = static_cast<float>(getHeight());
    if (width <= 0.f || height <= 0.f) {
        return;
    }
    chrome::DrawBorder(renderer, *this, opacity, kCorner);
    if (!isDisabled() && isHovered()) {
        chrome::DrawWash(renderer, *this, opacity, isPressed() || isShowing(), kCorner);
    }
    if (!isDisabled() && isFocusWithin()) {
        chrome::DrawFocusRing(renderer, *this, opacity, kCorner);
    }
    const float left = x + static_cast<float>(contentLeft());
    const float top = y + static_cast<float>(contentTop());
    const float valueWidth = width - static_cast<float>(kArrowWidth) - static_cast<float>(contentLeft());
    renderValue(renderer, opacity, left, top, std::max(0.f, valueWidth), static_cast<float>(contentHeight()));
    const float arrowX = x + width - static_cast<float>(kArrowWidth) * 0.5f;
    chrome::DrawArrowHead(renderer, arrowX, y + height * 0.5f, Side::Bottom,
                          chrome::Themed(*this, ThemeColor::Muted, opacity));
}

double ComboBoxBase::preferredContentHeight(double) const {
    return std::max(0.0, kPreferredHeight - computedStyle().padding.height());
}

void ComboBoxBase::handleMousePressed(const MouseEvent& event) {
    if (isDisabled() || event.button != 0) {
        return;
    }
    requestFocus();
    if (isShowing()) {
        hide();
    } else {
        show();
    }
}

void ComboBoxBase::handleKey(KeyEvent& event) {
    if (!event.pressed || event.repeat || isDisabled() || isShowing()) {
        return;
    }
    const bool opens = event.key == Key::Space || event.key == Key::F4 ||
                       (event.alt && (event.key == Key::Down || event.key == Key::Up));
    if (opens) {
        show();
        event.consume();
    }
}

void ComboBoxBase::sceneChanged(Scene* previous) {
    // A scene being torn down takes its popups with it, and nothing is left to tell.
    if (previous != nullptr && previous->isTearingDown()) {
        open_ = false;
        return;
    }
    if (previous != nullptr) {
        hide();
    }
}

}  // namespace jadefx

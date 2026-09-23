#include "jadefx/scene/controls/MenuButton.hpp"

#include "jadefx/scene/Scene.hpp"

namespace jadefx {

MenuButton::MenuButton() : MenuButton(std::string()) {}

MenuButton::MenuButton(std::string text) : ButtonBase(std::move(text)) {}

MenuButton::~MenuButton() {
    if (hookedScene_ != nullptr && hookedScene_->isTearingDown()) {
        hookId_ = 0;
        hookedScene_ = nullptr;
        return;
    }
    releaseHook();
    hide();
}

ObservableList<std::shared_ptr<MenuItem>>& MenuButton::getItems() { return menu_.getItems(); }

void MenuButton::show() {
    Scene* scene = getScene();
    if (scene == nullptr || isDisabled()) {
        return;
    }
    menu_.show(*scene, this, Side::Bottom);
}

void MenuButton::hide() { menu_.hide(); }

bool MenuButton::isShowing() const { return menu_.isShowing(); }

void MenuButton::handleMousePressed(const MouseEvent&) {
    if (isDisabled()) {
        return;
    }
    // The button owns the popup, so the press does not auto-hide it.
    if (isShowing()) {
        hide();
    } else {
        show();
    }
}

void MenuButton::handleMouseReleased(const MouseEvent&) {}

void MenuButton::handleKey(KeyEvent& event) {
    if (isDisabled() || !event.pressed || event.repeat) {
        return;
    }
    if (event.key != Key::Space && event.key != Key::Enter && event.key != Key::KpEnter) {
        return;
    }
    if (isShowing()) {
        hide();
    } else {
        show();
    }
    event.consume();
}

void MenuButton::sceneChanged(Scene* previous) {
    if (previous != nullptr && previous->isTearingDown()) {
        hookId_ = 0;
        hookedScene_ = nullptr;
        return;
    }
    if (hookId_ != 0 && hookedScene_ != nullptr && hookedScene_ != getScene()) {
        hookedScene_->removeKeyHook(hookId_);
        hookId_ = 0;
        hookedScene_ = nullptr;
    }
    if (previous != nullptr && getScene() == nullptr) {
        hide();
    }
    if (getScene() != nullptr && hookId_ == 0) {
        hookId_ = getScene()->addKeyHook([this](KeyEvent& event) { dispatchAccelerator(event); });
        hookedScene_ = getScene();
    }
}

void MenuButton::dispatchAccelerator(KeyEvent& event) {
    if (event.consumed || !event.pressed || event.repeat || isDisabled() || !isVisible()) {
        return;
    }
    MenuItem* item = matchMenuAccelerator(menu_.getItems(), event);
    if (item == nullptr) {
        return;
    }
    item->fire();
    event.consume();
    hide();
}

void MenuButton::releaseHook() {
    // Ancestor teardown nulls child callbacks, so sceneChanged never runs and the
    // scene has already dropped its hooks. Touching it here would be too late.
    if (hookId_ != 0 && hookedScene_ != nullptr && getParent() == nullptr) {
        hookedScene_->removeKeyHook(hookId_);
    }
    hookId_ = 0;
    hookedScene_ = nullptr;
}

}  // namespace jadefx

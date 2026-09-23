#include "jadefx/scene/Controls/MenuItem.hpp"

#include "jadefx/scene/Controls/Menu.hpp"

namespace jadefx {

MenuItem::MenuItem() : MenuItem(std::string()) {}

MenuItem::MenuItem(std::string text) : text_(std::move(text)) {}

MenuItem::~MenuItem() = default;

void MenuItem::setText(std::string text) {
    if (text_ == text) {
        return;
    }
    text_ = std::move(text);
    notifyParent();
}

const std::string& MenuItem::getText() const { return text_; }

void MenuItem::setDisable(bool value) {
    if (disable_ == value) {
        return;
    }
    disable_ = value;
    notifyParent();
}

bool MenuItem::isDisable() const { return disable_; }

void MenuItem::setVisible(bool value) {
    if (visible_ == value) {
        return;
    }
    visible_ = value;
    notifyParent();
}

bool MenuItem::isVisible() const { return visible_; }

void MenuItem::setOnAction(ActionHandler handler) { onAction_ = std::move(handler); }

void MenuItem::fire() {
    if (disable_ || !onAction_) {
        return;
    }
    ActionEvent event;
    event.source = nullptr;
    onAction_(event);
}

void MenuItem::setAccelerator(int key, int mods) {
    if (acceleratorKey_ == key && acceleratorMods_ == mods) {
        return;
    }
    acceleratorKey_ = key;
    acceleratorMods_ = mods;
    notifyParent();
}

int MenuItem::getAcceleratorKey() const { return acceleratorKey_; }

int MenuItem::getAcceleratorMods() const { return acceleratorMods_; }

Menu* MenuItem::getParentMenu() const { return parent_; }

void MenuItem::setParentMenu(Menu* menu) { parent_ = menu; }

void MenuItem::notifyParent() const {
    if (parent_ != nullptr) {
        parent_->refreshIfOpen();
    }
}

SeparatorMenuItem::SeparatorMenuItem() = default;

}  // namespace jadefx

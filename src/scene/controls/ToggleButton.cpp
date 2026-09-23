#include "jadefx/scene/controls/ToggleButton.hpp"

#include "jadefx/scene/controls/ToggleGroup.hpp"
#include "gl/UiRenderer.hpp"

namespace jadefx {

ToggleButton::ToggleButton() : ButtonBase("") {}

ToggleButton::ToggleButton(std::string text) : ButtonBase(std::move(text)) {}

ToggleButton::~ToggleButton() {
    if (group_ != nullptr) {
        group_->removeToggle(this);
        group_ = nullptr;
    }
}

bool ToggleButton::isSelected() const { return selected_; }

ToggleGroup* ToggleButton::getToggleGroup() const { return group_; }

void ToggleButton::setSelected(bool value) {
    if (selected_ == value) {
        return;
    }
    selected_ = value;
    Node::setSelected(value);
    if (group_ == nullptr) {
        return;
    }
    if (value) {
        if (group_->getSelectedToggle() != this) {
            group_->selectToggle(this);
        }
    } else if (group_->getSelectedToggle() == this) {
        group_->selectToggle(nullptr);
    }
}

void ToggleButton::setToggleGroup(ToggleGroup* group) {
    if (group_ == group) {
        return;
    }
    if (group_ != nullptr) {
        group_->removeToggle(this);
    }
    group_ = group;
    if (group_ == nullptr) {
        return;
    }
    group_->addToggle(this);
    if (!isSelected()) {
        return;
    }
    // The group keeps its current selection. An empty group takes this button.
    if (group_->getSelectedToggle() != nullptr) {
        setSelected(false);
    } else {
        group_->selectToggle(this);
    }
}

void ToggleButton::fire() {
    if (isDisabled()) {
        return;
    }
    setSelected(!isSelected());
    ButtonBase::fire();
}

void ToggleButton::renderContent(UiRenderer& renderer, float opacity) {
    if (isSelected()) {
        const float x = static_cast<float>(getAbsoluteX());
        const float y = static_cast<float>(getAbsoluteY());
        const float width = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());
        const float radius[4] = {4.f, 4.f, 4.f, 4.f};
        if (width > 0.f && height > 0.f) {
            Color wash = Color::rgb8(26, 115, 232);
            wash.a = 0.18f * opacity;
            const float at = 0.f;
            renderer.fillRounded(x, y, width, height, radius, &wash, &at, 1, 0.f);
        }
    }
    ButtonBase::renderContent(renderer, opacity);
}

}  // namespace jadefx

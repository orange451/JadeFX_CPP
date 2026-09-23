#include "jadefx/scene/Controls/ToggleGroup.hpp"

namespace jadefx {

ToggleGroup::~ToggleGroup() {
    const std::vector<Toggle*> copy = toggles_;
    toggles_.clear();
    selected_ = nullptr;
    for (Toggle* toggle : copy) {
        if (toggle != nullptr && toggle->getToggleGroup() == this) {
            toggle->setToggleGroup(nullptr);
        }
    }
}

Toggle* ToggleGroup::getSelectedToggle() const { return selected_; }

const std::vector<Toggle*>& ToggleGroup::getToggles() const { return toggles_; }

void ToggleGroup::addToggle(Toggle* toggle) {
    if (toggle == nullptr) {
        return;
    }
    for (Toggle* existing : toggles_) {
        if (existing == toggle) {
            return;
        }
    }
    toggles_.push_back(toggle);
}

void ToggleGroup::removeToggle(Toggle* toggle) {
    if (toggle == nullptr) {
        return;
    }
    for (auto it = toggles_.begin(); it != toggles_.end();) {
        if (*it == toggle) {
            it = toggles_.erase(it);
        } else {
            ++it;
        }
    }
    // Leave the toggle's own selected flag alone. Nobody else becomes selected.
    if (selected_ == toggle) {
        selected_ = nullptr;
    }
}

void ToggleGroup::selectToggle(Toggle* next) {
    if (selected_ == next) {
        return;
    }
    Toggle* previous = selected_;
    // Assign before notifying so setSelected cannot recurse.
    selected_ = next;
    if (previous != nullptr && previous != next) {
        previous->setSelected(false);
    }
    if (next != nullptr && !next->isSelected()) {
        next->setSelected(true);
    }
}

}  // namespace jadefx

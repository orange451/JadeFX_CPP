#pragma once

#include "jadefx/scene/controls/Toggle.hpp"

#include <vector>

namespace jadefx {

// At most one selected toggle. Selecting another clears the previous one.
// Removing the selected toggle leaves the rest unselected.
class ToggleGroup {
public:
    ToggleGroup() = default;
    ~ToggleGroup();

    ToggleGroup(const ToggleGroup&) = delete;
    ToggleGroup& operator=(const ToggleGroup&) = delete;
    ToggleGroup(ToggleGroup&&) = delete;
    ToggleGroup& operator=(ToggleGroup&&) = delete;

    Toggle* getSelectedToggle() const;
    // nullptr clears. The new selection is stored before either toggle is told.
    void selectToggle(Toggle* toggle);
    const std::vector<Toggle*>& getToggles() const;
    void addToggle(Toggle* toggle);     // no duplicates. Does not select.
    void removeToggle(Toggle* toggle);  // if it was selected, clear the selection without selecting another.

private:
    std::vector<Toggle*> toggles_;
    Toggle* selected_ = nullptr;
};

}  // namespace jadefx

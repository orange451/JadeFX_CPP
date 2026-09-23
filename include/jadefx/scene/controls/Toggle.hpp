#pragma once

namespace jadefx {

class ToggleGroup;

// Selected or not. A ToggleGroup clears the other toggles. This is not a node.
class Toggle {
public:
    virtual ~Toggle() = default;
    virtual bool isSelected() const = 0;
    virtual void setSelected(bool selected) = 0;
    virtual ToggleGroup* getToggleGroup() const = 0;
    virtual void setToggleGroup(ToggleGroup* group) = 0;
};

}  // namespace jadefx

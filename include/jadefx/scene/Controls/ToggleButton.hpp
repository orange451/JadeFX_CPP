#pragma once

#include "jadefx/scene/Controls/ButtonBase.hpp"
#include "jadefx/scene/Controls/Toggle.hpp"

#include <string>

namespace jadefx {

// A button that stays selected until it is clicked again. In a ToggleGroup the
// group's selection wins when this button joins, and a later click can turn it off.
class ToggleButton : public ButtonBase, public Toggle {
public:
    ToggleButton();
    explicit ToggleButton(std::string text);
    const char* getElementType() const override { return "togglebutton"; }
    bool isSelected() const override;
    void setSelected(bool selected) override;
    ToggleGroup* getToggleGroup() const override;
    void setToggleGroup(ToggleGroup* group) override;
    void fire() override;
    ~ToggleButton() override;

protected:
    void renderContent(UiRenderer& renderer, float opacity) override;

private:
    bool selected_ = false;
    ToggleGroup* group_ = nullptr;
};

}  // namespace jadefx

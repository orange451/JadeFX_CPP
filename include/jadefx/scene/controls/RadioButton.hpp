#pragma once

#include "jadefx/scene/controls/ToggleButton.hpp"

#include <string>

namespace jadefx {

// Selects on activation and does not toggle off, even when it has no group.
class RadioButton : public ToggleButton {
public:
    RadioButton();
    explicit RadioButton(std::string text);

    const char* getElementType() const override { return "radiobutton"; }

    void fire() override;

protected:
    void renderContent(UiRenderer& renderer, float opacity) override;
};

}  // namespace jadefx

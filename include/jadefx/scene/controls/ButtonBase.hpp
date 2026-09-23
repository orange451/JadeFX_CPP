#pragma once

#include "jadefx/scene/controls/Labeled.hpp"

namespace jadefx {

// A labeled control that fires an action from a click, Space, or Enter.
// The pointer has to be released inside the control. :active follows the mouse
// press and a keyboard arm. Disabled controls ignore input.
class ButtonBase : public Labeled {
public:
    void setOnAction(ActionHandler handler) { onAction_ = std::move(handler); }
    virtual void fire();
    bool isArmed() const { return armed_; }

protected:
    explicit ButtonBase(std::string text);

    void handleMousePressed(const MouseEvent& event) override;
    void handleMouseReleased(const MouseEvent& event) override;
    void handleKey(KeyEvent& event) override;
    void render(UiRenderer& renderer, float opacity) override;
    void renderContent(UiRenderer& renderer, float opacity) override;

private:
    ActionHandler onAction_;
    bool armed_ = false;
};

}  // namespace jadefx

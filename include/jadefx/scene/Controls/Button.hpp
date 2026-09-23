#pragma once

#include "jadefx/scene/Controls/ButtonBase.hpp"

namespace jadefx {

// A push button. setDefaultButton and setCancelButton mark the button Alert
// activates for Enter and Escape. The button itself fires when it is focused.
class Button : public ButtonBase {
public:
    Button();
    explicit Button(std::string text);

    const char* getElementType() const override { return "button"; }

    void setDefaultButton(bool value) { defaultButton_ = value; }
    bool isDefaultButton() const { return defaultButton_; }
    void setCancelButton(bool value) { cancelButton_ = value; }
    bool isCancelButton() const { return cancelButton_; }

private:
    bool defaultButton_ = false;
    bool cancelButton_ = false;
};

}  // namespace jadefx

#pragma once

#include "jadefx/scene/controls/ButtonBase.hpp"

#include <string>

namespace jadefx {

// Checked, unchecked, or indeterminate. A click, Space, or Enter toggles checked.
// allowIndeterminate cycles unchecked, then indeterminate, then checked.
// An indeterminate box draws a dash, including when it is also checked.
// Disabled input is ignored, and fire() does not run the action.
class CheckBox : public ButtonBase {
public:
    CheckBox();
    explicit CheckBox(std::string text);

    const char* getElementType() const override { return "checkbox"; }

    bool isSelected() const;
    void setSelected(bool selected);

    bool isIndeterminate() const { return indeterminate_; }
    void setIndeterminate(bool value);

    bool isAllowIndeterminate() const { return allowIndeterminate_; }
    void setAllowIndeterminate(bool value) { allowIndeterminate_ = value; }

    void fire() override;

protected:
    void renderContent(UiRenderer& renderer, float opacity) override;

private:
    bool indeterminate_ = false;
    bool allowIndeterminate_ = false;
};

}  // namespace jadefx

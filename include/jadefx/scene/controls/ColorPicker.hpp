#pragma once

#include "jadefx/paint/Color.hpp"
#include "jadefx/scene/controls/ColorChooser.hpp"
#include "jadefx/scene/controls/ComboBoxBase.hpp"

#include <functional>
#include <memory>

namespace jadefx {

// A swatch and its hex code that open a ColorChooser, in the shape of OpenJFX
// ColorPicker and the HTML color input. The value follows the chooser while it
// is open. Enter or a press outside keeps the new color, and Escape puts the
// old one back. Closing on a new color adds it to the recent colors and fires
// the action.
class ColorPicker : public ComboBoxBase {
public:
    ColorPicker();
    explicit ColorPicker(Color value);
    ~ColorPicker() override;

    const char* getElementType() const override { return "color-picker"; }

    // Does not fire the action.
    void setValue(Color value);
    Color getValue() const { return value_; }

    // Runs each time the value changes, including while the chooser is open.
    void setOnValueChanged(std::function<void()> handler) { onChanged_ = std::move(handler); }

    // The chooser the popup shows. Its presets and recent colors can be changed at any time.
    ColorChooser& getColorChooser() { return *chooser_; }

protected:
    std::shared_ptr<Node> createPopupContent() override;
    void popupShowing() override;
    void popupHidden() override;
    bool handlePopupKey(KeyEvent& event) override;
    void renderValue(UiRenderer& renderer, float opacity, float x, float y, float width, float height) override;
    double preferredContentWidth(double innerAvailable) const override;

private:
    Color value_ = Color::white();
    Color original_ = Color::white();
    std::shared_ptr<ColorChooser> chooser_;
    std::function<void()> onChanged_;
    bool cancelled_ = false;
};

}  // namespace jadefx

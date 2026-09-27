#pragma once

#include "jadefx/collections/ObservableList.hpp"
#include "jadefx/paint/Color.hpp"
#include "jadefx/scene/controls/Controls.hpp"

#include <functional>
#include <memory>

namespace jadefx {

// A color editor laid out like Paint.NET's color window: a hue and saturation
// wheel with a before-and-after swatch, the preset palette, and recent colors
// on the left; red, green, blue, the hex code, hue, saturation, value, and
// alpha on the right, each channel a slider over its own gradient with a
// number box beside it. Every part follows the others as any one changes.
// ColorPicker shows one in its popup; it also works on its own, in a dialog or
// a panel. Hue is kept while the color is a gray, so dragging saturation back
// up returns to the same hue.
class ColorChooser : public Controls {
public:
    // The most recent colors kept.
    static constexpr std::size_t kRecentLimit = 12;

    ColorChooser();
    explicit ColorChooser(Color initial);
    ~ColorChooser() override;

    const char* getElementType() const override { return "color-chooser"; }

    // Does not run the value handler.
    void setValue(Color color);
    Color getValue() const { return value_; }
    // Shown beside the value in the swatch; a click on it goes back to it.
    void setOriginalValue(Color color);
    Color getOriginalValue() const { return original_; }

    // The palette under the wheel. The default is twelve grays and twelve hues in five shades.
    ObservableList<Color>& getPresets() { return presets_; }
    // Newest first, at most kRecentLimit.
    ObservableList<Color>& getRecentColors() { return recent_; }
    // Moves color to the front of the recent colors.
    void addRecentColor(Color color);

    // The transparency slider and alpha in the hex code, on by default. Off, the
    // chooser picks opaque colors only, as for an RGB value such as a Color3, and
    // makes its value opaque; the web's color input works the same way without
    // its alpha attribute.
    void setShowAlpha(bool show);
    bool isShowAlpha() const { return showAlpha_; }
    // The recent colors under the palette, on by default.
    void setShowRecentColors(bool show);
    bool isShowRecentColors() const { return showRecent_; }

    // Runs after the value changes from the chooser's own parts.
    void setOnValueChanged(std::function<void()> handler) { onChanged_ = std::move(handler); }
    // Applies text typed but not yet entered in the hex field or a number box.
    void commitEdits();

protected:
    void layoutChildren() override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;

private:
    struct Parts;
    enum class Channel { Red, Green, Blue, Hue, Saturation, Brightness, Alpha };

    void build();
    void rebuildPresets();
    // Takes a new color from red, green, blue, and alpha, keeping hue and saturation where they are undefined.
    void applyColor(Color color, bool notify);
    void applyHsb(double hue, double saturation, double brightness, bool notify);
    void applyChannel(Channel channel, double value);
    bool applyHex(const std::string& text);
    // Shows the current color in every part.
    void refresh();
    // refresh, then the value handler when notify is set.
    void changed(bool notify);

    Color value_ = Color::white();
    Color original_ = Color::white();
    double hue_ = 0;
    double saturation_ = 0;
    double brightness_ = 1;
    ObservableList<Color> presets_;
    ObservableList<Color> recent_;
    std::function<void()> onChanged_;
    std::unique_ptr<Parts> parts_;
    bool syncing_ = false;
    bool showAlpha_ = true;
    bool showRecent_ = true;
};

}  // namespace jadefx

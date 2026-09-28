#include "jadefx/scene/controls/ColorPicker.hpp"

#include "ControlChrome.hpp"
#include "gl/UiRenderer.hpp"
#include "jadefx/scene/Scene.hpp"

#include <algorithm>

namespace jadefx {
namespace {

constexpr float kSwatchWidth = 22.f;
constexpr float kSwatchGap = 8.f;

}  // namespace

ColorPicker::ColorPicker() : ColorPicker(Color::white()) {}

ColorPicker::ColorPicker(Color value) : value_(value), original_(value), chooser_(std::make_shared<ColorChooser>(value)) {
    chooser_->setOnValueChanged([this] {
        value_ = chooser_->getValue();
        if (onChanged_) {
            onChanged_();
        }
    });
}

ColorPicker::~ColorPicker() = default;

void ColorPicker::setValue(Color value) {
    // The chooser may refuse part of it, such as alpha when it shows none.
    chooser_->setValue(value);
    value_ = chooser_->getValue();
    if (onChanged_) {
        onChanged_();
    }
}

std::shared_ptr<Node> ColorPicker::createPopupContent() { return chooser_; }

void ColorPicker::popupShowing() {
    if (isShowing()) {
        return;
    }
    original_ = value_;
    cancelled_ = false;
    chooser_->setOriginalValue(value_);
    chooser_->setValue(value_);
}

bool ColorPicker::handlePopupKey(KeyEvent& event) {
    // Ahead of the scene's own Escape, which would close the popup without telling it apart from a press outside.
    if (!event.pressed) {
        return false;
    }
    if (event.key == Key::Escape) {
        cancelled_ = true;
    } else if (event.key != Key::Enter && event.key != Key::KpEnter) {
        return false;
    }
    hide();
    return true;
}

void ColorPicker::popupHidden() {
    if (cancelled_) {
        cancelled_ = false;
        setValue(original_);
        return;
    }
    chooser_->commitEdits();
    if (!near(value_, original_)) {
        chooser_->addRecentColor(value_);
        fireAction();
    }
}

void ColorPicker::renderValue(UiRenderer& renderer, float opacity, float x, float y, float width, float height) {
    const float swatchHeight = std::max(0.f, height - 6.f);
    chrome::DrawColorSwatch(renderer, *this, x, y + (height - swatchHeight) * 0.5f, kSwatchWidth, swatchHeight, value_,
                            opacity);
    const Font font = chrome::FontOf(*this);
    const std::string text = value_.a < 1.f ? value_.toHex(true) : value_.toHex();
    const float textX = x + kSwatchWidth + kSwatchGap;
    const float textWidth = x + width - textX;
    if (textWidth <= 0.f) {
        return;
    }
    Color color = computedStyle().color;
    color.a *= opacity;
    renderer.pushClip(textX, y, textWidth, height);
    renderer.text(textX, y + (height - font.lineHeight()) * 0.5f, text, font.family(), font.size(), color,
                  computedStyle().subpixel);
    renderer.popClip();
}

double ColorPicker::preferredContentWidth(double) const {
    // Wide enough for any hex code, so the box does not change size with the color.
    const double text = chrome::FontOf(*this).measureWidth("#dddddddd");
    return kSwatchWidth + kSwatchGap + text + kArrowWidth;
}

}  // namespace jadefx

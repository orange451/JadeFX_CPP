#include "jadefx/scene/Controls/Spinner.hpp"

#include "gl/UiRenderer.hpp"
#include "jadefx/scene/Scene.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>

namespace jadefx {
namespace {

constexpr double kArrow = 22;
constexpr double kSplitArrow = 16;
constexpr float kCorner = 4.f;

std::string Trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && text[begin] <= ' ') {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && text[end - 1] <= ' ') {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::string FormatDouble(double value) {
    if (!std::isfinite(value)) {
        return {};
    }
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(2);
    out << value;
    std::string text = out.str();
    const std::size_t dot = text.find('.');
    if (dot != std::string::npos) {
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
    }
    if (text == "-0") {
        text = "0";
    }
    return text;
}

// OpenJFX Spinner.wrapValue, with a positive remainder so the result stays in range.
int WrapIndex(long long value, int min, int max) {
    if (max < min) {
        return min;
    }
    const long long span = static_cast<long long>(max) - static_cast<long long>(min) + 1;
    if (span <= 0) {
        return min;
    }
    if (value < 0) {
        value = static_cast<long long>(max) + value % span + 1;
    }
    long long shifted = (value - min) % span;
    if (shifted < 0) {
        shifted += span;
    }
    return static_cast<int>(static_cast<long long>(min) + shifted);
}

int ClampIndex(long long value, int min, int max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return static_cast<int>(value);
}

double WrapDouble(double current, double next, double min, double max) {
    if (next >= min && next <= max) {
        return next;
    }
    const double span = max - min;
    if (!(span > 0) || !std::isfinite(next)) {
        return next < min ? min : max;
    }
    double remainder = std::fmod(next, span);
    const double dust = 1e-9 * std::max(1.0, std::fabs(span));
    if (std::fabs(remainder) <= dust) {
        remainder = 0;
    }
    if (remainder == 0) {
        return next >= current ? max : min;
    }
    return next > max ? min + remainder : max + remainder;
}

void DrawTriangle(UiRenderer& renderer, float centerX, float centerY, bool vertical, bool forward, const Color& color) {
    if (color.a <= 0.f) {
        return;
    }
    constexpr float kWide = 8.f;
    constexpr float kTall = 5.f;
    const float radius[4] = {};
    const float at = 0.f;
    if (vertical) {
        // The point sits one sixth off the box center, so shift it back onto the midline.
        const float top = centerY - kTall * 0.5f + (forward ? -kTall / 6.f : kTall / 6.f);
        for (float row = 0.f; row < kTall; row += 1.f) {
            const float y = top + row;
            const float t = (row + 0.5f) / kTall;
            const float across = kWide * (forward ? t : 1.f - t);
            if (across < 0.4f) {
                continue;
            }
            renderer.fillRounded(centerX - across * 0.5f, y, across, 1.f, radius, &color, &at, 1, 0.f);
        }
        return;
    }
    const float left = centerX - kTall * 0.5f + (forward ? kTall / 6.f : -kTall / 6.f);
    for (float column = 0.f; column < kTall; column += 1.f) {
        const float x = left + column;
        const float t = (column + 0.5f) / kTall;
        const float across = kWide * (forward ? 1.f - t : t);
        if (across < 0.4f) {
            continue;
        }
        renderer.fillRounded(x, centerY - across * 0.5f, 1.f, across, radius, &color, &at, 1, 0.f);
    }
}

}  // namespace

class SpinnerArrow : public Controls {
public:
    SpinnerArrow(Spinner* owner, bool increment) : owner_(owner), increment_(increment) {
        setElementId(increment ? "increment" : "decrement");
    }

    const char* getElementType() const override {
        return increment_ ? "increment-arrow-button" : "decrement-arrow-button";
    }

    void unbind() { owner_ = nullptr; }

protected:
    void handleMousePressed(const MouseEvent&) override {
        if (owner_ != nullptr) {
            owner_->pressArrow(increment_);
        }
    }

    void handleMouseReleased(const MouseEvent&) override {
        if (owner_ != nullptr) {
            owner_->releaseArrow();
        }
    }

    void renderContent(UiRenderer& renderer, float opacity) override {
        const float x = static_cast<float>(getAbsoluteX());
        const float y = static_cast<float>(getAbsoluteY());
        const float width = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());
        if (width <= 0.f || height <= 0.f || owner_ == nullptr) {
            return;
        }
        if ((isHovered() || isPressed()) && !computedStyle().background.visible) {
            const float radius[4] = {3.f, 3.f, 3.f, 3.f};
            const float at = 0.f;
            Color fill = isPressed() ? Color::rgb8(232, 234, 237) : Color::rgb8(241, 243, 244);
            fill.a *= opacity;
            renderer.fillRounded(x + 1.f, y + 1.f, std::max(0.f, width - 2.f), std::max(0.f, height - 2.f), radius, &fill,
                                 &at, 1, 0.f);
        }
        const bool vertical = owner_->arrowsAreVertical();
        Color mark = Color::rgb8(95, 99, 104);
        mark.a *= opacity;
        DrawTriangle(renderer, x + width * 0.5f, y + height * 0.5f, vertical, increment_, mark);
    }

private:
    Spinner* owner_ = nullptr;
    bool increment_ = false;
};

class SpinnerEditor : public TextField {
public:
    explicit SpinnerEditor(Spinner* owner) : owner_(owner) {}

    void unbind() { owner_ = nullptr; }

protected:
    void handleKey(KeyEvent& event) override {
        if (owner_ == nullptr || !owner_->isEditable()) {
            return;
        }
        if (event.pressed && owner_->claimsArrowKey(event.key)) {
            return;
        }
        TextField::handleKey(event);
    }

    void handleText(TextEvent& event) override {
        if (owner_ == nullptr || !owner_->isEditable()) {
            return;
        }
        TextField::handleText(event);
    }

private:
    Spinner* owner_ = nullptr;
};

void SpinnerValueFactory::changed() {
    if (owner_ != nullptr) {
        owner_->onFactoryChanged();
    }
    if (onChanged_) {
        onChanged_();
    }
}

IntegerSpinnerValueFactory::IntegerSpinnerValueFactory(int min, int max) : IntegerSpinnerValueFactory(min, max, min, 1) {}

IntegerSpinnerValueFactory::IntegerSpinnerValueFactory(int min, int max, int initialValue)
    : IntegerSpinnerValueFactory(min, max, initialValue, 1) {}

IntegerSpinnerValueFactory::IntegerSpinnerValueFactory(int min, int max, int initialValue, int amountToStepBy)
    : min_(min), max_(max), step_(amountToStepBy) {
    const bool inside = initialValue >= min_ && initialValue <= max_;
    setValue(inside ? initialValue : min_);
}

void IntegerSpinnerValueFactory::setMin(int value) {
    if (value > max_) {
        value = max_;
    }
    min_ = value;
    if (has_ && value_ < min_) {
        setValue(min_);
    }
}

void IntegerSpinnerValueFactory::setMax(int value) {
    if (value < min_) {
        value = min_;
    }
    max_ = value;
    if (has_ && value_ > max_) {
        setValue(max_);
    }
}

void IntegerSpinnerValueFactory::setValue(int value) {
    if (value < min_) {
        value = min_;
    } else if (value > max_) {
        value = max_;
    }
    if (has_ && value_ == value) {
        return;
    }
    value_ = value;
    has_ = true;
    changed();
}

void IntegerSpinnerValueFactory::increment(int steps) {
    const long long next = static_cast<long long>(value_) + static_cast<long long>(steps) * static_cast<long long>(step_);
    setValue(wrap_ ? WrapIndex(next, min_, max_) : ClampIndex(next, min_, max_));
}

void IntegerSpinnerValueFactory::decrement(int steps) {
    const long long next = static_cast<long long>(value_) - static_cast<long long>(steps) * static_cast<long long>(step_);
    setValue(wrap_ ? WrapIndex(next, min_, max_) : ClampIndex(next, min_, max_));
}

std::string IntegerSpinnerValueFactory::valueText() const { return std::to_string(value_); }

bool IntegerSpinnerValueFactory::commitText(const std::string& text) {
    const std::string trimmed = Trim(text);
    if (trimmed.empty()) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(trimmed.c_str(), &end, 10);
    if (end == trimmed.c_str() || end == nullptr || *end != '\0' || errno == ERANGE) {
        return false;
    }
    if (parsed < static_cast<long>(std::numeric_limits<int>::min()) ||
        parsed > static_cast<long>(std::numeric_limits<int>::max())) {
        return false;
    }
    setValue(static_cast<int>(parsed));
    return true;
}

DoubleSpinnerValueFactory::DoubleSpinnerValueFactory(double min, double max) : DoubleSpinnerValueFactory(min, max, min, 1) {}

DoubleSpinnerValueFactory::DoubleSpinnerValueFactory(double min, double max, double initialValue)
    : DoubleSpinnerValueFactory(min, max, initialValue, 1) {}

DoubleSpinnerValueFactory::DoubleSpinnerValueFactory(double min, double max, double initialValue, double amountToStepBy)
    : min_(min), max_(max), step_(amountToStepBy) {
    if (!std::isfinite(min_) || !std::isfinite(max_) || !std::isfinite(step_)) {
        min_ = 0;
        max_ = 0;
        step_ = 1;
    }
    const bool inside = initialValue >= min_ && initialValue <= max_ && std::isfinite(initialValue);
    setValue(inside ? initialValue : min_);
}

void DoubleSpinnerValueFactory::setMin(double value) {
    if (!std::isfinite(value)) {
        return;
    }
    if (value > max_) {
        value = max_;
    }
    min_ = value;
    if (has_ && value_ < min_) {
        setValue(min_);
    }
}

void DoubleSpinnerValueFactory::setMax(double value) {
    if (!std::isfinite(value)) {
        return;
    }
    if (value < min_) {
        value = min_;
    }
    max_ = value;
    if (has_ && value_ > max_) {
        setValue(max_);
    }
}

void DoubleSpinnerValueFactory::setValue(double value) {
    if (!std::isfinite(value)) {
        return;
    }
    if (value < min_) {
        value = min_;
    } else if (value > max_) {
        value = max_;
    }
    if (has_ && value_ == value) {
        return;
    }
    value_ = value;
    has_ = true;
    changed();
}

void DoubleSpinnerValueFactory::setAmountToStepBy(double value) {
    if (!std::isfinite(value)) {
        return;
    }
    step_ = value;
}

void DoubleSpinnerValueFactory::increment(int steps) {
    const double next = value_ + static_cast<double>(steps) * step_;
    setValue(wrap_ ? WrapDouble(value_, next, min_, max_) : (next < min_ ? min_ : (next > max_ ? max_ : next)));
}

void DoubleSpinnerValueFactory::decrement(int steps) {
    const double next = value_ - static_cast<double>(steps) * step_;
    setValue(wrap_ ? WrapDouble(value_, next, min_, max_) : (next < min_ ? min_ : (next > max_ ? max_ : next)));
}

std::string DoubleSpinnerValueFactory::valueText() const { return FormatDouble(value_); }

bool DoubleSpinnerValueFactory::commitText(const std::string& text) {
    const std::string trimmed = Trim(text);
    if (trimmed.empty()) {
        return false;
    }
    char* end = nullptr;
    const double parsed = std::strtod(trimmed.c_str(), &end);
    if (end == trimmed.c_str() || end == nullptr || *end != '\0' || !std::isfinite(parsed)) {
        return false;
    }
    setValue(parsed);
    return true;
}

ListSpinnerValueFactory::ListSpinnerValueFactory(std::vector<std::string> items) {
    for (std::string& item : items) {
        items_.add(std::move(item));
    }
    items_.setAddCallback([this](const std::string&) { itemsChanged(); });
    items_.setRemoveCallback([this](const std::string&) { itemsChanged(); });
    if (!items_.empty()) {
        setValue(items_[0]);
    }
}

ListSpinnerValueFactory::~ListSpinnerValueFactory() {
    items_.setAddCallback(nullptr);
    items_.setRemoveCallback(nullptr);
}

void ListSpinnerValueFactory::setValue(std::string value) {
    int index = indexOf(value);
    if (index < 0) {
        mutating_ = true;
        items_.add(std::move(value));
        mutating_ = false;
        index = static_cast<int>(items_.size()) - 1;
    }
    select(index);
}

void ListSpinnerValueFactory::increment(int steps) {
    if (items_.empty()) {
        return;
    }
    const int max = static_cast<int>(items_.size()) - 1;
    const long long next = static_cast<long long>(has_ ? current_ : 0) + steps;
    select(wrap_ ? WrapIndex(next, 0, max) : ClampIndex(next, 0, max));
}

void ListSpinnerValueFactory::decrement(int steps) {
    if (items_.empty()) {
        return;
    }
    const int max = static_cast<int>(items_.size()) - 1;
    const long long next = static_cast<long long>(has_ ? current_ : 0) - steps;
    select(wrap_ ? WrapIndex(next, 0, max) : ClampIndex(next, 0, max));
}

std::string ListSpinnerValueFactory::valueText() const { return has_ ? value_ : std::string(); }

bool ListSpinnerValueFactory::commitText(const std::string& text) {
    setValue(text);
    return true;
}

void ListSpinnerValueFactory::select(int index) {
    if (items_.empty()) {
        current_ = 0;
        if (has_) {
            has_ = false;
            value_.clear();
            changed();
        }
        return;
    }
    if (index < 0) {
        index = 0;
    }
    if (index >= static_cast<int>(items_.size())) {
        index = static_cast<int>(items_.size()) - 1;
    }
    if (has_ && current_ == index && value_ == items_[static_cast<std::size_t>(index)]) {
        return;
    }
    current_ = index;
    value_ = items_[static_cast<std::size_t>(index)];
    has_ = true;
    changed();
}

void ListSpinnerValueFactory::itemsChanged() {
    if (mutating_) {
        return;
    }
    if (items_.empty()) {
        current_ = 0;
        if (has_) {
            has_ = false;
            value_.clear();
            changed();
        }
        return;
    }
    if (current_ < 0 || current_ >= static_cast<int>(items_.size())) {
        current_ = 0;
    }
    const std::string& now = items_[static_cast<std::size_t>(current_)];
    if (!has_ || value_ != now) {
        value_ = now;
        has_ = true;
        changed();
    }
}

int ListSpinnerValueFactory::indexOf(const std::string& value) const {
    const std::vector<std::string>& items = items_.items();
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i] == value) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

Spinner::Spinner() {
    setPadding(Insets::axes(6, 8));
    setBackground(Color::white());
    editor_ = std::make_shared<SpinnerEditor>(this);
    editor_->setPrefColumnCount(8);
    editor_->setBackground(Color::transparent());
    editor_->setPadding(Insets::axes(0, 2));
    editor_->setStylesheet("textfield { padding: 0 2px; border-width: 0; background-color: transparent; }");
    editor_->setEditable(false);
    editor_->setMouseTransparent(true);
    editor_->setOnAction([this](ActionEvent&) { commitValue(); });
    incrementArrow_ = std::make_shared<SpinnerArrow>(this, true);
    decrementArrow_ = std::make_shared<SpinnerArrow>(this, false);
    children().add(editor_);
    children().add(decrementArrow_);
    children().add(incrementArrow_);
}

Spinner::Spinner(std::shared_ptr<SpinnerValueFactory> factory) : Spinner() { setValueFactory(std::move(factory)); }

Spinner::Spinner(int min, int max, int initialValue)
    : Spinner(std::make_shared<IntegerSpinnerValueFactory>(min, max, initialValue)) {}

Spinner::Spinner(int min, int max, int initialValue, int amountToStepBy)
    : Spinner(std::make_shared<IntegerSpinnerValueFactory>(min, max, initialValue, amountToStepBy)) {}

Spinner::Spinner(double min, double max, double initialValue)
    : Spinner(std::make_shared<DoubleSpinnerValueFactory>(min, max, initialValue)) {}

Spinner::Spinner(double min, double max, double initialValue, double amountToStepBy)
    : Spinner(std::make_shared<DoubleSpinnerValueFactory>(min, max, initialValue, amountToStepBy)) {}

Spinner::Spinner(std::vector<std::string> items) : Spinner(std::make_shared<ListSpinnerValueFactory>(std::move(items))) {}

Spinner::~Spinner() {
    if (factory_) {
        factory_->attach(nullptr);
    }
    if (editor_) {
        editor_->setOnAction(nullptr);
        editor_->unbind();
    }
    if (incrementArrow_) {
        incrementArrow_->unbind();
    }
    if (decrementArrow_) {
        decrementArrow_->unbind();
    }
}

TextField* Spinner::getEditor() const { return editor_.get(); }

void Spinner::setValueFactory(std::shared_ptr<SpinnerValueFactory> factory) {
    if (factory_.get() == factory.get()) {
        syncFromFactory();
        return;
    }
    if (factory_) {
        factory_->attach(nullptr);
    }
    factory_ = std::move(factory);
    if (factory_) {
        factory_->attach(this);
    }
    syncFromFactory();
    if (onChanged_) {
        onChanged_();
    }
}

void Spinner::setEditable(bool editable) {
    editable_ = editable;
    if (editor_ != nullptr) {
        editor_->setEditable(editable_);
        editor_->setMouseTransparent(!editable_);
    }
}

void Spinner::setPromptText(std::string text) {
    if (editor_ != nullptr) {
        editor_->setPromptText(std::move(text));
    }
}

const std::string& Spinner::getPromptText() const {
    static const std::string empty;
    return editor_ != nullptr ? editor_->getPromptText() : empty;
}

void Spinner::setInitialDelay(double seconds) { initialDelay_ = seconds > 0 ? seconds : 0; }

void Spinner::setRepeatDelay(double seconds) { repeatDelay_ = seconds > 0 ? seconds : 0; }

void Spinner::setDisable(bool value) {
    Node::setDisable(value);
    if (editor_ != nullptr) {
        editor_->setDisable(value);
    }
    if (value) {
        spinning_ = false;
    }
}

void Spinner::increment(int steps) {
    if (factory_ == nullptr || steps == 0) {
        return;
    }
    if (steps < 0) {
        decrement(-steps);
        return;
    }
    if (!commitValue()) {
        return;
    }
    factory_->increment(steps);
}

void Spinner::decrement(int steps) {
    if (factory_ == nullptr || steps == 0) {
        return;
    }
    if (steps < 0) {
        increment(-steps);
        return;
    }
    if (!commitValue()) {
        return;
    }
    factory_->decrement(steps);
}

bool Spinner::commitValue() {
    if (committing_ || !editable_ || editor_ == nullptr || factory_ == nullptr) {
        return true;
    }
    committing_ = true;
    const bool ok = factory_->commitText(editor_->getText());
    committing_ = false;
    if (!ok) {
        cancelEdit();
        return false;
    }
    syncFromFactory();
    return true;
}

void Spinner::cancelEdit() {
    if (!editable_) {
        return;
    }
    syncFromFactory();
}

bool Spinner::arrowsAreVertical() const {
    const ArrowLayout layout = arrowLayout();
    return layout != ArrowLayout::RightHorizontal && layout != ArrowLayout::LeftHorizontal &&
           layout != ArrowLayout::SplitHorizontal;
}

Spinner::ArrowLayout Spinner::arrowLayout() const {
    bool leftVertical = false;
    bool leftHorizontal = false;
    bool rightHorizontal = false;
    bool splitVertical = false;
    bool splitHorizontal = false;
    for (const std::string& name : getClassList().items()) {
        if (name == kArrowsOnLeftVertical) {
            leftVertical = true;
        } else if (name == kArrowsOnLeftHorizontal) {
            leftHorizontal = true;
        } else if (name == kArrowsOnRightHorizontal) {
            rightHorizontal = true;
        } else if (name == kSplitArrowsVertical) {
            splitVertical = true;
        } else if (name == kSplitArrowsHorizontal) {
            splitHorizontal = true;
        }
    }
    if (leftVertical) {
        return ArrowLayout::LeftVertical;
    }
    if (leftHorizontal) {
        return ArrowLayout::LeftHorizontal;
    }
    if (rightHorizontal) {
        return ArrowLayout::RightHorizontal;
    }
    if (splitVertical) {
        return ArrowLayout::SplitVertical;
    }
    if (splitHorizontal) {
        return ArrowLayout::SplitHorizontal;
    }
    return ArrowLayout::RightVertical;
}

bool Spinner::claimsArrowKey(int key) const {
    if (arrowsAreVertical()) {
        return key == Key::Up || key == Key::Down;
    }
    return key == Key::Left || key == Key::Right;
}

void Spinner::syncFromFactory() {
    if (editor_ == nullptr) {
        return;
    }
    const std::string text = factory_ != nullptr ? factory_->valueText() : std::string();
    if (editor_->getText() != text) {
        editor_->setText(text);
    }
}

void Spinner::onFactoryChanged() {
    syncFromFactory();
    if (onChanged_) {
        onChanged_();
    }
}

void Spinner::pressArrow(bool incrementButton) {
    if (isDisabled() || factory_ == nullptr) {
        return;
    }
    requestFocus();
    if (editable_ && !commitValue()) {
        spinning_ = false;
        return;
    }
    spinning_ = true;
    spinUp_ = incrementButton;
    if (incrementButton) {
        factory_->increment(1);
    } else {
        factory_->decrement(1);
    }
    const double now = getScene() != nullptr ? getScene()->timeSeconds() : 0;
    nextSpin_ = now + initialDelay_;
}

void Spinner::releaseArrow() { spinning_ = false; }

void Spinner::advanceSpin() {
    if (!spinning_ || isDisabled() || factory_ == nullptr || getScene() == nullptr) {
        return;
    }
    const double now = getScene()->timeSeconds();
    if (now < nextSpin_) {
        return;
    }
    int guard = 0;
    while (now >= nextSpin_ && guard < 32) {
        if (spinUp_) {
            factory_->increment(1);
        } else {
            factory_->decrement(1);
        }
        if (!(repeatDelay_ > 0)) {
            nextSpin_ = now + 1e-6;
            break;
        }
        nextSpin_ += repeatDelay_;
        ++guard;
    }
}

double Spinner::arrowBreadth() const {
    switch (arrowLayout()) {
        case ArrowLayout::SplitVertical:
            return 0;
        case ArrowLayout::RightHorizontal:
        case ArrowLayout::LeftHorizontal:
        case ArrowLayout::SplitHorizontal:
            return kArrow * 2;
        case ArrowLayout::RightVertical:
        case ArrowLayout::LeftVertical:
            return kArrow;
    }
    return kArrow;
}

void Spinner::layoutChildren() {
    const double width = getWidth();
    const double height = getHeight();
    const ArrowLayout layout = arrowLayout();
    if (editor_ == nullptr || incrementArrow_ == nullptr || decrementArrow_ == nullptr) {
        return;
    }
    const double insetLeft = contentLeft();
    const double insetRight = computedStyle().padding.right + computedStyle().border.right;
    if (layout == ArrowLayout::SplitVertical) {
        const double band = std::min(kSplitArrow, std::max(0.0, height) * 0.5);
        const double editorX = std::min(insetLeft, width);
        incrementArrow_->performLayout(0, 0, width, band);
        editor_->performLayout(editorX, band, std::max(0.0, width - insetRight - editorX), std::max(0.0, height - band * 2));
        decrementArrow_->performLayout(0, height - band, width, band);
    } else if (layout == ArrowLayout::SplitHorizontal) {
        const double band = std::min(kArrow, std::max(0.0, width) * 0.5);
        const double editorX = std::max(band, insetLeft);
        const double editorRight = std::min(width - band, width - insetRight);
        decrementArrow_->performLayout(0, 0, band, height);
        editor_->performLayout(editorX, 0, std::max(0.0, editorRight - editorX), height);
        incrementArrow_->performLayout(width - band, 0, band, height);
    } else if (layout == ArrowLayout::RightHorizontal || layout == ArrowLayout::LeftHorizontal) {
        const double total = std::min(kArrow * 2, std::max(0.0, width));
        const double one = total * 0.5;
        const bool onRight = layout == ArrowLayout::RightHorizontal;
        const double buttonsX = onRight ? width - total : 0;
        const double editorX = onRight ? std::min(insetLeft, width - total) : total;
        const double editorRight = onRight ? width - total : std::min(width - insetRight, width);
        editor_->performLayout(editorX, 0, std::max(0.0, editorRight - editorX), height);
        decrementArrow_->performLayout(buttonsX, 0, one, height);
        incrementArrow_->performLayout(buttonsX + one, 0, one, height);
    } else {
        const double band = std::min(kArrow, std::max(0.0, width));
        const bool onRight = layout != ArrowLayout::LeftVertical;
        const double buttonsX = onRight ? width - band : 0;
        const double editorX = onRight ? std::min(insetLeft, width - band) : band;
        const double editorRight = onRight ? width - band : std::min(width - insetRight, width);
        const double top = std::floor(height * 0.5);
        editor_->performLayout(editorX, 0, std::max(0.0, editorRight - editorX), height);
        incrementArrow_->performLayout(buttonsX, 0, band, top);
        decrementArrow_->performLayout(buttonsX, top, band, height - top);
    }

    const bool editorFocused = editor_->isFocused();
    if (editorHadFocus_ && !editorFocused) {
        commitValue();
    }
    editorHadFocus_ = editorFocused;
    advanceSpin();
}

void Spinner::render(UiRenderer& renderer, float opacity) {
    Node::render(renderer, isDisabled() ? opacity * 0.45f : opacity);
}

void Spinner::renderContent(UiRenderer& renderer, float opacity) {
    const float x = static_cast<float>(getAbsoluteX());
    const float y = static_cast<float>(getAbsoluteY());
    const float width = static_cast<float>(getWidth());
    const float height = static_cast<float>(getHeight());
    if (width <= 0.f || height <= 0.f) {
        return;
    }
    const float radii[4] = {kCorner, kCorner, kCorner, kCorner};
    const ComputedStyle& style = computedStyle();
    const bool cssBorder = style.borderStyle == BorderStyle::Solid &&
                           (style.border.top > 0 || style.border.right > 0 || style.border.bottom > 0 || style.border.left > 0);
    if (!cssBorder) {
        const float sides[4] = {1.f, 1.f, 1.f, 1.f};
        Color line = Color::rgb8(218, 220, 224);
        line.a *= opacity;
        renderer.strokeRounded(x, y, width, height, radii, sides, line);
    }
    if (!isDisabled() && isFocusWithin()) {
        const float ring[4] = {2.f, 2.f, 2.f, 2.f};
        Color color = Color::rgb8(26, 115, 232);
        color.a *= opacity;
        renderer.strokeRounded(x, y, width, height, radii, ring, color);
    }
}

double Spinner::preferredContentWidth(double innerAvailable) const {
    const double editor = editor_ != nullptr ? editor_->measuredWidth(innerAvailable) : 0;
    return editor + arrowBreadth();
}

double Spinner::preferredContentHeight(double innerWidth) const {
    double height = editor_ != nullptr ? editor_->measuredHeight(innerWidth, -1) : 0;
    if (arrowLayout() == ArrowLayout::SplitVertical) {
        height += kSplitArrow * 2;
    }
    return height;
}

void Spinner::handleKey(KeyEvent& event) {
    if (!event.pressed || isDisabled() || !claimsArrowKey(event.key)) {
        return;
    }
    const bool up = event.key == Key::Up || event.key == Key::Right;
    if (up) {
        increment();
    } else {
        decrement();
    }
    event.consume();
}

void Spinner::sceneChanged(Scene*) {
    if (isTearingDown()) {
        return;
    }
    spinning_ = false;
    commitValue();
}

}  // namespace jadefx

#include "jadefx/scene/Controls/ComboBox.hpp"

#include "gl/UiRenderer.hpp"
#include "jadefx/scene/Scene.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace jadefx {
namespace {

constexpr double kRowHeight = 28.0;
constexpr double kArrowWidth = 28.0;
constexpr double kPreferredHeight = 32.0;
constexpr double kMinimumWidth = 120.0;
constexpr double kArrowGap = 36.0;
constexpr float kCorner = 4.f;

struct Raise {
    bool& flag;
    bool previous;
    explicit Raise(bool& flag) : flag(flag), previous(flag) { flag = true; }
    ~Raise() { flag = previous; }
    Raise(const Raise&) = delete;
    Raise& operator=(const Raise&) = delete;
};

Font FaceOf(const Node& node) {
    const ComputedStyle& style = node.computedStyle();
    const float size = style.fontSize > 0.f ? style.fontSize : 16.f;
    if (style.fontFamily.empty()) {
        return Font("Open Sans", size);
    }
    return Font(style.fontFamily, size);
}

int Step(int current, int delta, int count) {
    if (count <= 0 || delta == 0) {
        return current;
    }
    if (current < 0) {
        return delta > 0 ? 0 : count - 1;
    }
    const int next = current + (delta > 0 ? 1 : -1);
    if (next < 0 || next >= count) {
        return current;
    }
    return next;
}

void DrawLine(UiRenderer& renderer, float x, float y, float width, float height, const std::string& text,
              const ComputedStyle& style, Color color) {
    if (text.empty() || width <= 0.f || height <= 0.f || color.a <= 0.f) {
        return;
    }
    const Font face(style.fontFamily.empty() ? "Open Sans" : style.fontFamily, style.fontSize > 0.f ? style.fontSize : 16.f);
    const ShapedText shaped = face.shape(text);
    const float top = y + (height - shaped.height) * 0.5f;
    renderer.pushClip(x, y, width, height);
    renderer.text(x, top, text, face.family(), face.size(), color, style.subpixel);
    renderer.popClip();
}

}  // namespace

class ComboRow : public Controls {
public:
    ComboRow(ComboBox* combo, int index, std::string text) : combo_(combo), index_(index), text_(std::move(text)) {
        setDefaultCursor(Cursor::Pointer);
        setElementId(text_);
    }

    const char* getElementType() const override { return "combo-row"; }

    void unbind() { combo_ = nullptr; }

protected:
    void handleMousePressed(const MouseEvent&) override {
        if (combo_ != nullptr) {
            combo_->activateRow(index_);
        }
    }

    void renderContent(UiRenderer& renderer, float opacity) override {
        const float x = static_cast<float>(getAbsoluteX());
        const float y = static_cast<float>(getAbsoluteY());
        const float width = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());
        if (width <= 0.f || height <= 0.f) {
            return;
        }
        const bool armed = combo_ != nullptr && combo_->rowIsArmed(index_);
        if (armed || isHovered()) {
            Color fill = armed ? Color::rgb8(210, 227, 252) : Color::rgb8(232, 240, 254);
            fill.a *= opacity;
            const float radii[4] = {0.f, 0.f, 0.f, 0.f};
            const float at = 0.f;
            renderer.fillRounded(x, y, width, height, radii, &fill, &at, 1, 0.f);
        }
        Color color = computedStyle().color;
        color.a *= opacity;
        const float inset = 8.f;
        DrawLine(renderer, x + inset, y, std::max(0.f, width - inset * 2.f), height, text_, computedStyle(), color);
    }

private:
    ComboBox* combo_ = nullptr;
    int index_ = -1;
    std::string text_;
};

class ComboPopup : public Controls {
public:
    ComboPopup() {
        setBackground(Color::white());
        setStyle("border-radius: 4px;");
    }

    const char* getElementType() const override { return "combo-popup"; }

    void bind(ComboBox* combo) { combo_ = combo; }

    void unbind() {
        combo_ = nullptr;
        for (const std::shared_ptr<ComboRow>& row : rows_) {
            if (row) {
                row->unbind();
            }
        }
    }

    void rebuild() {
        children().clear();
        rows_.clear();
        if (combo_ == nullptr) {
            return;
        }
        const ObservableList<std::string>& items = combo_->getItems();
        rows_.reserve(items.size());
        for (std::size_t i = 0; i < items.size(); ++i) {
            rows_.push_back(std::make_shared<ComboRow>(combo_, static_cast<int>(i), items[i]));
            children().add(rows_.back());
        }
    }

protected:
    void layoutChildren() override {
        if (combo_ == nullptr) {
            return;
        }
        const double width = std::max(0.0, getWidth());
        const double scroll = combo_->scroll_;
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            if (!rows_[i]) {
                continue;
            }
            const double y = static_cast<double>(i) * kRowHeight - scroll;
            rows_[i]->performLayout(0.0, y, width, kRowHeight);
        }
    }

    void renderChildren(UiRenderer& renderer, float opacity) override {
        const float x = static_cast<float>(getAbsoluteX());
        const float y = static_cast<float>(getAbsoluteY());
        const float width = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());
        const bool clip = width > 0.f && height > 0.f;
        if (clip) {
            renderer.pushClip(x, y, width, height);
        }
        Node::renderChildren(renderer, opacity);
        if (clip) {
            renderer.popClip();
        }
    }

    void renderContent(UiRenderer& renderer, float opacity) override {
        const float x = static_cast<float>(getAbsoluteX());
        const float y = static_cast<float>(getAbsoluteY());
        const float width = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());
        if (width <= 0.f || height <= 0.f) {
            return;
        }
        const float radii[4] = {kCorner, kCorner, kCorner, kCorner};
        const float sides[4] = {1.f, 1.f, 1.f, 1.f};
        Color line = Color::rgb8(218, 220, 224);
        line.a *= opacity;
        renderer.strokeRounded(x, y, width, height, radii, sides, line);
    }

    void handleScroll(ScrollEvent& event) override {
        if (combo_ == nullptr) {
            return;
        }
        const double delta = event.deltaY != 0.0 ? event.deltaY : event.deltaX;
        combo_->scrollBy(delta);
        event.consume();
    }

private:
    ComboBox* combo_ = nullptr;
    std::vector<std::shared_ptr<ComboRow>> rows_;
};

ComboBox::ComboBox() {
    setDefaultCursor(Cursor::Pointer);
    setBackground(Color::white());
    setPadding(Insets::axes(4, 8));
    setPrefHeight(kPreferredHeight);
    setStyle("border-radius: 4px;");
    items_.setIndexedAddCallback([this](std::string, std::size_t) { onItemsChanged(); });
    items_.setIndexedRemoveCallback([this](std::string, std::size_t) { onItemsChanged(); });
}

ComboBox::~ComboBox() {
    closing_ = true;
    commitSuppressed_ = true;
    items_.setIndexedAddCallback(nullptr);
    items_.setIndexedRemoveCallback(nullptr);
    if (editor_ != nullptr) {
        editor_->setOnAction(nullptr);
    }
    if (popup_ != nullptr) {
        popup_->unbind();
    }
    hide();
}

void ComboBox::setValue(std::string value) {
    value_ = std::move(value);
    selection_ = indexOf(value_);
    syncEditor();
}

void ComboBox::select(int index) {
    if (index < 0 || index >= static_cast<int>(items_.size())) {
        value_.clear();
        selection_ = -1;
    } else {
        value_ = items_[static_cast<std::size_t>(index)];
        selection_ = index;
    }
    syncEditor();
}

void ComboBox::setPromptText(std::string text) {
    prompt_ = std::move(text);
    if (editor_ != nullptr) {
        editor_->setPromptText(prompt_);
    }
}

void ComboBox::setVisibleRowCount(int rows) {
    visibleRowCount_ = std::max(1, rows);
    if (isShowing()) {
        revealHighlight();
        presentPopup();
    }
}

void ComboBox::setEditable(bool editable) {
    if (editable_ == editable) {
        if (editor_ != nullptr) {
            editor_->setEditable(true);
            editor_->setDisable(isDisable());
            syncEditor();
        }
        return;
    }
    editable_ = editable;
    if (!editable_) {
        if (editor_ != nullptr) {
            editor_->setOnAction(nullptr);
            detachChild(editor_.get());
            editor_.reset();
        }
        return;
    }
    editor_ = std::make_shared<TextField>(value_);
    editor_->setEditable(true);
    editor_->setPromptText(prompt_);
    editor_->setBackground(Color::transparent());
    editor_->setPadding(Insets::axes(0, 2));
    editor_->setDisable(isDisable());
    editor_->setOnAction([this](ActionEvent&) { commitEditor(); });
    children().add(editor_);
}

void ComboBox::setDisable(bool value) {
    Node::setDisable(value);
    if (editor_ != nullptr) {
        editor_->setDisable(value);
    }
    if (value) {
        hide();
    }
}

void ComboBox::show() {
    if (closing_ || isDisabled() || items_.empty() || getScene() == nullptr) {
        return;
    }
    ensurePopup();
    if (!isShowing()) {
        highlight_ = selection_ >= 0 ? selection_ : 0;
        scroll_ = 0.0;
    }
    revealHighlight();
    popupArmed_ = true;
    presentPopup();
}

void ComboBox::hide() {
    if (hiding_) {
        return;
    }
    if (Scene* scene = getScene(); scene != nullptr && scene->isTearingDown()) {
        popupArmed_ = false;
        return;
    }
    Raise guard(hiding_);
    const bool live = isShowing();
    if (!live && !popupArmed_) {
        commitSuppressed_ = false;
        return;
    }
    const bool commit = !commitSuppressed_ && !closing_;
    commitSuppressed_ = false;
    popupArmed_ = false;
    if (commit) {
        commitEditorIfDirty();
    }
    if (live) {
        if (Scene* scene = getScene()) {
            scene->hidePopup(popup_.get());
        }
    }
}

bool ComboBox::isShowing() const {
    return popup_ != nullptr && getScene() != nullptr && getScene()->isPopupShowing(popup_.get());
}

void ComboBox::layoutChildren() {
    layoutEditor();
    // Escape and an outside press hide the popup in the scene, without calling hide().
    // The next layout is the first place the combo can commit the editor.
    if (popupArmed_ && !isShowing()) {
        hide();
    }
    if (popup_ != nullptr && isShowing()) {
        popup_->setPrefSize(popupWidth(), viewportHeight());
    }
}

void ComboBox::render(UiRenderer& renderer, float opacity) {
    Node::render(renderer, isDisabled() ? opacity * 0.45f : opacity);
}

void ComboBox::renderContent(UiRenderer& renderer, float opacity) {
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
        const float ringSides[4] = {2.f, 2.f, 2.f, 2.f};
        Color ring = Color::rgb8(26, 115, 232);
        ring.a *= opacity;
        renderer.strokeRounded(x, y, width, height, radii, ringSides, ring);
    }

    if (!editable_) {
        const bool prompt = value_.empty();
        const std::string& shown = prompt ? prompt_ : value_;
        Color color = style.color;
        if (prompt) {
            color.a *= 0.45f;
        }
        color.a *= opacity;
        const float textX = x + static_cast<float>(contentLeft());
        const float textW = std::max(0.f, width - static_cast<float>(kArrowWidth) - static_cast<float>(contentLeft()));
        DrawLine(renderer, textX, y, textW, height, shown, style, color);
    }

    Color mark = Color::rgb8(95, 99, 104);
    mark.a *= opacity;
    const float barWidth[3] = {9.f, 6.f, 3.f};
    const float barHeight = 2.f;
    const float gap = 2.f;
    const float block = barHeight * 3.f + gap * 2.f;
    float top = y + (height - block) * 0.5f;
    const float center = x + width - static_cast<float>(kArrowWidth) * 0.5f;
    const float at = 0.f;
    const float barRadius[4] = {1.f, 1.f, 1.f, 1.f};
    for (float bar : barWidth) {
        renderer.fillRounded(center - bar * 0.5f, top, bar, barHeight, barRadius, &mark, &at, 1, 0.f);
        top += barHeight + gap;
    }
}

double ComboBox::preferredContentWidth(double) const {
    const Font face = FaceOf(*this);
    double widest = static_cast<double>(face.measureWidth(prompt_));
    for (const std::string& item : items_.items()) {
        widest = std::max(widest, static_cast<double>(face.measureWidth(item)));
    }
    const double total = std::max(kMinimumWidth, widest + kArrowGap);
    return std::max(0.0, total - computedStyle().padding.width());
}

double ComboBox::preferredContentHeight(double) const {
    return std::max(0.0, kPreferredHeight - computedStyle().padding.height());
}

void ComboBox::handleMousePressed(const MouseEvent& event) {
    if (isDisabled()) {
        return;
    }
    const double localX = event.x - getAbsoluteX();
    if (editable_) {
        if (localX < getWidth() - kArrowWidth) {
            if (editor_ != nullptr) {
                editor_->requestFocus();
            }
            return;
        }
    }
    if (isShowing()) {
        hide();
    } else {
        show();
    }
    if (editable_ && editor_ != nullptr) {
        editor_->requestFocus();
    }
}

void ComboBox::handleKey(KeyEvent& event) {
    if (closing_ || !event.pressed || isDisabled()) {
        return;
    }
    const bool arrow = event.key == Key::Up || event.key == Key::Down;
    if (event.repeat && !arrow) {
        return;
    }
    const bool editorFocused = editor_ != nullptr && editor_->isFocused();
    const bool enter = event.key == Key::Enter || event.key == Key::KpEnter;
    const bool editorAlreadyCommitted = editorActionDuringKey_;
    if (editorFocused && enter) {
        if (!editorAlreadyCommitted) {
            commitEditor();
        }
        editorActionDuringKey_ = false;
        event.consume();
        return;
    }
    editorActionDuringKey_ = false;

    if (arrow) {
        const int delta = event.key == Key::Down ? 1 : -1;
        if (isShowing()) {
            moveHighlight(delta);
        } else {
            moveClosedSelection(delta);
        }
        event.consume();
        return;
    }
    if (enter && isShowing()) {
        const int count = static_cast<int>(items_.size());
        if (highlight_ >= 0 && highlight_ < count) {
            activateRow(highlight_);
        } else {
            hide();
        }
        event.consume();
        return;
    }
    if (event.key == Key::Space && !editable_ && !isShowing()) {
        show();
        event.consume();
    }
}

void ComboBox::sceneChanged(Scene* previous) {
    if (previous != nullptr && previous->isTearingDown()) {
        popupArmed_ = false;
        return;
    }
    if (previous != nullptr) {
        hide();
    }
}

void ComboBox::ensurePopup() {
    if (popup_ != nullptr) {
        return;
    }
    popup_ = std::make_shared<ComboPopup>();
    popup_->bind(this);
}

void ComboBox::presentPopup() {
    Scene* scene = getScene();
    if (scene == nullptr || popup_ == nullptr) {
        return;
    }
    popup_->setPrefSize(popupWidth(), viewportHeight());
    popup_->rebuild();
    PopupOptions options;
    options.owner = this;
    options.autoHide = true;
    popupArmed_ = true;
    scene->showPopupNear(popup_, this, Side::Bottom, options);
}

void ComboBox::onItemsChanged() {
    selection_ = indexOf(value_);
    const int count = static_cast<int>(items_.size());
    if (highlight_ >= count) {
        highlight_ = count > 0 ? count - 1 : -1;
    }
    clampScroll();
    if (!isShowing()) {
        return;
    }
    if (count == 0) {
        hide();
        return;
    }
    revealHighlight();
    presentPopup();
}

void ComboBox::syncEditor() {
    if (editor_ == nullptr) {
        return;
    }
    Raise guard(syncingEditor_);
    if (editor_->getPromptText() != prompt_) {
        editor_->setPromptText(prompt_);
    }
    if (editor_->getText() != value_) {
        editor_->setText(value_);
    }
}

void ComboBox::fire() {
    if (closing_ || !onAction_) {
        return;
    }
    ActionEvent event;
    event.source = this;
    onAction_(event);
}

void ComboBox::commitEditor() {
    if (closing_ || syncingEditor_ || committingEditor_ || !editable_ || editor_ == nullptr) {
        return;
    }
    Raise guard(committingEditor_);
    editorActionDuringKey_ = true;
    setValue(editor_->getText());
    commitSuppressed_ = true;
    fire();
    hide();
}

void ComboBox::commitEditorIfDirty() {
    if (closing_ || !editable_ || editor_ == nullptr || editor_->getText() == value_) {
        return;
    }
    setValue(editor_->getText());
    fire();
}

void ComboBox::activateRow(int index) {
    if (closing_ || isDisabled() || index < 0 || index >= static_cast<int>(items_.size())) {
        return;
    }
    highlight_ = index;
    select(index);
    commitSuppressed_ = true;
    fire();
    if (editable_ && editor_ != nullptr) {
        editor_->requestFocus();
    } else {
        requestFocus();
    }
    hide();
}

void ComboBox::moveClosedSelection(int delta) {
    const int count = static_cast<int>(items_.size());
    const int next = Step(selection_, delta, count);
    if (next < 0 || next == selection_) {
        return;
    }
    highlight_ = next;
    select(next);
    fire();
}

void ComboBox::moveHighlight(int delta) {
    const int count = static_cast<int>(items_.size());
    const int next = Step(highlight_, delta, count);
    if (next == highlight_) {
        return;
    }
    highlight_ = next;
    revealHighlight();
    relayoutPopup();
}

void ComboBox::scrollBy(double deltaY) {
    const double magnitude = std::fabs(deltaY);
    double pixels = deltaY;
    // A mouse notch is about ±1. Anything else is already a pixel distance.
    if (magnitude > 0.0 && std::fabs(magnitude - 1.0) <= 0.2) {
        pixels = std::copysign(kRowHeight, deltaY);
    }
    // Same direction as TreeView: a negative delta reveals later rows.
    scroll_ -= pixels;
    clampScroll();
    relayoutPopup();
}

void ComboBox::revealHighlight() {
    const int count = static_cast<int>(items_.size());
    if (highlight_ >= 0 && highlight_ < count) {
        const double top = static_cast<double>(highlight_) * kRowHeight;
        const double bottom = top + kRowHeight;
        const double view = viewportHeight();
        if (top < scroll_) {
            scroll_ = top;
        }
        if (bottom > scroll_ + view) {
            scroll_ = bottom - view;
        }
    }
    clampScroll();
}

void ComboBox::clampScroll() {
    const double maxScroll = std::max(0.0, static_cast<double>(items_.size()) * kRowHeight - viewportHeight());
    if (scroll_ < 0.0) {
        scroll_ = 0.0;
    }
    if (scroll_ > maxScroll) {
        scroll_ = maxScroll;
    }
}

void ComboBox::relayoutPopup() {
    if (popup_ == nullptr || !isShowing()) {
        return;
    }
    popup_->performLayout(popup_->getX(), popup_->getY(), popup_->getWidth(), popup_->getHeight());
}

void ComboBox::layoutEditor() {
    if (editor_ == nullptr) {
        return;
    }
    if (editor_->isDisable() != isDisable()) {
        editor_->setDisable(isDisable());
    }
    const double left = contentLeft();
    const double top = contentTop();
    const double right = std::min(left + contentWidth(), getWidth() - kArrowWidth);
    editor_->performLayout(left, top, std::max(0.0, right - left), std::max(0.0, contentHeight()));
}

int ComboBox::indexOf(const std::string& value) const {
    const std::vector<std::string>& list = items_.items();
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (list[i] == value) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

double ComboBox::viewportHeight() const {
    const int count = static_cast<int>(items_.size());
    if (count <= 0) {
        return 0.0;
    }
    return static_cast<double>(std::min(visibleRowCount_, count)) * kRowHeight;
}

double ComboBox::popupWidth() const {
    if (getWidth() > 0.0) {
        return getWidth();
    }
    return measuredWidth(100000.0);
}

bool ComboBox::rowIsArmed(int index) const { return index == highlight_ || index == selection_; }

}  // namespace jadefx

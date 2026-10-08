#include "jadefx/scene/controls/ComboBox.hpp"

#include "ControlChrome.hpp"
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
            const Color fill =
                chrome::Themed(*this, armed ? ThemeColor::SelectionHover : ThemeColor::Selection, opacity);
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
    ComboPopup() = default;

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
        chrome::DrawBorder(renderer, *this, opacity, kCorner);
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
    setPrefHeight(kPreferredHeight);
    items_.setIndexedAddCallback([this](std::string, std::size_t) { onItemsChanged(); });
    items_.setIndexedRemoveCallback([this](std::string, std::size_t) { onItemsChanged(); });
}

ComboBox::~ComboBox() {
    closing_ = true;
    commitSuppressed_ = true;
    items_.setIndexedAddCallback(nullptr);
    items_.setIndexedRemoveCallback(nullptr);
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

void ComboBox::setVisibleRowCount(int rows) {
    visibleRowCount_ = std::max(1, rows);
    if (isShowing()) {
        show();
    }
}

std::shared_ptr<Node> ComboBox::createPopupContent() {
    popup_ = std::make_shared<ComboPopup>();
    popup_->bind(this);
    return popup_;
}

bool ComboBox::canShowPopup() const { return !closing_ && !items_.empty(); }

void ComboBox::popupShowing() {
    if (!isShowing()) {
        highlight_ = selection_ >= 0 ? selection_ : 0;
        scroll_ = 0.0;
    }
    revealHighlight();
    popup_->setPrefSize(popupWidth(), viewportHeight());
    popup_->rebuild();
}

void ComboBox::popupHidden() {
    // Escape and an outside press close the list without a choice, so typed text is kept.
    if (!commitSuppressed_ && !closing_) {
        commitEditorIfDirty();
    }
}

void ComboBox::editorFocusLost() {
    // Typed text is committed when the focus leaves the combo, as in JavaFX, but not
    // when it only moves into the list.
    if (focusLeftControl()) {
        commitEditorIfDirty();
    }
}

void ComboBox::closeCommitted() {
    commitSuppressed_ = true;
    hide();
    commitSuppressed_ = false;
}

void ComboBox::layoutChildren() {
    ComboBoxBase::layoutChildren();
    if (popup_ != nullptr && isShowing()) {
        popup_->setPrefSize(popupWidth(), viewportHeight());
    }
}

double ComboBox::preferredContentWidth(double) const {
    const Font face = chrome::FontOf(*this);
    double widest = static_cast<double>(face.measureWidth(getPromptText()));
    for (const std::string& item : items_.items()) {
        widest = std::max(widest, static_cast<double>(face.measureWidth(item)));
    }
    const double total = std::max(kMinimumWidth, widest + kArrowGap);
    return std::max(0.0, total - computedStyle().padding.width());
}

void ComboBox::handleKey(KeyEvent& event) {
    if (closing_ || !event.pressed || isDisabled()) {
        return;
    }
    const bool arrow = event.key == Key::Up || event.key == Key::Down;
    if (event.repeat && !arrow) {
        return;
    }
    const bool editorFocused = getEditor() != nullptr && getEditor()->isFocused();
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

    if (arrow && !event.alt) {
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
    ComboBoxBase::handleKey(event);
}

void ComboBox::onItemsChanged() {
    // preferredContentWidth measures every item's text, even while the list is closed.
    markLayoutDirty();
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
    show();
}

void ComboBox::fire() {
    if (!closing_) {
        fireAction();
    }
}

void ComboBox::commitEditor() {
    TextField* editor = getEditor();
    if (closing_ || isSyncingEditor() || committingEditor_ || editor == nullptr) {
        return;
    }
    Raise guard(committingEditor_);
    editorActionDuringKey_ = true;
    setValue(editor->getText());
    fire();
    closeCommitted();
}

void ComboBox::commitEditorIfDirty() {
    TextField* editor = getEditor();
    if (closing_ || editor == nullptr || editor->getText() == value_) {
        return;
    }
    setValue(editor->getText());
    fire();
}

void ComboBox::activateRow(int index) {
    if (closing_ || isDisabled() || index < 0 || index >= static_cast<int>(items_.size())) {
        return;
    }
    highlight_ = index;
    select(index);
    fire();
    if (TextField* editor = getEditor()) {
        editor->requestFocus();
    } else {
        requestFocus();
    }
    closeCommitted();
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

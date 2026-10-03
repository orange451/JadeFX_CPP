#include "jadefx/scene/controls/ComboBoxBase.hpp"

#include "ControlChrome.hpp"
#include "gl/UiRenderer.hpp"
#include "jadefx/scene/Scene.hpp"

#include <algorithm>

namespace jadefx {
namespace {

constexpr double kPreferredHeight = 32.0;
constexpr float kCorner = 4.f;

}  // namespace

ComboBoxBase::ComboBoxBase() {
    setDefaultCursor(Cursor::Pointer);
    setPadding(Insets::axes(4, 8));
}

ComboBoxBase::~ComboBoxBase() { releaseKeyHook(); }

void ComboBoxBase::show() {
    Scene* scene = getScene();
    if (scene == nullptr || isDisabled() || !canShowPopup()) {
        return;
    }
    if (popup_ == nullptr) {
        popup_ = createPopupContent();
        if (popup_ == nullptr) {
            return;
        }
    }
    const bool opening = !isShowing();
    if (opening && onShowing_) {
        onShowing_();
    }
    popupShowing();
    open_ = true;
    if (opening) {
        releaseKeyHook();
        hookScene_ = scene;
        keyHook_ = scene->addKeyHook([this](KeyEvent& event) {
            if (isShowing() && handlePopupKey(event)) {
                event.consume();
            }
        });
    }
    PopupOptions options;
    options.owner = this;
    options.autoHide = true;
    scene->showPopupNear(popup_, this, Side::Bottom, options);
}

void ComboBoxBase::hide() {
    if (hiding_ || !open_) {
        return;
    }
    hiding_ = true;
    releaseKeyHook();
    Scene* scene = getScene();
    if (popup_ != nullptr && scene != nullptr && !scene->isTearingDown() && scene->isPopupShowing(popup_.get())) {
        scene->hidePopup(popup_.get());
    }
    open_ = false;
    if (scene == nullptr || !scene->isTearingDown()) {
        popupHidden();
        if (onHidden_) {
            onHidden_();
        }
    }
    hiding_ = false;
}

bool ComboBoxBase::isShowing() const {
    return popup_ != nullptr && getScene() != nullptr && getScene()->isPopupShowing(popup_.get());
}

void ComboBoxBase::releaseKeyHook() {
    if (hookScene_ != nullptr && keyHook_ != 0 && !hookScene_->isTearingDown()) {
        hookScene_->removeKeyHook(keyHook_);
    }
    hookScene_ = nullptr;
    keyHook_ = 0;
}

void ComboBoxBase::setEditable(bool editable) {
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
    editor_ = std::make_shared<TextField>();
    editor_->setEditable(true);
    editor_->setBackground(Color::transparent());
    editor_->setPadding(Insets::axes(0, 2));
    // This sheet is the editor's, so it follows ancestor rules. A shared textfield
    // padding or border cannot shrink the line below the box.
    editor_->setStylesheet("textfield { padding: 0 2px; border-width: 0; background-color: transparent; }");
    editor_->setDisable(isDisable());
    editor_->setOnAction([this](ActionEvent&) {
        if (!syncingEditor_) {
            editorAction();
        }
    });
    editor_->setOnFocusChanged([this](bool focused) {
        if (!focused && !syncingEditor_) {
            editorFocusLost();
        }
    });
    children().add(editor_);
    syncEditor();
}

void ComboBoxBase::setPromptText(std::string text) {
    prompt_ = std::move(text);
    if (editor_ != nullptr) {
        editor_->setPromptText(prompt_);
    }
}

void ComboBoxBase::syncEditor() {
    if (editor_ == nullptr) {
        return;
    }
    const bool previous = syncingEditor_;
    syncingEditor_ = true;
    if (editor_->getPromptText() != prompt_) {
        editor_->setPromptText(prompt_);
    }
    const std::string text = valueText();
    if (editor_->getText() != text) {
        editor_->setText(text);
    }
    syncingEditor_ = previous;
}

void ComboBoxBase::setDisable(bool value) {
    if (editor_ != nullptr) {
        editor_->setDisable(value);
    }
    Node::setDisable(value);
    if (value) {
        hide();
    }
}

void ComboBoxBase::fireAction() {
    if (!onAction_) {
        return;
    }
    ActionEvent event;
    event.source = this;
    onAction_(event);
}

void ComboBoxBase::layoutChildren() {
    if (editor_ != nullptr) {
        if (editor_->isDisable() != isDisable()) {
            editor_->setDisable(isDisable());
        }
        const double left = contentLeft();
        const double right = std::min(left + contentWidth(), getWidth() - kArrowWidth);
        // The text box is the box's full height. Width still stops at the arrow.
        editor_->performLayout(left, 0.0, std::max(0.0, right - left), std::max(0.0, getHeight()));
    }
    // Escape, an outside press, and a window losing the focus close the popup in the
    // scene, without calling hide(). The next layout is where the control learns of it,
    // and of focus that moved on to another control.
    if (open_ && (!isShowing() || focusLeftControl())) {
        hide();
    }
}

bool ComboBoxBase::focusLeftControl() const {
    const Scene* scene = getScene();
    if (scene == nullptr || !scene->isWindowFocused()) {
        return true;
    }
    // A press on a part that takes no focus, such as a swatch or a day, leaves none.
    const Node* owner = scene->focusedNode();
    if (owner == nullptr) {
        return false;
    }
    const bool inside = owner == this || isAncestorOf(owner);
    const bool inPopup = popup_ != nullptr && (owner == popup_.get() || popup_->isAncestorOf(owner));
    return !inside && !inPopup;
}

void ComboBoxBase::render(UiRenderer& renderer, float opacity) {
    Node::render(renderer, isDisabled() ? opacity * 0.45f : opacity);
}

void ComboBoxBase::renderContent(UiRenderer& renderer, float opacity) {
    const float x = static_cast<float>(getAbsoluteX());
    const float y = static_cast<float>(getAbsoluteY());
    const float width = static_cast<float>(getWidth());
    const float height = static_cast<float>(getHeight());
    if (width <= 0.f || height <= 0.f) {
        return;
    }
    chrome::DrawBorder(renderer, *this, opacity, kCorner);
    if (!isDisabled() && isHovered() && !editable_) {
        chrome::DrawWash(renderer, *this, opacity, isPressed() || isShowing(), kCorner);
    }
    if (!isDisabled() && isFocusWithin()) {
        chrome::DrawFocusRing(renderer, *this, opacity, kCorner);
    }
    const float left = x + static_cast<float>(contentLeft());
    const float top = y + static_cast<float>(contentTop());
    const float valueWidth = width - static_cast<float>(kArrowWidth) - static_cast<float>(contentLeft());
    renderValue(renderer, opacity, left, top, std::max(0.f, valueWidth), static_cast<float>(contentHeight()));
    const float arrowX = x + width - static_cast<float>(kArrowWidth) * 0.5f;
    chrome::DrawArrowHead(renderer, arrowX, y + height * 0.5f, Side::Bottom,
                          chrome::Themed(*this, ThemeColor::Muted, opacity));
}

void ComboBoxBase::renderValue(UiRenderer& renderer, float opacity, float x, float y, float width, float height) {
    if (editable_ || width <= 0.f || height <= 0.f) {
        return;
    }
    const std::string value = valueText();
    const bool prompt = value.empty();
    const std::string& text = prompt ? prompt_ : value;
    if (text.empty()) {
        return;
    }
    const ComputedStyle& style = computedStyle();
    Color color = style.color;
    color.a *= prompt ? opacity * 0.45f : opacity;
    const Font font = chrome::FontOf(*this);
    renderer.pushClip(x, y, width, height);
    renderer.text(x, y + (height - font.shape(text).height) * 0.5f, text, font.family(), font.size(), color,
                  style.subpixel);
    renderer.popClip();
}

double ComboBoxBase::preferredContentHeight(double) const {
    return std::max(0.0, kPreferredHeight - computedStyle().padding.height());
}

void ComboBoxBase::handleMousePressed(const MouseEvent& event) {
    if (isDisabled() || event.button != 0) {
        return;
    }
    // In an editable box only the arrow opens the popup; the rest is the field.
    if (editable_ && editor_ != nullptr && event.x - getAbsoluteX() < getWidth() - kArrowWidth) {
        editor_->requestFocus();
        return;
    }
    if (isShowing()) {
        hide();
    } else {
        show();
    }
    if (editable_ && editor_ != nullptr) {
        editor_->requestFocus();
    } else {
        requestFocus();
    }
}

void ComboBoxBase::handleKey(KeyEvent& event) {
    if (!event.pressed || event.repeat || isDisabled() || isShowing()) {
        return;
    }
    // Space is a character in an editable box's field.
    const bool space = event.key == Key::Space && !editable_;
    if (space || event.key == Key::F4 || (event.alt && (event.key == Key::Down || event.key == Key::Up))) {
        show();
        event.consume();
    }
}

void ComboBoxBase::filterKey(KeyEvent& event) {
    TextField* editor = getEditor();
    if (editor != nullptr && editor->isFocused() && (event.key == Key::Up || event.key == Key::Down)) {
        handleKey(event);
    }
}

void ComboBoxBase::sceneChanged(Scene* previous) {
    // A scene being torn down takes its popups with it, and nothing is left to tell.
    if (previous != nullptr && previous->isTearingDown()) {
        open_ = false;
        hookScene_ = nullptr;
        keyHook_ = 0;
        return;
    }
    if (previous != nullptr) {
        hide();
    }
}

}  // namespace jadefx

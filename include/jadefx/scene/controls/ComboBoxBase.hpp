#pragma once

#include "jadefx/scene/controls/Controls.hpp"
#include "jadefx/scene/controls/TextField.hpp"

#include <functional>
#include <memory>
#include <string>

namespace jadefx {

// A box with an arrow that opens a popup under itself, in the shape of OpenJFX's
// ComboBoxBase. A subclass supplies the popup's content and the value's text;
// the base shows and hides the popup, draws the border, focus ring, arrow, and
// value, keeps the optional text editor, and handles the keys.
// A press on the box toggles the popup. Space, F4, and Alt+Down or Alt+Up open
// it from the keyboard, as in JavaFX. Escape or a press outside closes it, as
// does hide(). An editable box has a TextField left of the arrow; a press there
// types instead of opening. ComboBox, ColorPicker, and DatePicker are built on it.
class ComboBoxBase : public Controls {
public:
    ~ComboBoxBase() override;

    // Opens the popup, or brings an open one up to date with the control.
    void show();
    void hide();
    bool isShowing() const;

    void setOnAction(ActionHandler handler) { onAction_ = std::move(handler); }
    // Before the popup appears, and after it is gone, however it closed.
    void setOnShowing(std::function<void()> handler) { onShowing_ = std::move(handler); }
    void setOnHidden(std::function<void()> handler) { onHidden_ = std::move(handler); }

    void setEditable(bool editable);
    bool isEditable() const { return editable_; }
    // Non-null only while the box is editable. The field is a child of the box and
    // its text box fills the box's height.
    TextField* getEditor() const { return editable_ ? editor_.get() : nullptr; }
    // Shown, faded, while the value's text is empty.
    void setPromptText(std::string text);
    const std::string& getPromptText() const { return prompt_; }

    // Also disables the editor and hides the popup. Node::setDisable is not virtual;
    // call this on the control.
    void setDisable(bool value);

protected:
    ComboBoxBase();

    // The popup's content, made the first time the popup opens.
    virtual std::shared_ptr<Node> createPopupContent() = 0;
    // False keeps the popup closed, as for a combo box with no items.
    virtual bool canShowPopup() const { return true; }
    // Runs before the popup appears, and again when show() is called while it is up,
    // so its content can show the current value. isShowing() tells the two apart.
    virtual void popupShowing() {}
    // Runs after the popup is gone.
    virtual void popupHidden() {}
    // A key while the popup is up, before the focused node sees it. True consumes it.
    virtual bool handlePopupKey(KeyEvent&) { return false; }

    // The value as text, for the box and the editor.
    virtual std::string valueText() const { return {}; }
    // Enter in the editor.
    virtual void editorAction() {}
    // The editor lost the focus.
    virtual void editorFocusLost() {}
    // Puts valueText and the prompt in the editor.
    void syncEditor();
    // True while syncEditor writes the editor, so its handlers can ignore that.
    bool isSyncingEditor() const { return syncingEditor_; }

    // Draws the value in the box, left of the arrow; the rectangle is in window points.
    // The default draws valueText, or the prompt faded, and nothing while editable.
    virtual void renderValue(UiRenderer& renderer, float opacity, float x, float y, float width, float height);
    // The popup's content, or null before it first opens.
    Node* popupContent() const { return popup_.get(); }
    void fireAction();

    void layoutChildren() override;
    void render(UiRenderer& renderer, float opacity) override;
    void renderContent(UiRenderer& renderer, float opacity) override;
    double preferredContentHeight(double innerWidth) const override;
    void handleMousePressed(const MouseEvent& event) override;
    void handleKey(KeyEvent& event) override;
    void sceneChanged(Scene* previous) override;

    static constexpr double kArrowWidth = 28.0;

private:
    void releaseKeyHook();

    std::shared_ptr<Node> popup_;
    std::shared_ptr<TextField> editor_;
    std::string prompt_;
    ActionHandler onAction_;
    std::function<void()> onShowing_;
    std::function<void()> onHidden_;
    Scene* hookScene_ = nullptr;
    int keyHook_ = 0;
    // True from show until the base has seen the popup close.
    bool open_ = false;
    bool hiding_ = false;
    bool editable_ = false;
    bool syncingEditor_ = false;
};

}  // namespace jadefx

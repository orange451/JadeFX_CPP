#pragma once

#include "jadefx/scene/controls/Controls.hpp"

#include <functional>
#include <memory>

namespace jadefx {

// A box with an arrow that opens a popup under itself, in the shape of OpenJFX's
// ComboBoxBase. A subclass supplies the popup's content and draws its value in
// the box; the base shows and hides the popup, draws the border, focus ring,
// and arrow, and handles the keys.
// A press on the box toggles the popup. Space, F4, and Alt+Down or Alt+Up open
// it from the keyboard, as in JavaFX. Escape or a press outside closes it, as
// does hide(). ComboBox, ColorPicker, and DatePicker are built on it.
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

    // Also hides the popup. Node::setDisable is not virtual; call this on the control.
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
    // Draws the value in the box, left of the arrow. The rectangle is in window points.
    virtual void renderValue(UiRenderer& renderer, float opacity, float x, float y, float width, float height) = 0;
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
    std::shared_ptr<Node> popup_;
    ActionHandler onAction_;
    std::function<void()> onShowing_;
    std::function<void()> onHidden_;
    // True from show until the base has seen the popup close.
    bool open_ = false;
    bool hiding_ = false;
};

}  // namespace jadefx

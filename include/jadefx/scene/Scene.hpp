#pragma once

#include "jadefx/scene/Node.hpp"

#include <functional>
#include <memory>
#include <string>

namespace jadefx {

class StackPane;

// The root of one window. Layout is in CSS pixels (window points), origin top left.
class Scene : public Node {
public:
    explicit Scene(std::shared_ptr<Node> root);
    Scene(std::shared_ptr<Node> root, double prefWidth, double prefHeight);
    ~Scene() override;

    const char* getElementType() const override { return "scene"; }

    void setRoot(std::shared_ptr<Node> root);
    Node* getRoot() const { return root_.get(); }

    double requestedWidth() const { return requestedWidth_; }
    double requestedHeight() const { return requestedHeight_; }

    // Lay out into a window of this size in points.
    void layout(double width, double height);
    void layout(double width, double height, double timeSeconds);

    void setSafeInsets(const Insets& insets) { safe_ = insets; }

    void noteMove(double x, double y);
    void noteButton(int button, bool down, double x, double y);
    void noteScroll(double x, double y, double deltaX, double deltaY);
    // Returns true when the focused node consumed the event.
    bool noteKey(int key, bool pressed, bool repeat, int mods);
    bool noteText(const std::string& text);
    // Shift, control, alt, and super bits from the latest key event. See Key::ModShift.
    int modifierMask() const { return keyMods_; }

    void requestFocus(Node* node);
    Node* focusedNode() const { return focused_; }
    // Drop focus when it is this node or one of its descendants.
    void releaseFocus(Node* node);

    void setClipboardText(std::string text);
    std::string clipboardText() const;
    void setClipboardBridge(std::function<void(const std::string&)> setText, std::function<std::string()> getText);

protected:
    Scene* asScene() override { return this; }

private:
    std::shared_ptr<StackPane> internal_;
    std::shared_ptr<Node> root_;
    double requestedWidth_ = 0;
    double requestedHeight_ = 0;
    double lastWidth_ = 0;
    double lastHeight_ = 0;
    double lastTime_ = 0;
    Insets safe_;
    Node* pressedTarget_ = nullptr;
    Node* focused_ = nullptr;
    int keyMods_ = 0;
    bool laidOut_ = false;
    std::string clipboard_;
    std::function<void(const std::string&)> clipboardSet_;
    std::function<std::string()> clipboardGet_;
};

}  // namespace jadefx

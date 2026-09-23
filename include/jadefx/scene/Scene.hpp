#pragma once

#include "jadefx/scene/Node.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace jadefx {

class StackPane;

// How a popup sits above the scene. owner keeps it open while the pointer is on that
// node. autoHide closes it on an outside press. hideOnPress closes it on any press.
// modal is not dismissed by Escape. fillScene stretches it over the window.
struct PopupOptions {
    Node* owner = nullptr;
    bool autoHide = true;
    bool hideOnPress = false;
    bool modal = false;
    bool fillScene = false;
};

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

    // Seconds passed to the latest layout. Hover popups and carets read this.
    double timeSeconds() const { return lastTime_; }

    // An overlay drawn above the root and hit-tested first. width or height below 0
    // sizes the popup from its preferred size. A popup opened from inside another
    // stays up with that parent.
    void showPopup(std::shared_ptr<Node> popup, double x, double y, double width, double height,
                   PopupOptions options = {});
    // Measures the popup, sits it on the anchor's edge, and flips toward the scene if it clips.
    void showPopupNear(std::shared_ptr<Node> popup, Node* anchor, Side side, PopupOptions options = {});
    void movePopup(Node* popup, double x, double y, double width, double height);
    void hidePopup(Node* popup);
    void hidePopupsOwnedBy(Node* owner);
    bool isPopupShowing(const Node* popup) const;

    // Runs before the focused node sees the key. Menus use this for accelerators.
    // The returned id is passed to removeKeyHook.
    int addKeyHook(std::function<void(KeyEvent&)> hook);
    void removeKeyHook(int id);

    // One turn of the window loop. 1 advanced, 0 means a frame is already running
    // (do not nest), -1 means there is no pump or the window is closing.
    // Application::launch installs the pump. Alert::showAndWait uses it.
    void setEventPump(std::function<int()> pump);
    int runEventPump();

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

    struct PopupRecord {
        std::shared_ptr<Node> node;
        Node* owner = nullptr;
        double x = 0;
        double y = 0;
        double width = 0;
        double height = 0;
        bool autoHide = true;
        bool hideOnPress = false;
        bool modal = false;
        bool fillScene = false;
        bool measure = false;
    };

    struct HookRecord {
        int id = 0;
        std::function<void(KeyEvent&)> hook;
    };

    struct HoverState {
        Node* owner = nullptr;
        HoverPopup spec{};
        double enteredAt = 0;
        double hideAt = -1;
        double shownAt = 0;
        Node* shownOwner = nullptr;
        bool showing = false;
        bool armed = true;
    };

    void layoutPopup(PopupRecord& popup);
    void updateHoverPopup(Node* hit);
    bool popupStays(const PopupRecord& popup, Node* hit, int depth) const;
    PopupRecord* findPopup(const Node* popup);

    std::vector<PopupRecord> popups_;
    std::vector<HookRecord> keyHooks_;
    int nextHookId_ = 1;
    HoverState hover_;
    double pointerX_ = 0;
    double pointerY_ = 0;
    bool pointerValid_ = false;
    std::function<int()> eventPump_;
};

}  // namespace jadefx

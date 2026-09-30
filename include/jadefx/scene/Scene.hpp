#pragma once

#include "jadefx/scene/Node.hpp"

#include <functional>
#include <memory>
#include <string>
#include <utility>
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
    explicit Scene(std::shared_ptr<Node> root = nullptr);
    Scene(std::shared_ptr<Node> root, double prefWidth, double prefHeight);
    ~Scene() override;

    const char* getElementType() const override { return "scene"; }

    // The lowest layer of this scene's cascade: light, dark, or CSS text, as
    // Theme::setUserAgentStylesheet takes. Empty uses the application's.
    // Switching it restyles the scene at the next layout.
    void setUserAgentStylesheet(std::string cssOrTheme);
    const std::string& getUserAgentStylesheet() const { return userAgentSource_; }
    // The user-agent stylesheet this scene's nodes are styled with.
    const Stylesheet& userAgentStylesheet() const;

    void setRoot(std::shared_ptr<Node> root);
    Node* getRoot() const { return root_.get(); }

    double requestedWidth() const { return requestedWidth_; }
    double requestedHeight() const { return requestedHeight_; }

    // Lay out into a window of this size in points.
    void layout(double width, double height);
    void layout(double width, double height, double timeSeconds);

    void setSafeInsets(const Insets& insets) { safe_ = insets; }

    void noteMove(double x, double y);
    // The pointer left the window. Hover ends and the cursor returns to the arrow.
    void notePointerExit();
    // Cursor for the pointer's last position. The arrow when the pointer is outside.
    Cursor hoverCursor();
    // mods are the Key::Mod bits held with the button. The left button presses,
    // drags, and clicks. A right press asks for a context menu. A node that
    // receives all buttons also hears right and middle presses and releases.
    void noteButton(int button, bool down, double x, double y, int mods = 0);
    void noteScroll(double x, double y, double deltaX, double deltaY);
    // Returns true when the focused node consumed the event.
    bool noteKey(int key, bool pressed, bool repeat, int mods);
    bool noteText(const std::string& text);
    // Shift, control, alt, and super bits from the latest key event. See Key::ModShift.
    int modifierMask() const { return keyMods_; }
    // The window gained or lost the system's keyboard focus, as JavaFX's Window.focused.
    // While it is away the focus owner keeps its place but is not focused: it sees
    // handleFocusLost and a focus change to false, and the reverse when the window
    // comes back. Held modifier keys are forgotten, since they are released in
    // another window, and popups that hide on an outside press close.
    void noteWindowFocus(bool focused);
    bool isWindowFocused() const { return windowFocused_; }
    // Pointer lock, for a view that turns a camera by the mouse's motion. While
    // locked the host hides the system pointer and holds it where it was, moves
    // are not delivered, and the motion adds up for takePointerDelta. Stage
    // passes the lock to its host through the bridge; with no bridge, as in a
    // test, the lock is only recorded. Losing the window's focus ends the lock.
    void setPointerLocked(bool locked);
    bool isPointerLocked() const { return pointerLocked_; }
    // The motion since the last take, in window points. Zero while unlocked.
    void takePointerDelta(double& dx, double& dy);
    // The host's side: Stage sets the bridge and forwards the motion.
    void setPointerLockBridge(std::function<void(bool)> bridge);
    void notePointerDelta(double dx, double dy);
    // Files dropped on the window from the system, at a point in window points. The node
    // there sees them as a drag with no source: entered, over, then dropped if it
    // accepts, and exited. Returns true when a node completed the drop.
    bool noteFileDrop(double x, double y, std::vector<std::string> paths);
    // True while a drag started with startDragAndDrop is under way. Escape cancels it.
    bool isDragging() const { return drag_ != nullptr; }

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

    // Runs after every layout, popups included, as JavaFX's post-layout pulse
    // listeners do. A listener may move, show, or hide popups, and add or remove
    // listeners; those take effect for the next pulse. The id removes it.
    int addPostLayoutPulseListener(std::function<void()> listener);
    void removePostLayoutPulseListener(int id);

    // One turn of the window loop. 1 advanced, 0 means a frame is already running
    // (do not nest), -1 means there is no pump or the window is closing.
    // Application::launch installs the pump. Alert::showAndWait uses it.
    void setEventPump(std::function<int()> pump);
    int runEventPump();

protected:
    Scene* asScene() override { return this; }

private:
    friend class Node;

    // A drag under way: its data and the nodes it has met.
    struct DragState {
        Dragboard board;
        Node* source = nullptr;
        // The node under the pointer and its ancestors, which have heard entered.
        std::vector<Node*> entered;
        // The node that accepted the latest over, and the mode it took.
        Node* acceptor = nullptr;
        TransferMode mode = TransferMode::Move;
        std::shared_ptr<Node> view;
        // Set while finishDrag tells the nodes, so nothing moves the drag meanwhile.
        bool ending = false;
    };

    // Focuses next, or nothing when it is null, and tells the node that had focus.
    void moveFocus(Node* next);
    // A node is leaving this scene, so nothing here may point at it any more.
    void forgetNode(Node* node);
    // A right or middle button. It reaches the nearest node over the pointer
    // that receives all buttons, and its release reaches that same node.
    void noteOtherButton(int button, bool down, double x, double y, int mods);
    // Tells each node holding a right or middle button that it was released.
    void releaseHeldButtons();
    // A press became a drag: runs drag-detected from the pressed node up.
    void detectDrag(const MouseEvent& event);
    Dragboard* beginDrag(Node* source, TransferModes modes);
    // Moves the drag to a point: entered and exited, over, the cursor, and the view.
    void updateDrag(DragState& drag, double x, double y, Node* source);
    // Ends the drag, dropping it where it is when drop is set, and tells the source.
    // Returns true when the drop completed.
    bool finishDrag(double x, double y, bool drop);
    DragEvent makeDragEvent(DragState& drag, double x, double y, Node* source) const;
    std::shared_ptr<StackPane> internal_;
    std::shared_ptr<Node> root_;
    double requestedWidth_ = 0;
    double requestedHeight_ = 0;
    double lastWidth_ = 0;
    double lastHeight_ = 0;
    double lastTime_ = 0;
    Insets safe_;
    std::string userAgentSource_;
    Stylesheet userAgent_;
    Node* pressedTarget_ = nullptr;
    // The node that heard each right or middle press, by GLFW button, until its release.
    static constexpr int kHeldButtons = 3;
    Node* heldButtonTargets_[kHeldButtons] = {};
    std::unique_ptr<DragState> drag_;
    // The node whose drag-detected handlers are running, which may start a drag.
    Node* detecting_ = nullptr;
    double pressX_ = 0;
    double pressY_ = 0;
    bool stillSincePress_ = true;
    // The run of left presses that clickCount counts.
    int clickCount_ = 0;
    double lastClickSeconds_ = -1;
    double lastClickX_ = 0;
    double lastClickY_ = 0;
    Node* focused_ = nullptr;
    int keyMods_ = 0;
    bool windowFocused_ = true;
    bool laidOut_ = false;
    std::string clipboard_;
    std::function<void(const std::string&)> clipboardSet_;
    std::function<std::string()> clipboardGet_;
    std::function<void(bool)> pointerLockBridge_;
    bool pointerLocked_ = false;
    double pointerDeltaX_ = 0;
    double pointerDeltaY_ = 0;

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
    std::vector<std::pair<int, std::function<void()>>> pulseListeners_;
    int nextHookId_ = 1;
    HoverState hover_;
    double pointerX_ = 0;
    double pointerY_ = 0;
    bool pointerValid_ = false;
    std::function<int()> eventPump_;
};

}  // namespace jadefx

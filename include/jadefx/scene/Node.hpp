#pragma once

#include "jadefx/collections/ObservableList.hpp"
#include "jadefx/event/DragEvent.hpp"
#include "jadefx/event/Events.hpp"
#include "jadefx/geometry/Geometry.hpp"
#include "jadefx/paint/Color.hpp"
#include "jadefx/scene/text/Font.hpp"
#include "jadefx/style/Style.hpp"
#include "jadefx/style/Theme.hpp"

#include <any>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace jadefx {

class Node;
class Scene;
class SubScene;
class UiRenderer;

// Shown beside a node after the pointer rests on it. Scene owns the timing.
// content is the popup. Delays and showDuration are seconds. 0 shows or hides immediately.
// A positive showDuration dismisses the popup even if the pointer stays.
struct HoverPopup {
    std::shared_ptr<Node> content;
    double showDelay = 1;
    double hideDelay = 0.2;
    double showDuration = 5;
};

// One element in the scene graph. Own nodes with std::shared_ptr (see make<T>()).
// The parent list holds a shared_ptr, and the child keeps a raw parent pointer.
class Node {
public:
    Node();
    virtual ~Node();

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    virtual const char* getElementType() const = 0;

    double getX() const { return x_; }
    double getY() const { return y_; }
    double getWidth() const { return width_; }
    double getHeight() const { return height_; }
    double getAbsoluteX() const;
    double getAbsoluteY() const;

    void setPrefSize(double width, double height);
    void setPrefWidth(double width);
    void setPrefHeight(double height);
    void setPrefWidthRatio(double ratio);
    void setPrefHeightRatio(double ratio);
    void setMinSize(double width, double height);
    void setMaxSize(double width, double height);
    double getPrefWidth() const;
    double getPrefHeight() const;
    // Pixel minimum from setMinSize. Zero when no pixel minimum is set.
    double getMinWidth() const;
    double getMinHeight() const;

    void setTranslateX(double value) { translateX_ = value; }
    void setTranslateY(double value) { translateY_ = value; }
    double getTranslateX() const { return translateX_; }
    double getTranslateY() const { return translateY_; }

    void setAlignment(Pos pos) { alignment_ = pos; }
    Pos getAlignment() const { return alignment_; }
    Pos usingAlignment() const;

    void setPadding(const Insets& insets) { padding_ = insets; }
    const Insets& getPadding() const { return padding_; }
    void setBorder(const Insets& insets) { border_ = insets; }
    const Insets& getBorder() const { return border_; }

    void setBackground(const Color& color);
    void setVisible(bool visible) { visible_ = visible; }
    bool isVisible() const { return visible_; }
    void setMouseTransparent(bool value) { mouseTransparent_ = value; }
    bool isMouseTransparent() const { return mouseTransparent_; }
    // JavaFX's pickOnBounds. False makes the node itself never the target of a
    // press, hover, or cursor: the pointer finds its children, or whatever is
    // under it, as if its own box were empty. An overlay that fills a view but
    // should only catch the mouse on its contents uses it.
    void setPickOnBounds(bool value) { pickOnBounds_ = value; }
    bool isPickOnBounds() const { return pickOnBounds_; }
    void setOpacity(float opacity) { opacity_ = opacity; }
    float getOpacity() const { return opacity_; }

    Node* getParent() const { return parent_; }
    Scene* getScene() const { return scene_; }

    void setStyle(std::string css);
    const std::string& getStyle() const { return styleText_; }

    // Cursor while the pointer is over this node. A stylesheet replaces it.
    // Inherit follows the parent. Auto uses the control's own cursor.
    void setCursor(Cursor cursor);
    Cursor getCursor() const;
    void setStylesheet(std::string css);
    ObservableList<std::string>& getClassList() { return classList_; }
    const ObservableList<std::string>& getClassList() const { return classList_; }
    // Values a parent reads about this child, as JavaFX's getProperties. Layout
    // panes keep constraints here, such as GridPane::setColumnIndex.
    std::unordered_map<std::string, std::any>& getProperties() { return properties_; }
    const std::unordered_map<std::string, std::any>& getProperties() const { return properties_; }

    void setElementId(std::string id) { id_ = std::move(id); }
    const std::string& getElementId() const { return id_; }

    void setOnMousePressed(MouseHandler handler) { onPressed_ = std::move(handler); }
    void setOnMouseReleased(MouseHandler handler) { onReleased_ = std::move(handler); }
    void setOnMouseClicked(MouseHandler handler) { onClicked_ = std::move(handler); }
    void setOnMouseEntered(MouseHandler handler) { onEntered_ = std::move(handler); }
    void setOnMouseExited(MouseHandler handler) { onExited_ = std::move(handler); }
    // JavaFX's focusedProperty listener. Runs with true when this node takes the
    // focus and false when it loses it, after handleFocusGained or handleFocusLost.
    void setOnFocusChanged(std::function<void(bool focused)> handler) { onFocusChanged_ = std::move(handler); }
    // Right-click. The scene walks from the hit node to the root and stops at the first handler.
    void setOnContextMenuRequested(MouseHandler handler) { onContext_ = std::move(handler); }
    bool hasContextMenuHandler() const { return static_cast<bool>(onContext_); }
    void fireContextMenu(const MouseEvent& event) {
        if (onContext_) {
            onContext_(event);
        }
    }

    // Drag and drop, in the shape of JavaFX's. A press that moves far enough to be a
    // drag runs drag-detected on the pressed node and its ancestors; a handler starts
    // the drag with startDragAndDrop. The node under the pointer then hears entered,
    // over, and exited, and dropped on release; the source hears done at the end.
    void setOnDragDetected(MouseHandler handler) { drag_.detected = std::move(handler); }
    void setOnDragEntered(DragHandler handler) { drag_.entered = std::move(handler); }
    void setOnDragOver(DragHandler handler) { drag_.over = std::move(handler); }
    void setOnDragExited(DragHandler handler) { drag_.exited = std::move(handler); }
    void setOnDragDropped(DragHandler handler) { drag_.dropped = std::move(handler); }
    void setOnDragDone(DragHandler handler) { drag_.done = std::move(handler); }
    // Starts a drag from this node that allows modes, and returns its dragboard to fill.
    // Works only while drag-detected runs for this node; otherwise returns null.
    Dragboard* startDragAndDrop(TransferModes modes);

    bool isHovered() const { return hovered_; }
    bool isPressed() const { return pressed_; }
    // Keyboard arming uses the same flag as a mouse press, so :active matches.
    void setPressed(bool pressed) { pressed_ = pressed; }
    bool isFocused() const { return focused_; }
    // True when this node or a descendant is focused. Matches the :focus-within pseudo.
    bool isFocusWithin();
    void setSelected(bool selected) { selected_ = selected; }
    bool isSelected() const { return selected_; }

    // Extra pseudo-classes such as :horizontal and :vertical, as JavaFX's PseudoClass.
    // A stylesheet matches any other name against these. Hover, focus, and disabled stay separate.
    void setPseudoState(const std::string& name, bool enabled);
    bool pseudoState(const std::string& name) const;
    // This node's 1-based place among its parent's children, which :nth-child()
    // matches. Zero without a parent. A recycled cell reports its item's place instead.
    virtual int getNthChildIndex() const;

    // disable is this node's own flag. disabled is that flag, or an ancestor's.
    // A disabled node is not picked, focused, or sent input. TabPane keeps its own
    // flag so a disabled pane can still show an interactive page.
    void setDisable(bool value);
    bool isDisable() const { return disable_; }
    bool isDisabled() const;

    // True when node is this or a descendant.
    bool isAncestorOf(const Node* node) const;

    // True while this node's destructor has started. Scene sets it before its
    // members are destroyed so controls can skip hooks and popups on the way out.
    bool isTearingDown() const { return tearingDown_; }

    // Replaces any hover popup. An empty content clears it. Scene shows content
    // after showDelay and hides it on press, after hideDelay, or at showDuration.
    void setHoverPopup(HoverPopup popup);
    void clearHoverPopup();
    const HoverPopup* getHoverPopup() const;

    bool contains(double x, double y) const;
    Node* pick(double x, double y);
    // The cursor at a window point this node covers. A control may use a different
    // cursor for a scrollbar than for its text.
    virtual Cursor cursorAt(double x, double y) const;

    // A press on a node that is not focus traversable focuses its nearest ancestor
    // that is, as JavaFX does for a control's inner scroll bars. The default is true.
    void setFocusTraversable(bool value) { focusTraversable_ = value; }
    bool isFocusTraversable() const { return focusTraversable_; }

    // A node that captures keys, as a terminal does, gets them while it or a
    // node inside it is focused before the scene's key hooks and menu
    // accelerators do. A key it does not consume goes on to them.
    void setCapturesKeys(bool value) { capturesKeys_ = value; }
    bool capturesKeys() const { return capturesKeys_; }

    // The scene gives a node only the left button unless it asks for all of them,
    // as a view that turns a camera while the right button is held does. A right
    // or middle press then goes to the nearest such node over the pointer, and
    // its release to the same node wherever the pointer is. A right press still
    // asks for a context menu as well.
    void setReceivesAllButtons(bool value) { receivesAllButtons_ = value; }
    bool receivesAllButtons() const { return receivesAllButtons_; }

    // Focus this node. Keys and text input are delivered here until another press.
    void requestFocus();

    // Input that belongs to the node under the pointer, or to the focused node for keys.
    // A control overrides these. The mouse callbacks above still run as well.
    virtual void handleMousePressed(const MouseEvent&) {}
    virtual void handleMouseReleased(const MouseEvent&) {}
    virtual void handleMouseDragged(const MouseEvent&) {}
    virtual void handleMouseMoved(const MouseEvent&) {}
    virtual void handleScroll(ScrollEvent&) {}
    virtual void handleKey(KeyEvent&) {}
    virtual void handleText(TextEvent&) {}
    // Focus moved off this node: to another node, to nothing, or because this node left the scene
    // or was disabled. Not called when a press lands on the node that already has focus.
    virtual void handleFocusLost() {}
    // This node took the focus, from a press, requestFocus, or the keyboard.
    virtual void handleFocusGained() {}
    // isHovered changed. Runs before the entered or exited handler, so a control
    // can restyle itself and leave those handlers to the application.
    virtual void handleHoverChanged() {}
    // Drag and drop, before the handlers set above. A control that starts drags or
    // takes drops overrides these.
    virtual void handleDragDetected(const MouseEvent&) {}
    virtual void handleDragEntered(DragEvent&) {}
    virtual void handleDragOver(DragEvent&) {}
    virtual void handleDragExited(DragEvent&) {}
    virtual void handleDragDropped(DragEvent&) {}
    virtual void handleDragDone(DragEvent&) {}

    Node* getElementById(const std::string& id);
    std::vector<Node*> getElementsByClassName(const std::string& className);

    const ComputedStyle& computedStyle() const { return computed_; }
    // A theme color as this node's style sets it, through the custom property
    // Theme::variableName names, or the light theme's value when nothing sets it.
    // currentColor is the node's text color, as in CSS.
    Color themeColor(ThemeColor color) const;

    // Reparents the node. Adding it to a child list does this on its own.
    void setParent(Node* parent);
    // Drop a child this node owns, including a slot that is not in the child list.
    virtual void detachChild(Node* child);

    // Resolves this node's style and its children's now, as JavaFX's applyCss
    // does, instead of at the next Scene::layout. A control that creates nodes
    // during layout calls this before it measures them.
    void applyCss();

    // Place this node inside its parent and lay out its children.
    // Scene::layout is the call most applications make.
    void performLayout(double x, double y, double width, double height);
    double measuredWidth(double available) const;
    double measuredHeight(double width, double availableHeight) const;

    virtual void render(UiRenderer& renderer, float opacity);

protected:
    ObservableList<std::shared_ptr<Node>>& children() { return children_; }
    const ObservableList<std::shared_ptr<Node>>& children() const { return children_; }

    virtual void layoutChildren();
    virtual double preferredContentWidth(double innerAvailable) const;
    virtual double preferredContentHeight(double innerWidth) const;
    virtual void renderContent(UiRenderer& renderer, float opacity);
    // Drawn after the background and before renderContent. A scroller clips this.
    virtual void renderChildren(UiRenderer& renderer, float opacity);
    virtual void visitChildren(const std::function<void(Node*)>& visitor);
    virtual Scene* asScene() { return nullptr; }
    // A SubScene's root starts a cascade of its own. See SubScene.
    virtual const SubScene* asSubScene() const { return nullptr; }
    // previous is the scene this node just left, or null when it is joining one.
    virtual void sceneChanged(Scene*) {}
    // This node's style is resolved. Children are styled after this returns.
    virtual void styleDidApply() {}
    // host no longer holds this node as its hover popup: its popup was replaced
    // or cleared, or host is being destroyed. The popup can outlive host, since
    // the scene and the application may hold it too, so a popup that keeps a
    // pointer to its host lets go of it here.
    virtual void hoverHostReleased(const Node* /*host*/) {}

    double contentLeft() const;
    double contentTop() const;
    double contentWidth() const;
    double contentHeight() const;

    ComputedStyle& computed() { return computed_; }
    const Font& font() const { return font_; }
    bool fontExplicit() const { return fontExplicit_; }
    bool fillExplicit() const { return fillExplicit_; }
    const Color& textFill() const { return textFill_; }
    float spacingValue() const { return spacing_; }
    void setSpacingValue(double spacing) { spacing_ = static_cast<float>(spacing); }

    void setFontInternal(const Font& font, bool explicitSize);
    void setTextFillInternal(const Color& color, bool explicitColor);
    void setSubpixelRenderingInternal(bool enabled);
    // Buttons and text controls set this. setCursor and stylesheets replace it.
    void setDefaultCursor(Cursor cursor) { defaultCursor_ = cursor; }

    friend class Scene;
    friend class SplitPane;

private:
    void applyStyles(const ComputedStyle& inherited, double timeSeconds);
    // What a child inherits from this node's style: text color, font, and cursor.
    ComputedStyle inheritableStyle() const;
    // What the top of a cascade inherits: a scene, or a SubScene's root.
    static ComputedStyle rootInheritance();
    // What this node's parent hands it: rootInheritance for a SubScene's root.
    ComputedStyle inheritedFromParent() const;
    // The nearest enclosing SubScene's user-agent stylesheet, else the scene's. Null for neither.
    const Stylesheet* userAgentSheet() const;
    void syncHover(Node* hit);
    // Like pick, but a disabled node still supplies its cursor.
    Node* pickCursorTarget(double x, double y);
    void setPressedChain(Node* hit);
    void clearFocus();
    void markFocused(Node* hit);
    void dispatchHoverChanges();
    void fireMouse(const MouseHandler Node::* handler, const MouseEvent& event);
    void drawChrome(UiRenderer& renderer, float opacity);

    struct ColorAnim {
        Color displayed;
        Color from;
        Color to;
        double start = 0;
        double duration = 0;
        bool ready = false;
    };

    Color animateColor(ColorAnim& anim, const Color& target, double duration, double delay, double time);

    ObservableList<std::shared_ptr<Node>> children_;
    ObservableList<std::string> classList_;
    Node* parent_ = nullptr;
    Scene* scene_ = nullptr;

    double x_ = 0;
    double y_ = 0;
    double width_ = 0;
    double height_ = 0;
    double translateX_ = 0;
    double translateY_ = 0;

    SizeSpec prefWidth_;
    SizeSpec prefHeight_;
    SizeSpec minWidth_;
    SizeSpec minHeight_;
    SizeSpec maxWidth_;
    SizeSpec maxHeight_;
    Insets padding_;
    Insets border_;
    Pos alignment_ = Pos::Ancestor;
    float spacing_ = 0.f;
    float opacity_ = 1.f;

    Color background_ = Color::transparent();
    bool backgroundExplicit_ = false;
    Font font_{"Open Sans", 16.f};
    bool fontExplicit_ = false;
    Color textFill_ = Color::black();
    bool fillExplicit_ = false;
    bool subpixel_ = kSubpixelByDefault;
    bool subpixelExplicit_ = false;
    Cursor cursor_ = Cursor::Inherit;
    bool cursorExplicit_ = false;
    Cursor defaultCursor_ = Cursor::Inherit;

    std::string id_;
    std::unordered_map<std::string, std::any> properties_;
    std::string styleText_;
    std::vector<Declaration> inline_;
    Stylesheet stylesheet_;
    ComputedStyle computed_;

    struct InsetAnim {
        Insets displayed{};
        Insets from{};
        Insets to{};
        double start = 0;
        double duration = 0;
        bool ready = false;
    };
    struct ShadowAnim {
        std::vector<BoxShadow> displayed;
        std::vector<BoxShadow> from;
        std::vector<BoxShadow> to;
        double start = 0;
        double duration = 0;
        bool ready = false;
    };

    ColorAnim backgroundAnim_;
    ColorAnim stopAnim_[kMaxGradientStops];
    ColorAnim colorAnim_;
    ColorAnim borderColorAnim_;
    InsetAnim borderAnim_;
    ShadowAnim shadowAnim_;

    Insets animateInsets(InsetAnim& anim, const Insets& target, double duration, double delay, double time);
    bool visible_ = true;
    bool mouseTransparent_ = false;
    bool pickOnBounds_ = true;
    bool hovered_ = false;
    bool wasHovered_ = false;
    bool pressed_ = false;
    bool focused_ = false;
    bool selected_ = false;
    bool disable_ = false;
    bool focusTraversable_ = true;
    bool capturesKeys_ = false;
    bool receivesAllButtons_ = false;
    std::vector<std::string> pseudoStates_;
    // Unset means a SplitPane may resize this item with the pane.
    std::optional<bool> resizableWithParent_;
    bool tearingDown_ = false;
    std::optional<HoverPopup> hoverPopup_;

    MouseHandler onPressed_;
    MouseHandler onReleased_;
    MouseHandler onClicked_;
    MouseHandler onEntered_;
    MouseHandler onExited_;
    MouseHandler onContext_;
    std::function<void(bool)> onFocusChanged_;

    struct DragHandlers {
        MouseHandler detected;
        DragHandler entered;
        DragHandler over;
        DragHandler exited;
        DragHandler dropped;
        DragHandler done;
    };
    DragHandlers drag_;

    void fireFocusChanged(bool focused) {
        if (onFocusChanged_) {
            onFocusChanged_(focused);
        }
    }
};

}  // namespace jadefx

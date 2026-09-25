#pragma once

#include "jadefx/collections/ObservableList.hpp"
#include "jadefx/event/Events.hpp"
#include "jadefx/geometry/Geometry.hpp"
#include "jadefx/paint/Color.hpp"
#include "jadefx/scene/text/Font.hpp"
#include "jadefx/style/Style.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace jadefx {

class Node;
class Scene;
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
    void setOpacity(float opacity) { opacity_ = opacity; }

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
    void setElementId(std::string id) { id_ = std::move(id); }
    const std::string& getElementId() const { return id_; }

    void setOnMousePressed(MouseHandler handler) { onPressed_ = std::move(handler); }
    void setOnMouseReleased(MouseHandler handler) { onReleased_ = std::move(handler); }
    void setOnMouseClicked(MouseHandler handler) { onClicked_ = std::move(handler); }
    void setOnMouseEntered(MouseHandler handler) { onEntered_ = std::move(handler); }
    void setOnMouseExited(MouseHandler handler) { onExited_ = std::move(handler); }
    // Right-click. The scene walks from the hit node to the root and stops at the first handler.
    void setOnContextMenuRequested(MouseHandler handler) { onContext_ = std::move(handler); }
    bool hasContextMenuHandler() const { return static_cast<bool>(onContext_); }
    void fireContextMenu(const MouseEvent& event) {
        if (onContext_) {
            onContext_(event);
        }
    }

    bool isHovered() const { return hovered_; }
    bool isPressed() const { return pressed_; }
    // Keyboard arming uses the same flag as a mouse press, so :active matches.
    void setPressed(bool pressed) { pressed_ = pressed; }
    bool isFocused() const { return focused_; }
    // True when this node or a descendant is focused. Matches the :focus-within pseudo.
    bool isFocusWithin();
    void setSelected(bool selected) { selected_ = selected; }
    bool isSelected() const { return selected_; }

    // Extra pseudos such as :horizontal and :vertical. Hover, focus, and disabled stay separate.
    void setPseudoState(const std::string& name, bool enabled);
    bool pseudoState(const std::string& name) const;

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

    Node* getElementById(const std::string& id);
    std::vector<Node*> getElementsByClassName(const std::string& className);

    const ComputedStyle& computedStyle() const { return computed_; }

    // Reparents the node. Adding it to a child list does this on its own.
    void setParent(Node* parent);
    // Drop a child this node owns, including a slot that is not in the child list.
    virtual void detachChild(Node* child);

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
    // previous is the scene this node just left, or null when it is joining one.
    virtual void sceneChanged(Scene*) {}
    // This node's style is resolved. Children are styled after this returns.
    virtual void styleDidApply() {}

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
    bool subpixel_ = true;
    bool subpixelExplicit_ = false;
    Cursor cursor_ = Cursor::Inherit;
    bool cursorExplicit_ = false;
    Cursor defaultCursor_ = Cursor::Inherit;

    std::string id_;
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
    bool hovered_ = false;
    bool wasHovered_ = false;
    bool pressed_ = false;
    bool focused_ = false;
    bool selected_ = false;
    bool disable_ = false;
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
};

}  // namespace jadefx

#include "jadefx/scene/Scene.hpp"

#include "jadefx/scene/layout/StackPane.hpp"
#include "jadefx/style/Theme.hpp"
#include "gl/UiRenderer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace jadefx {
namespace {

// How far the pointer may wander from a press, in points, and still be a click.
constexpr double kPressHysteresis = 4;

// The Mod bit a modifier key sets, or 0 for any other key.
int ModifierBit(int key) {
    switch (key) {
        case Key::LeftShift:
        case Key::RightShift:
            return Key::ModShift;
        case Key::LeftControl:
        case Key::RightControl:
            return Key::ModControl;
        case Key::LeftAlt:
        case Key::RightAlt:
            return Key::ModAlt;
        case Key::LeftSuper:
        case Key::RightSuper:
            return Key::ModSuper;
        default:
            return 0;
    }
}
// Presses closer together than this, in time and in points, count as one multi-click.
constexpr double kMultiClickSeconds = 0.4;
constexpr double kMultiClickPoints = 4;

bool Related(Node* a, Node* b) {
    if (a == nullptr || b == nullptr) {
        return false;
    }
    if (a == b) {
        return true;
    }
    for (Node* node = a; node != nullptr; node = node->getParent()) {
        if (node == b) {
            return true;
        }
    }
    for (Node* node = b; node != nullptr; node = node->getParent()) {
        if (node == a) {
            return true;
        }
    }
    return false;
}

}  // namespace

Scene::Scene(std::shared_ptr<Node> root) : Scene(std::move(root), 0, 0) {}

void Scene::setUserAgentStylesheet(std::string cssOrTheme) {
    userAgent_ = cssOrTheme.empty() ? Stylesheet() : Stylesheet::parse(Theme::expand(cssOrTheme));
    userAgentSource_ = std::move(cssOrTheme);
}

const Stylesheet& Scene::userAgentStylesheet() const {
    return userAgentSource_.empty() ? Theme::userAgentStylesheet() : userAgent_;
}

Scene::Scene(std::shared_ptr<Node> root, double prefWidth, double prefHeight)
    : requestedWidth_(prefWidth), requestedHeight_(prefHeight) {
    scene_ = this;
    // The scene is the document root, so :root rules and their variables start here.
    setPseudoState("root", true);
    internal_ = std::make_shared<StackPane>();
    // StackPane's default Center would stop the root from inheriting the scene alignment.
    internal_->setAlignment(Pos::Ancestor);
    children().add(internal_);
    if (root) {
        setRoot(std::move(root));
    }
}

Scene::~Scene() {
    // Detach every descendant while this object is still alive. Nodes the
    // caller kept then see a null scene instead of a freed one.
    tearingDown_ = true;
    keyHooks_.clear();
    hover_ = {};
    popups_.clear();
    children().clear();
}

void Scene::setRoot(std::shared_ptr<Node> root) {
    if (internal_) {
        internal_->getChildren().clear();
    }
    root_ = std::move(root);
    if (internal_ && root_) {
        internal_->getChildren().add(root_);
    }
}

void Scene::layout(double width, double height) {
    using Clock = std::chrono::steady_clock;
    static const auto start = Clock::now();
    const double time = std::chrono::duration<double>(Clock::now() - start).count();
    layout(width, height, time);
}

void Scene::layout(double width, double height, double timeSeconds) {
    ComputedStyle inherited;
    inherited.color = Theme::defaultColor(ThemeColor::Text);
    inherited.fontSize = 16.f;
    inherited.fontFamily = "Open Sans";
    inherited.cursor = Cursor::Default;
    applyStyles(inherited, timeSeconds);

    x_ = 0;
    y_ = 0;
    width_ = std::max(0.0, width);
    height_ = std::max(0.0, height);
    lastWidth_ = width_;
    lastHeight_ = height_;
    lastTime_ = timeSeconds;
    laidOut_ = true;

    if (!internal_) {
        return;
    }
    const double right = computed_.padding.right + computed_.border.right;
    const double bottom = computed_.padding.bottom + computed_.border.bottom;
    const double x = safe_.left + contentLeft();
    const double y = safe_.top + contentTop();
    const double innerWidth = std::max(0.0, width_ - safe_.left - safe_.right - contentLeft() - right);
    const double innerHeight = std::max(0.0, height_ - safe_.top - safe_.bottom - contentTop() - bottom);
    internal_->performLayout(x, y, innerWidth, innerHeight);
    for (std::size_t i = 0; i < popups_.size(); ++i) {
        layoutPopup(popups_[i]);
    }
    if (pointerValid_) {
        updateHoverPopup(pick(pointerX_, pointerY_));
    }
    // A copy, since a listener may add or remove listeners.
    const auto listeners = pulseListeners_;
    for (const auto& listener : listeners) {
        if (listener.second) {
            listener.second();
        }
    }
}

void Scene::notePointerExit() {
    pointerValid_ = false;
    syncHover(nullptr);
    updateHoverPopup(nullptr);
}

Cursor Scene::hoverCursor() {
    if (!pointerValid_) {
        return Cursor::Default;
    }
    // A drag shows what a drop there would do.
    if (drag_ != nullptr) {
        if (drag_->acceptor == nullptr) {
            return Cursor::NotAllowed;
        }
        return drag_->mode == TransferMode::Copy ? Cursor::Copy
               : drag_->mode == TransferMode::Link ? Cursor::Alias
                                                   : Cursor::Default;
    }
    Node* hit = pickCursorTarget(pointerX_, pointerY_);
    if (hit == nullptr) {
        return Cursor::Default;
    }
    return hit->cursorAt(pointerX_, pointerY_);
}

void Scene::noteMove(double x, double y) {
    pointerX_ = x;
    pointerY_ = y;
    pointerValid_ = true;
    // While a drag and drop runs, the pointer belongs to it.
    if (drag_ != nullptr) {
        if (!drag_->ending) {
            updateDrag(*drag_, x, y, drag_->source);
        }
        return;
    }
    Node* hit = pick(x, y);
    syncHover(hit);
    MouseEvent event;
    event.x = x;
    event.y = y;
    event.mods = keyMods_;
    if (pressedTarget_ != nullptr) {
        bool detect = false;
        if (stillSincePress_ && std::hypot(x - pressX_, y - pressY_) > kPressHysteresis) {
            stillSincePress_ = false;
            // A drag is never the first half of a double-click.
            lastClickSeconds_ = -1;
            detect = true;
        }
        event.clickCount = clickCount_;
        event.stillSincePress = stillSincePress_;
        event.target = pressedTarget_;
        pressedTarget_->handleMouseDragged(event);
        // As in JavaFX, drag-detected follows the first dragged event past the threshold.
        if (detect && pressedTarget_ != nullptr) {
            detectDrag(event);
            if (drag_ != nullptr) {
                updateDrag(*drag_, x, y, drag_->source);
                return;
            }
        }
    }
    if (hit != nullptr) {
        event.target = hit;
        hit->handleMouseMoved(event);
    }
    updateHoverPopup(hit);
}

void Scene::noteButton(int button, bool down, double x, double y, int mods) {
    keyMods_ = mods;
    noteMove(x, y);
    // GLFW button 1 is the right button. A press opens a context menu and does not click.
    if (button == 1) {
        if (!down) {
            return;
        }
        Node* hit = pick(x, y);
        std::vector<Node*> dismiss;
        for (const PopupRecord& popup : popups_) {
            if (popup.node && !popupStays(popup, hit, 0)) {
                dismiss.push_back(popup.node.get());
            }
        }
        for (Node* popup : dismiss) {
            hidePopup(popup);
        }
        hit = pick(x, y);
        MouseEvent event;
        event.x = x;
        event.y = y;
        event.button = button;
        event.mods = mods;
        for (Node* node = hit; node != nullptr; node = node->getParent()) {
            if (!node->hasContextMenuHandler()) {
                continue;
            }
            event.target = node;
            node->fireContextMenu(event);
            break;
        }
        return;
    }
    if (button != 0) {
        return;
    }
    MouseEvent event;
    event.x = x;
    event.y = y;
    event.button = button;
    event.mods = mods;
    if (down) {
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        const bool repeat = lastClickSeconds_ >= 0 && now - lastClickSeconds_ < kMultiClickSeconds &&
                            std::hypot(x - lastClickX_, y - lastClickY_) < kMultiClickPoints;
        clickCount_ = repeat ? clickCount_ + 1 : 1;
        lastClickSeconds_ = now;
        lastClickX_ = x;
        lastClickY_ = y;
        event.clickCount = clickCount_;
        Node* hit = pick(x, y);
        std::vector<Node*> dismiss;
        for (const PopupRecord& popup : popups_) {
            if (popup.node && !popupStays(popup, hit, 0)) {
                dismiss.push_back(popup.node.get());
            }
        }
        for (Node* popup : dismiss) {
            hidePopup(popup);
        }
        pressedTarget_ = pick(x, y);
        pressX_ = x;
        pressY_ = y;
        stillSincePress_ = true;
        setPressedChain(pressedTarget_);
        Node* focusTarget = pressedTarget_;
        while (focusTarget != nullptr && !focusTarget->isFocusTraversable()) {
            focusTarget = focusTarget->getParent();
        }
        moveFocus(focusTarget);
        if (pressedTarget_ != nullptr) {
            event.target = pressedTarget_;
            pressedTarget_->handleMousePressed(event);
            fireMouse(&Node::onPressed_, event);
        }
        return;
    }

    if (drag_ != nullptr) {
        finishDrag(x, y, true);
        pressedTarget_ = nullptr;
        return;
    }
    event.clickCount = clickCount_;
    Node* released = pick(x, y);
    Node* pressed = pressedTarget_;
    event.stillSincePress = pressed == nullptr || stillSincePress_;
    setPressedChain(nullptr);
    if (pressed != nullptr) {
        event.target = pressed;
        pressed->handleMouseReleased(event);
        fireMouse(&Node::onReleased_, event);
    }
    if (Related(pressed, released)) {
        event.target = pressed;
        fireMouse(&Node::onClicked_, event);
    }
    pressedTarget_ = nullptr;
}

void Scene::noteScroll(double x, double y, double deltaX, double deltaY) {
    noteMove(x, y);
    ScrollEvent event;
    event.x = x;
    event.y = y;
    event.deltaX = deltaX;
    event.deltaY = deltaY;
    event.target = pick(x, y);
    for (Node* node = event.target; node != nullptr && !event.consumed; node = node->getParent()) {
        node->handleScroll(event);
    }
}

bool Scene::noteKey(int key, bool pressed, bool repeat, int mods) {
    // A modifier key's own event carries the state from before it (GLFW on X11), so it is folded in here.
    const int bit = ModifierBit(key);
    if (bit != 0) {
        mods = pressed || repeat ? (mods | bit) : (mods & ~bit);
    }
    const bool modifiersChanged = mods != keyMods_;
    keyMods_ = mods;
    // What is under a still pointer can depend on the modifiers, such as a link that follows on Ctrl-click.
    if (modifiersChanged && pointerValid_ && pressedTarget_ == nullptr) {
        if (Node* hit = pick(pointerX_, pointerY_)) {
            MouseEvent moved;
            moved.x = pointerX_;
            moved.y = pointerY_;
            moved.mods = keyMods_;
            moved.target = hit;
            hit->handleMouseMoved(moved);
        }
    }
    if (focused_ != nullptr && focused_->getScene() != this) {
        focused_ = nullptr;
    }
    KeyEvent event;
    event.key = key;
    event.pressed = pressed;
    event.repeat = repeat;
    event.shift = (mods & Key::ModShift) != 0;
    event.control = (mods & Key::ModControl) != 0;
    event.alt = (mods & Key::ModAlt) != 0;
    event.meta = (mods & Key::ModSuper) != 0;
    // A node that captures keys, and the nodes inside it, see the key before
    // the hooks. What it leaves goes on from its parent after them.
    Node* resume = focused_;
    for (Node* node = focused_; node != nullptr; node = node->getParent()) {
        if (!node->capturesKeys()) {
            continue;
        }
        for (Node* inside = focused_; inside != node->getParent(); inside = inside->getParent()) {
            inside->handleKey(event);
            if (event.consumed) {
                return true;
            }
        }
        resume = node->getParent();
        break;
    }
    const std::vector<HookRecord> hooks = keyHooks_;
    for (const HookRecord& hook : hooks) {
        if (hook.hook) {
            hook.hook(event);
        }
        if (event.consumed) {
            return true;
        }
    }
    if (event.pressed && event.key == Key::Escape && drag_ != nullptr) {
        finishDrag(pointerX_, pointerY_, false);
        return true;
    }
    if (event.pressed && event.key == Key::Escape) {
        for (auto it = popups_.rbegin(); it != popups_.rend(); ++it) {
            if (it->node && (it->autoHide || it->hideOnPress)) {
                hidePopup(it->node.get());
                return true;
            }
        }
    }
    for (Node* node = resume; node != nullptr; node = node->getParent()) {
        node->handleKey(event);
        if (event.consumed) {
            return true;
        }
    }
    return false;
}

bool Scene::noteText(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    // Shortcuts use the key event. A matching character would otherwise be inserted too.
    if ((keyMods_ & Key::ModControl) != 0 || (keyMods_ & Key::ModSuper) != 0) {
        return false;
    }
    if (focused_ != nullptr && focused_->getScene() != this) {
        focused_ = nullptr;
    }
    TextEvent event;
    event.text = text;
    for (Node* node = focused_; node != nullptr; node = node->getParent()) {
        node->handleText(event);
        if (event.consumed) {
            return true;
        }
    }
    return false;
}

void Scene::moveFocus(Node* next) {
    Node* const previous = focused_;
    clearFocus();
    // previous may already be out of the tree, as a field in a popup that just
    // closed, where clearing the scene's nodes does not reach it.
    if (previous != nullptr) {
        previous->focused_ = false;
    }
    focused_ = next;
    // In a window without the system focus only the owner changes. Nothing is
    // focused, and the owner already heard it lost the focus when the window did.
    if (!windowFocused_) {
        return;
    }
    markFocused(next);
    if (previous == next || isTearingDown()) {
        return;
    }
    // A node leaves focus before it leaves the scene, so previous is still alive here.
    if (previous != nullptr) {
        previous->handleFocusLost();
        previous->fireFocusChanged(false);
    }
    // A handler above may have moved the focus on again.
    if (next != nullptr && focused_ == next) {
        next->handleFocusGained();
        next->fireFocusChanged(true);
    }
}

void Scene::noteWindowFocus(bool focused) {
    if (focused == windowFocused_ || isTearingDown()) {
        return;
    }
    windowFocused_ = focused;
    if (focused_ != nullptr && focused_->getScene() != this) {
        focused_ = nullptr;
    }
    if (!focused) {
        keyMods_ = 0;
        finishDrag(pointerX_, pointerY_, false);
        std::vector<Node*> dismiss;
        for (const PopupRecord& popup : popups_) {
            if (popup.node && (popup.autoHide || popup.hideOnPress)) {
                dismiss.push_back(popup.node.get());
            }
        }
        for (Node* popup : dismiss) {
            hidePopup(popup);
        }
        clearFocus();
        if (focused_ != nullptr) {
            focused_->handleFocusLost();
            focused_->fireFocusChanged(false);
        }
        return;
    }
    if (focused_ != nullptr) {
        markFocused(focused_);
        focused_->handleFocusGained();
        focused_->fireFocusChanged(true);
    }
}

void Scene::requestFocus(Node* node) {
    if (node != nullptr && node != this && (node->getScene() != this || node->isDisabled())) {
        node = nullptr;
    }
    moveFocus(node);
}

void Scene::releaseFocus(Node* node) {
    if (focused_ == nullptr || node == nullptr) {
        return;
    }
    for (Node* cursor = focused_; cursor != nullptr; cursor = cursor->getParent()) {
        if (cursor == node) {
            moveFocus(nullptr);
            return;
        }
    }
}

void Scene::setClipboardText(std::string text) {
    if (clipboardSet_) {
        clipboardSet_(text);
        return;
    }
    clipboard_ = std::move(text);
}

std::string Scene::clipboardText() const {
    if (clipboardGet_) {
        return clipboardGet_();
    }
    return clipboard_;
}

void Scene::setClipboardBridge(std::function<void(const std::string&)> setText, std::function<std::string()> getText) {
    clipboardSet_ = std::move(setText);
    clipboardGet_ = std::move(getText);
}

Scene::PopupRecord* Scene::findPopup(const Node* popup) {
    for (PopupRecord& record : popups_) {
        if (record.node.get() == popup) {
            return &record;
        }
    }
    return nullptr;
}

void Scene::layoutPopup(PopupRecord& popup) {
    if (!popup.node) {
        return;
    }
    popup.node->applyStyles(inheritableStyle(), lastTime_);
    if (popup.fillScene) {
        popup.node->performLayout(0, 0, std::max(0.0, width_), std::max(0.0, height_));
        return;
    }
    double width = popup.width;
    double height = popup.height;
    if (popup.measure) {
        const double available = std::max(0.0, width_);
        width = popup.node->measuredWidth(available > 0 ? available : 100000);
        height = popup.node->measuredHeight(width, -1);
        popup.width = width;
        popup.height = height;
    }
    popup.node->performLayout(popup.x, popup.y, std::max(0.0, width), std::max(0.0, height));
}

bool Scene::popupStays(const PopupRecord& popup, Node* hit, int depth) const {
    if (depth > 8 || !popup.node) {
        return false;
    }
    if (popup.hideOnPress) {
        return false;
    }
    if (!popup.autoHide) {
        return true;
    }
    if (popup.node->isAncestorOf(hit)) {
        return true;
    }
    if (popup.owner != nullptr && popup.owner->isAncestorOf(hit)) {
        return true;
    }
    for (const PopupRecord& other : popups_) {
        if (!other.node || other.node.get() == popup.node.get() || other.owner == nullptr) {
            continue;
        }
        if (popup.node->isAncestorOf(other.owner) && popupStays(other, hit, depth + 1)) {
            return true;
        }
    }
    return false;
}

void Scene::showPopup(std::shared_ptr<Node> popup, double x, double y, double width, double height, PopupOptions options) {
    if (!popup) {
        return;
    }
    Node* const popupNode = popup.get();
    if (findPopup(popupNode) == nullptr) {
        PopupRecord created;
        created.node = popup;
        created.openedAt = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        popups_.push_back(std::move(created));
        bool listed = false;
        for (const std::shared_ptr<Node>& child : children().items()) {
            if (child.get() == popupNode) {
                listed = true;
            }
        }
        if (!listed) {
            children().add(popup);
        }
    }
    PopupRecord* record = findPopup(popupNode);
    if (record == nullptr) {
        return;
    }
    // As an HTML popover while it is shown.
    popupNode->setPseudoState("popover-open", true);
    record->owner = options.owner;
    record->autoHide = options.autoHide;
    record->hideOnPress = options.hideOnPress;
    record->modal = options.modal;
    record->fillScene = options.fillScene;
    record->animate = options.animate && !options.fillScene && !options.modal;
    record->fromBottom = false;
    record->x = x;
    record->y = y;
    record->measure = width < 0 || height < 0 || options.fillScene;
    record->width = width;
    record->height = height;
    layoutPopup(*record);
}

void Scene::showPopupNear(std::shared_ptr<Node> popup, Node* anchor, Side side, PopupOptions options) {
    if (!popup || anchor == nullptr) {
        return;
    }
    Node* const popupNode = popup.get();
    showPopup(std::move(popup), 0, 0, -1, -1, options);
    PopupRecord* record = findPopup(popupNode);
    if (record == nullptr || record->node == nullptr) {
        return;
    }
    const double popupWidth = record->width;
    const double popupHeight = record->height;
    const double anchorX = anchor->getAbsoluteX();
    const double anchorY = anchor->getAbsoluteY();
    double x = anchorX;
    double y = anchorY;
    switch (side) {
        case Side::Bottom:
            y = anchorY + anchor->getHeight();
            break;
        case Side::Top:
            y = anchorY - popupHeight;
            break;
        case Side::Right:
            x = anchorX + anchor->getWidth();
            break;
        case Side::Left:
            x = anchorX - popupWidth;
            break;
    }
    auto clips = [&](double left, double top) {
        return width_ > 0 && height_ > 0 &&
               (left < 0 || top < 0 || left + popupWidth > width_ + 0.5 || top + popupHeight > height_ + 0.5);
    };
    if (clips(x, y)) {
        switch (side) {
            case Side::Bottom:
                y = anchorY - popupHeight;
                break;
            case Side::Top:
                y = anchorY + anchor->getHeight();
                break;
            case Side::Right:
                x = anchorX - popupWidth;
                break;
            case Side::Left:
                x = anchorX + anchor->getWidth();
                break;
        }
    }
    if (width_ > 0) {
        if (x + popupWidth > width_) {
            x = width_ - popupWidth;
        }
        if (x < 0) {
            x = 0;
        }
    }
    if (height_ > 0) {
        if (y + popupHeight > height_) {
            y = height_ - popupHeight;
        }
        if (y < 0) {
            y = 0;
        }
    }
    record->x = x;
    record->y = y;
    record->fromBottom = y + popupHeight <= anchorY + 0.5;
    record->node->performLayout(x, y, std::max(0.0, popupWidth), std::max(0.0, popupHeight));
}

void Scene::renderChildren(UiRenderer& renderer, float opacity) {
    constexpr double kOpenSeconds = 0.14;
    constexpr float kShadow = 12.f;
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    std::vector<Node*> kids;
    visitChildren([&](Node* child) { kids.push_back(child); });
    for (Node* child : kids) {
        const PopupRecord* record = findPopup(child);
        const double t = record != nullptr && record->animate ? (now - record->openedAt) / kOpenSeconds : 1.0;
        if (t >= 1.0 || t < 0.0) {
            child->render(renderer, opacity);
            continue;
        }
        const double eased = 1.0 - std::pow(1.0 - t, 3.0);
        const float x = static_cast<float>(child->getAbsoluteX()) - kShadow;
        const float y = static_cast<float>(child->getAbsoluteY());
        const float w = static_cast<float>(child->getWidth()) + 2.f * kShadow;
        const float full = static_cast<float>(child->getHeight()) + kShadow;
        const float shown = full * static_cast<float>(eased);
        if (record->fromBottom) {
            renderer.pushClip(x, y - kShadow + full - shown, w, shown);
        } else {
            renderer.pushClip(x, y, w, shown);
        }
        child->render(renderer, opacity * static_cast<float>(0.25 + 0.75 * eased));
        renderer.popClip();
    }
}

void Scene::movePopup(Node* popup, double x, double y, double width, double height) {
    PopupRecord* record = findPopup(popup);
    if (record == nullptr) {
        return;
    }
    record->x = x;
    record->y = y;
    if (width >= 0 && height >= 0) {
        record->measure = false;
        record->width = width;
        record->height = height;
    }
    layoutPopup(*record);
}

void Scene::hidePopup(Node* popup) {
    if (popup == nullptr) {
        return;
    }
    std::vector<Node*> drop;
    drop.push_back(popup);
    for (std::size_t i = 0; i < drop.size(); ++i) {
        for (const PopupRecord& other : popups_) {
            if (!other.node || other.owner == nullptr || other.node.get() == drop[i]) {
                continue;
            }
            if (std::find(drop.begin(), drop.end(), other.node.get()) != drop.end()) {
                continue;
            }
            if (drop[i]->isAncestorOf(other.owner)) {
                drop.push_back(other.node.get());
            }
        }
    }
    if (hover_.spec.content && std::find(drop.begin(), drop.end(), hover_.spec.content.get()) != drop.end()) {
        hover_.showing = false;
        hover_.shownOwner = nullptr;
        hover_.enteredAt = lastTime_;
    }
    std::vector<std::shared_ptr<Node>> alive;
    popups_.erase(std::remove_if(popups_.begin(), popups_.end(),
                                 [&](PopupRecord& record) {
                                     const bool match =
                                         record.node && std::find(drop.begin(), drop.end(), record.node.get()) != drop.end();
                                     if (match) {
                                         alive.push_back(std::move(record.node));
                                     }
                                     return match;
                                 }),
                  popups_.end());
    for (const std::shared_ptr<Node>& node : alive) {
        if (node) {
            node->setPseudoState("popover-open", false);
        }
        if (node && node->getParent() == this) {
            detachChild(node.get());
        }
    }
}

void Scene::hidePopupsOwnedBy(Node* owner) {
    if (owner == nullptr) {
        return;
    }
    std::vector<Node*> drop;
    for (const PopupRecord& popup : popups_) {
        if (!popup.node) {
            continue;
        }
        if (popup.owner == owner || owner->isAncestorOf(popup.owner)) {
            drop.push_back(popup.node.get());
        }
    }
    for (Node* popup : drop) {
        hidePopup(popup);
    }
    if (hover_.owner != nullptr && (hover_.owner == owner || owner->isAncestorOf(hover_.owner))) {
        hover_.owner = nullptr;
        hover_.armed = false;
        if (hover_.showing && hover_.spec.content) {
            hidePopup(hover_.spec.content.get());
        }
    }
}

bool Scene::isPopupShowing(const Node* popup) const {
    for (const PopupRecord& record : popups_) {
        if (record.node.get() == popup) {
            return true;
        }
    }
    return false;
}

int Scene::addKeyHook(std::function<void(KeyEvent&)> hook) {
    const int id = nextHookId_++;
    keyHooks_.push_back({id, std::move(hook)});
    return id;
}

void Scene::removeKeyHook(int id) {
    keyHooks_.erase(std::remove_if(keyHooks_.begin(), keyHooks_.end(),
                                   [id](const HookRecord& hook) { return hook.id == id; }),
                    keyHooks_.end());
}

int Scene::addPostLayoutPulseListener(std::function<void()> listener) {
    const int id = nextHookId_++;
    pulseListeners_.emplace_back(id, std::move(listener));
    return id;
}

void Scene::removePostLayoutPulseListener(int id) {
    pulseListeners_.erase(std::remove_if(pulseListeners_.begin(), pulseListeners_.end(),
                                         [id](const auto& listener) { return listener.first == id; }),
                          pulseListeners_.end());
}

void Scene::setEventPump(std::function<int()> pump) { eventPump_ = std::move(pump); }

int Scene::runEventPump() {
    if (!eventPump_) {
        return -1;
    }
    return eventPump_();
}

void Scene::updateHoverPopup(Node* hit) {
    bool modal = false;
    for (const PopupRecord& popup : popups_) {
        if (popup.modal) {
            modal = true;
        }
    }
    Node* owner = nullptr;
    const HoverPopup* spec = nullptr;
    if (!modal) {
        if (hover_.showing && hover_.spec.content && hover_.spec.content->isAncestorOf(hit)) {
            owner = hover_.shownOwner;
            spec = &hover_.spec;
        } else {
            for (Node* node = hit; node != nullptr; node = node->getParent()) {
                if (const HoverPopup* found = node->getHoverPopup()) {
                    if (!node->isDisabled()) {
                        owner = node;
                        spec = found;
                    }
                    break;
                }
            }
        }
    }
    const double now = lastTime_;
    if (owner != hover_.owner) {
        if (hover_.showing && hover_.spec.content && owner != hover_.shownOwner) {
            const double hideDelay = hover_.spec.hideDelay;
            if (owner == nullptr && hideDelay > 0) {
                hover_.hideAt = now + hideDelay;
            } else {
                hidePopup(hover_.spec.content.get());
                hover_.showing = false;
                hover_.shownOwner = nullptr;
            }
        }
        hover_.owner = owner;
        hover_.armed = true;
        hover_.enteredAt = now;
        if (spec != nullptr) {
            hover_.spec = *spec;
        }
        if (owner != nullptr) {
            hover_.hideAt = -1;
        }
    }
    if (modal) {
        return;
    }
    if (hover_.showing && hover_.spec.content) {
        const bool expired = hover_.spec.showDuration > 0 && now - hover_.shownAt >= hover_.spec.showDuration;
        if (expired || !hover_.armed) {
            hidePopup(hover_.spec.content.get());
            hover_.showing = false;
            hover_.shownOwner = nullptr;
            hover_.armed = false;
            return;
        }
        if (hover_.owner == nullptr && hover_.hideAt >= 0 && now >= hover_.hideAt) {
            hidePopup(hover_.spec.content.get());
            hover_.showing = false;
            hover_.shownOwner = nullptr;
            return;
        }
        if (hover_.owner != nullptr && hover_.owner == hover_.shownOwner) {
            PopupOptions options;
            options.owner = hover_.owner;
            options.autoHide = true;
            options.hideOnPress = true;
            showPopupNear(hover_.spec.content, hover_.owner, Side::Bottom, options);
            return;
        }
    }
    if (hover_.owner != nullptr && hover_.armed && hover_.spec.content &&
        now - hover_.enteredAt >= hover_.spec.showDelay) {
        PopupOptions options;
        options.owner = hover_.owner;
        options.autoHide = true;
        options.hideOnPress = true;
        showPopupNear(hover_.spec.content, hover_.owner, Side::Bottom, options);
        hover_.showing = true;
        hover_.shownOwner = hover_.owner;
        hover_.shownAt = now;
    }
}

}  // namespace jadefx

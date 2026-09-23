#include "jadefx/scene/Scene.hpp"

#include "jadefx/scene/layout/StackPane.hpp"

#include <algorithm>
#include <chrono>

namespace jadefx {
namespace {

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

Scene::Scene(std::shared_ptr<Node> root, double prefWidth, double prefHeight)
    : requestedWidth_(prefWidth), requestedHeight_(prefHeight) {
    scene_ = this;
    setBackground(Color::rgb8(248, 248, 248));
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
    inherited.color = Color::black();
    inherited.fontSize = 16.f;
    inherited.fontFamily = "Open Sans";
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
}

void Scene::noteMove(double x, double y) {
    pointerX_ = x;
    pointerY_ = y;
    pointerValid_ = true;
    Node* hit = pick(x, y);
    syncHover(hit);
    MouseEvent event;
    event.x = x;
    event.y = y;
    if (pressedTarget_ != nullptr) {
        event.target = pressedTarget_;
        pressedTarget_->handleMouseDragged(event);
    }
    if (hit != nullptr) {
        event.target = hit;
        hit->handleMouseMoved(event);
    }
    updateHoverPopup(hit);
}

void Scene::noteButton(int button, bool down, double x, double y) {
    noteMove(x, y);
    if (button != 0) {
        return;
    }
    MouseEvent event;
    event.x = x;
    event.y = y;
    event.button = button;
    if (down) {
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
        setPressedChain(pressedTarget_);
        clearFocus();
        markFocused(pressedTarget_);
        focused_ = pressedTarget_;
        if (pressedTarget_ != nullptr) {
            event.target = pressedTarget_;
            pressedTarget_->handleMousePressed(event);
            fireMouse(&Node::onPressed_, event);
        }
        return;
    }

    Node* released = pick(x, y);
    Node* pressed = pressedTarget_;
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
    keyMods_ = mods;
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
    const std::vector<HookRecord> hooks = keyHooks_;
    for (const HookRecord& hook : hooks) {
        if (hook.hook) {
            hook.hook(event);
        }
        if (event.consumed) {
            return true;
        }
    }
    if (event.pressed && event.key == Key::Escape) {
        for (auto it = popups_.rbegin(); it != popups_.rend(); ++it) {
            if (it->node && (it->autoHide || it->hideOnPress)) {
                hidePopup(it->node.get());
                return true;
            }
        }
    }
    for (Node* node = focused_; node != nullptr; node = node->getParent()) {
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

void Scene::requestFocus(Node* node) {
    clearFocus();
    focused_ = nullptr;
    if (node == nullptr) {
        return;
    }
    if (node != this && (node->getScene() != this || node->isDisabled())) {
        return;
    }
    markFocused(node);
    focused_ = node;
}

void Scene::releaseFocus(Node* node) {
    if (focused_ == nullptr || node == nullptr) {
        return;
    }
    for (Node* cursor = focused_; cursor != nullptr; cursor = cursor->getParent()) {
        if (cursor == node) {
            clearFocus();
            focused_ = nullptr;
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
    ComputedStyle pass;
    pass.color = computed_.color;
    pass.fontSize = computed_.fontSize > 0.f ? computed_.fontSize : 16.f;
    pass.fontFamily = computed_.fontFamily.empty() ? "Open Sans" : computed_.fontFamily;
    pass.subpixel = computed_.subpixel;
    popup.node->applyStyles(pass, lastTime_);
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
    record->owner = options.owner;
    record->autoHide = options.autoHide;
    record->hideOnPress = options.hideOnPress;
    record->modal = options.modal;
    record->fillScene = options.fillScene;
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
    record->node->performLayout(x, y, std::max(0.0, popupWidth), std::max(0.0, popupHeight));
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

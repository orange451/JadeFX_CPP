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

Scene::~Scene() = default;

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
}

void Scene::noteMove(double x, double y) {
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
    if (node != this && node->getScene() != this) {
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

}  // namespace jadefx

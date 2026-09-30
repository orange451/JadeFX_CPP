#include "jadefx/event/DragEvent.hpp"
#include "jadefx/scene/Scene.hpp"
#include "jadefx/scene/image/ImageView.hpp"

#include <algorithm>

// Drag and drop: the data types, and the scene's side of the gesture.
namespace jadefx {
namespace {

// How translucent the view under the pointer is.
constexpr float kViewOpacity = 0.75f;

bool Contains(const std::vector<Node*>& nodes, const Node* node) {
    return std::find(nodes.begin(), nodes.end(), node) != nodes.end();
}

}  // namespace

std::string ClipboardContent::get(const std::string& format) const {
    const auto found = data_.find(format);
    return found != data_.end() ? found->second : std::string();
}

std::vector<std::string> ClipboardContent::getTypes() const {
    std::vector<std::string> types;
    for (const auto& entry : data_) {
        types.push_back(entry.first);
    }
    if (!files_.empty()) {
        types.push_back("Files");
    }
    return types;
}

void Dragboard::setDragView(std::shared_ptr<Image> image, double offsetX, double offsetY) {
    setDragView(image ? std::make_shared<ImageView>(std::move(image)) : nullptr, offsetX, offsetY);
}

void Dragboard::setDragView(std::shared_ptr<Node> view, double offsetX, double offsetY) {
    view_ = std::move(view);
    offsetX_ = offsetX;
    offsetY_ = offsetY;
}

void DragEvent::acceptTransferModes(TransferModes modes) {
    const TransferModes allowed = dragboard != nullptr ? dragboard->getTransferModes() & modes : modes;
    if (allowed.empty()) {
        return;
    }
    // The shortcut key asks for a copy, and with Shift for a link, as system file managers do.
    TransferMode chosen = TransferMode::Move;
    if (shortcut && shift && allowed.contains(TransferMode::Link)) {
        chosen = TransferMode::Link;
    } else if (shortcut && allowed.contains(TransferMode::Copy)) {
        chosen = TransferMode::Copy;
    } else {
        for (const TransferMode mode : {TransferMode::Move, TransferMode::Copy, TransferMode::Link}) {
            if (allowed.contains(mode)) {
                chosen = mode;
                break;
            }
        }
    }
    accepted_ = true;
    mode_ = chosen;
    gestureTarget = target;
}

Dragboard* Node::startDragAndDrop(TransferModes modes) {
    return scene_ != nullptr ? scene_->beginDrag(this, modes) : nullptr;
}

Dragboard* Scene::beginDrag(Node* source, TransferModes modes) {
    // Only from drag-detected, and only on the pressed node or one of its ancestors.
    if (detecting_ == nullptr || source == nullptr || !(source == detecting_ || source->isAncestorOf(detecting_))) {
        return nullptr;
    }
    if (drag_ == nullptr) {
        drag_ = std::make_unique<DragState>();
        drag_->board = Dragboard(modes);
        drag_->source = source;
    }
    return &drag_->board;
}

void Scene::detectDrag(const MouseEvent& pressed) {
    MouseEvent event = pressed;
    detecting_ = pressedTarget_;
    for (Node* node = pressedTarget_; node != nullptr && drag_ == nullptr; node = node->getParent()) {
        event.target = node;
        node->handleDragDetected(event);
        if (node->drag_.detected && drag_ == nullptr) {
            node->drag_.detected(event);
        }
    }
    detecting_ = nullptr;
    if (drag_ != nullptr) {
        // The press belongs to the drag now, as in JavaFX: the source hears no release,
        // even when the drag is cancelled before the button comes up.
        setPressedChain(nullptr);
        pressedTarget_ = nullptr;
        syncHover(nullptr);
    }
}

DragEvent Scene::makeDragEvent(DragState& drag, double x, double y, Node* source) const {
    DragEvent event;
    event.x = x;
    event.y = y;
    event.dragboard = &drag.board;
    event.gestureSource = source;
    event.gestureTarget = drag.acceptor;
    event.shortcut = (keyMods_ & (Key::ModControl | Key::ModSuper)) != 0;
    event.shift = (keyMods_ & Key::ModShift) != 0;
    return event;
}

void Scene::updateDrag(DragState& drag, double x, double y, Node* source) {
    Node* hit = pick(x, y);
    std::vector<Node*> chain;
    for (Node* node = hit; node != nullptr; node = node->getParent()) {
        chain.push_back(node);
    }
    // Exited from the deepest node out, then entered from the outermost in, as hover does.
    const std::vector<Node*> previous = drag.entered;
    drag.entered = chain;
    for (Node* node : previous) {
        if (!Contains(chain, node)) {
            DragEvent event = makeDragEvent(drag, x, y, source);
            event.target = node;
            node->handleDragExited(event);
            if (node->drag_.exited) {
                node->drag_.exited(event);
            }
        }
    }
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        if (!Contains(previous, *it)) {
            DragEvent event = makeDragEvent(drag, x, y, source);
            event.target = *it;
            (*it)->handleDragEntered(event);
            if ((*it)->drag_.entered) {
                (*it)->drag_.entered(event);
            }
        }
    }
    // A target accepts afresh on every over, so a drag that leaves it is refused again.
    drag.acceptor = nullptr;
    DragEvent over = makeDragEvent(drag, x, y, source);
    for (Node* node = hit; node != nullptr && !over.consumed; node = node->getParent()) {
        over.target = node;
        node->handleDragOver(over);
        if (node->drag_.over && !over.consumed) {
            node->drag_.over(over);
        }
    }
    if (over.accepted_) {
        drag.acceptor = over.gestureTarget;
        drag.mode = over.mode_;
    }
    if (const std::shared_ptr<Node>& view = drag.board.getDragView()) {
        if (drag.view != view) {
            drag.view = view;
            view->setMouseTransparent(true);
            view->setOpacity(kViewOpacity);
        }
        PopupOptions options;
        options.autoHide = false;
        showPopup(view, x - drag.board.getDragViewOffsetX(), y - drag.board.getDragViewOffsetY(), -1, -1, options);
    }
}

bool Scene::finishDrag(double x, double y, bool drop) {
    if (drag_ == nullptr || drag_->ending) {
        return false;
    }
    // The state stays in place while the nodes hear the end, so a node the handlers
    // remove is forgotten here too; the source and the entered nodes are read afresh.
    DragState* const drag = drag_.get();
    drag->ending = true;
    bool completed = false;
    if (drop && drag->acceptor != nullptr) {
        DragEvent dropped = makeDragEvent(*drag, x, y, drag->source);
        dropped.accepted_ = true;
        dropped.mode_ = drag->mode;
        for (Node* node = pick(x, y); node != nullptr && !dropped.consumed; node = node->getParent()) {
            dropped.target = node;
            node->handleDragDropped(dropped);
            if (node->drag_.dropped && !dropped.consumed) {
                node->drag_.dropped(dropped);
            }
        }
        completed = dropped.completed_;
    }
    while (!drag->entered.empty()) {
        Node* node = drag->entered.front();
        drag->entered.erase(drag->entered.begin());
        DragEvent event = makeDragEvent(*drag, x, y, drag->source);
        event.target = node;
        node->handleDragExited(event);
        if (node->drag_.exited) {
            node->drag_.exited(event);
        }
    }
    if (drag->view != nullptr) {
        hidePopup(drag->view.get());
    }
    if (Node* source = drag->source) {
        DragEvent done = makeDragEvent(*drag, x, y, source);
        done.accepted_ = completed;
        done.mode_ = drag->mode;
        for (Node* node = source; node != nullptr && !done.consumed; node = node->getParent()) {
            done.target = node;
            node->handleDragDone(done);
            if (node->drag_.done && !done.consumed) {
                node->drag_.done(done);
            }
        }
    }
    drag_.reset();
    return completed;
}

bool Scene::noteFileDrop(double x, double y, std::vector<std::string> paths) {
    if (paths.empty() || drag_ != nullptr) {
        return false;
    }
    pointerX_ = x;
    pointerY_ = y;
    pointerValid_ = true;
    drag_ = std::make_unique<DragState>();
    drag_->board.putFiles(std::move(paths));
    updateDrag(*drag_, x, y, nullptr);
    return finishDrag(x, y, true);
}

void Scene::forgetNode(Node* node) {
    if (pressedTarget_ == node) {
        pressedTarget_ = nullptr;
    }
    for (Node*& held : heldButtonTargets_) {
        if (held == node) {
            held = nullptr;
        }
    }
    if (detecting_ == node) {
        detecting_ = nullptr;
    }
    if (drag_ == nullptr) {
        return;
    }
    if (drag_->source == node) {
        drag_->source = nullptr;
    }
    if (drag_->acceptor == node) {
        drag_->acceptor = nullptr;
    }
    drag_->entered.erase(std::remove(drag_->entered.begin(), drag_->entered.end(), node), drag_->entered.end());
}

}  // namespace jadefx

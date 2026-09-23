#include "jadefx/scene/controls/TreeItem.hpp"

#include <utility>

namespace jadefx {

TreeItem::TreeItem() : TreeItem(std::string()) {}

TreeItem::TreeItem(std::string value) : TreeItem(std::move(value), nullptr) {}

TreeItem::TreeItem(std::string value, std::shared_ptr<Node> graphic)
    : value_(std::move(value)), graphic_(std::move(graphic)) {
    children_.setIndexedAddCallback([this](std::shared_ptr<TreeItem> child, std::size_t index) {
        onAdded(std::move(child), index);
    });
    children_.setIndexedRemoveCallback([this](std::shared_ptr<TreeItem> child, std::size_t) {
        onRemoved(child);
    });
}

TreeItem::~TreeItem() {
    structure_ = nullptr;
    onExpanded_ = nullptr;
    onCollapsed_ = nullptr;
    children_.setIndexedAddCallback(nullptr);
    children_.setIndexedRemoveCallback(nullptr);
    for (const std::shared_ptr<TreeItem>& child : children_.items()) {
        if (child && child->parent_ == this) {
            child->parent_ = nullptr;
        }
    }
    children_.clear();
}

void TreeItem::setValue(std::string value) {
    if (value_ == value) {
        return;
    }
    value_ = std::move(value);
    notifyStructure();
}

void TreeItem::setGraphic(std::shared_ptr<Node> graphic) {
    if (graphic_ == graphic) {
        return;
    }
    graphic_ = std::move(graphic);
    notifyStructure();
}

void TreeItem::setExpanded(bool expanded) {
    if (expanded_ == expanded) {
        return;
    }
    expanded_ = expanded;
    if (!isLeaf()) {
        if (expanded_ && onExpanded_) {
            onExpanded_(*this);
        }
        if (!expanded_ && onCollapsed_) {
            onCollapsed_(*this);
        }
    }
    notifyStructure();
}

bool TreeItem::isLeaf() const {
    for (const std::shared_ptr<TreeItem>& child : children_.items()) {
        if (child) {
            return false;
        }
    }
    return true;
}

TreeItem* TreeItem::nextSibling() const {
    if (parent_ == nullptr) {
        return nullptr;
    }
    const std::vector<std::shared_ptr<TreeItem>>& items = parent_->children_.items();
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i].get() != this) {
            continue;
        }
        if (i + 1 < items.size()) {
            return items[i + 1].get();
        }
        return nullptr;
    }
    return nullptr;
}

TreeItem* TreeItem::previousSibling() const {
    if (parent_ == nullptr) {
        return nullptr;
    }
    const std::vector<std::shared_ptr<TreeItem>>& items = parent_->children_.items();
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i].get() != this) {
            continue;
        }
        if (i == 0) {
            return nullptr;
        }
        return items[i - 1].get();
    }
    return nullptr;
}

void TreeItem::removeItem(TreeItem* child) {
    children_.removeIf([child](const std::shared_ptr<TreeItem>& item) { return item.get() == child; });
}

void TreeItem::onAdded(std::shared_ptr<TreeItem> child, std::size_t index) {
    if (!child || child.get() == this || child->reaches(this)) {
        children_.removeAt(index);
        return;
    }
    if (child->parent_ == this) {
        int copies = 0;
        for (const std::shared_ptr<TreeItem>& item : children_.items()) {
            if (item.get() == child.get()) {
                ++copies;
            }
        }
        if (copies > 1) {
            children_.removeAt(index);
        }
        return;
    }
    if (child->parent_ != nullptr) {
        child->parent_->removeItem(child.get());
    }
    child->parent_ = this;
    notifyStructure();
}

void TreeItem::onRemoved(const std::shared_ptr<TreeItem>& child) {
    if (!child) {
        return;
    }
    if (child->parent_ == this && !lists(child.get())) {
        child->parent_ = nullptr;
    }
    notifyStructure();
}

void TreeItem::notifyStructure() {
    if (structure_) {
        structure_();
    } else if (parent_ != nullptr) {
        parent_->notifyStructure();
    }
}

bool TreeItem::lists(const TreeItem* item) const {
    for (const std::shared_ptr<TreeItem>& child : children_.items()) {
        if (child.get() == item) {
            return true;
        }
    }
    return false;
}

bool TreeItem::reaches(const TreeItem* target) const {
    if (target == nullptr) {
        return false;
    }
    if (this == target) {
        return true;
    }
    for (const std::shared_ptr<TreeItem>& child : children_.items()) {
        if (child && child->reaches(target)) {
            return true;
        }
    }
    return false;
}

}  // namespace jadefx

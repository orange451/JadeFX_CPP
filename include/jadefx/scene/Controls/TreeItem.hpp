#pragma once

#include "jadefx/collections/ObservableList.hpp"
#include "jadefx/scene/Node.hpp"

#include <functional>
#include <memory>
#include <string>

namespace jadefx {

class TreeView;

// One row in a TreeView. A TreeItem is not a node: it holds a label, an optional
// graphic, and child items. The view draws a row for each expanded item.
// A leaf has no children, so it draws no disclosure arrow and ignores expand clicks.
class TreeItem {
public:
    TreeItem();
    explicit TreeItem(std::string value);
    TreeItem(std::string value, std::shared_ptr<Node> graphic);
    ~TreeItem();

    TreeItem(const TreeItem&) = delete;
    TreeItem& operator=(const TreeItem&) = delete;

    void setValue(std::string value);
    const std::string& getValue() const { return value_; }

    void setGraphic(std::shared_ptr<Node> graphic);
    std::shared_ptr<Node> getGraphic() const { return graphic_; }

    ObservableList<std::shared_ptr<TreeItem>>& getChildren() { return children_; }
    const ObservableList<std::shared_ptr<TreeItem>>& getChildren() const { return children_; }

    TreeItem* getParent() const { return parent_; }

    // Expanded branches show their children. The flag is kept on a leaf so children
    // added later appear immediately, but a leaf does not fire the expand callbacks.
    void setExpanded(bool expanded);
    bool isExpanded() const { return expanded_; }
    bool isLeaf() const;

    TreeItem* nextSibling() const;
    TreeItem* previousSibling() const;

    void setOnExpanded(std::function<void(TreeItem&)> handler) { onExpanded_ = std::move(handler); }
    void setOnCollapsed(std::function<void(TreeItem&)> handler) { onCollapsed_ = std::move(handler); }

private:
    friend class TreeView;

    void setStructureListener(std::function<void()> listener) { structure_ = std::move(listener); }
    void removeItem(TreeItem* child);
    void onAdded(std::shared_ptr<TreeItem> child, std::size_t index);
    void onRemoved(const std::shared_ptr<TreeItem>& child);
    void notifyStructure();
    bool lists(const TreeItem* item) const;
    bool reaches(const TreeItem* target) const;

    std::string value_;
    std::shared_ptr<Node> graphic_;
    ObservableList<std::shared_ptr<TreeItem>> children_;
    TreeItem* parent_ = nullptr;
    bool expanded_ = false;
    std::function<void()> structure_;
    std::function<void(TreeItem&)> onExpanded_;
    std::function<void(TreeItem&)> onCollapsed_;
};

}  // namespace jadefx

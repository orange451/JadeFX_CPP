#pragma once

#include "jadefx/scene/controls/Controls.hpp"
#include "jadefx/scene/controls/TreeItem.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace jadefx {

class TreeScrollBar;

// Single keeps one row selected. Multiple adds Ctrl or Command and a click to
// add or remove a row, and Shift and a click, or Shift and an arrow key, to
// select every row from the anchor to that one.
enum class SelectionMode { Single, Multiple };

// Where dragged rows would land against the row under the pointer. The top and
// bottom quarters of a row are Before and After it, as its sibling. The middle
// half is Into it, as its last child.
enum class TreeDropPosition { Before, Into, After };

// One drop, while it is dragged and when it lands. items are the dragged rows in
// row order, with any row whose ancestor is also dragged left out.
struct TreeDrop {
    std::vector<TreeItem*> items;
    TreeItem* target = nullptr;
    TreeDropPosition position = TreeDropPosition::Into;

    // The item the rows would become children of: target for Into, else its parent.
    TreeItem* parent() const;
};

// Material tree, in the shape of JFoenix's JFXTreeView.
// Rows indent by level. A branch draws a disclosure arrow that turns down when
// the branch is open. The selected row keeps a thin bar on its left edge.
// A click selects the row. The arrow, or a second click on the row, opens or
// closes a branch. A double-click handler can take that second click instead.
// A right-click selects the row and asks for a context menu; in Multiple mode a
// right-click on a selected row keeps the others. Arrow keys move the selection. The wheel and trackpad scroll
// in pixels when the rows are taller than the view, so a slow swipe still moves.
// The scrollbar matches StyledTextArea: drag the thumb, or click the track to page.
class TreeView : public Controls {
    friend class TreeScrollBar;

public:
    TreeView();
    explicit TreeView(std::shared_ptr<TreeItem> root);
    ~TreeView() override;

    const char* getElementType() const override { return "treeview"; }

    void setRoot(std::shared_ptr<TreeItem> root);
    std::shared_ptr<TreeItem> getRoot() const;

    // When this is false the root stays in the model and its children are the
    // top rows. Those children stay hidden while the root is collapsed.
    void setShowRoot(bool show);
    bool isShowRoot() const;

    // Extra left inset for each level under the first visible row. JavaFX uses 10.
    void setIndent(double indent);
    double getIndent() const;

    // Height of every row. The default is 32.
    void setFixedCellSize(double size);
    double getFixedCellSize() const;

    // The selected row's left bar. JFXTreeCell starts this at red.
    void setSelectionBarColor(const Color& color);
    Color getSelectionBarColor() const;

    int getExpandedItemCount() const;
    TreeItem* getTreeItem(int row) const;
    int getRow(const TreeItem* item) const;

    void setSelectionMode(SelectionMode mode);
    SelectionMode getSelectionMode() const;

    // The last row picked. In Multiple mode it is one of getSelectedItems.
    TreeItem* getSelectedItem() const;
    int getSelectedIndex() const;
    // Every selected row, in the order it was picked. The last is getSelectedItem.
    std::vector<TreeItem*> getSelectedItems() const;
    bool isSelected(const TreeItem* item) const;
    // select replaces the whole selection with one row, and makes it the anchor.
    void select(TreeItem* item);
    void select(int row);
    // Replaces the selection. A repeat keeps its first place, and an item not
    // in this tree is skipped. The last item becomes the selected item and the
    // anchor. Single mode keeps only the last.
    void selectItems(const std::vector<TreeItem*>& items);
    void clearSelection();
    // The selected item changed. The argument is getSelectedItem.
    void setOnSelectionChanged(std::function<void(TreeItem*)> handler);
    // The set of selected items changed, from a click, a key, a call, or a row
    // leaving the tree. Read getSelectedItems.
    void setOnSelectedItemsChanged(std::function<void()> handler);
    // The row drawing item, or null when that row is not on screen.
    Node* getCell(const TreeItem* item) const;

    // Right-click on a row. The view has already selected that row.
    void setOnContextMenuRequested(std::function<void(TreeItem&, const MouseEvent&)> handler);
    // Double-click. Return true to keep a branch from opening or closing.
    void setOnItemActivated(std::function<bool(TreeItem&)> handler);
    // Drawn on the right of the row under the pointer. Hidden when no row is
    // hovered. The pointer can rest on the node without that row losing it.
    // Null clears it. A click belongs to the node, not the row.
    void setHoverAccessory(std::shared_ptr<Node> node);
    // The row the accessory is standing on, or null.
    TreeItem* getHoveredItem() const;
    // The row calls these. contextMenuRequested selects the item first.
    void contextMenuRequested(TreeItem& item, const MouseEvent& event);
    bool itemActivated(TreeItem& item);
    // A left click on the row with these Key::Mod bits. False when the click
    // only changed the selection, so it cannot start a double-click.
    bool rowClicked(TreeItem& item, int mods);
    // The pointer moved with the left button down on item's row.
    void dragRow(TreeItem& item, const MouseEvent& event);
    // The left button came up. True when it ended a drag, or a drag Escape
    // cancelled, so the click that follows is not a click.
    bool releaseRow();

    // Row drag and drop, in the shape of a JavaFX cell factory's onDragDetected,
    // onDragOver, and onDragDropped. It is off until a drop handler is set, so a
    // tree that never asks for it keeps click and drag doing nothing.
    // A left press that moves a few points starts the drag. A selected row drags
    // the whole selection; any other row is selected first and drags alone.
    // While it moves, the view marks the landing place: a line between rows for
    // Before and After, or a box around the row for Into. Near the top or bottom
    // edge the rows scroll, and a closed branch held under Into opens. Where
    // the last row of a branch meets a shallower one, the pointer's distance
    // from the left picks which level the line is at. Escape cancels.
    // The view never moves items itself. The handler changes the model, and
    // the model's change moves the rows.
    void setOnItemsDropped(std::function<void(const TreeDrop&)> handler);
    // Asked for each landing place the pointer finds. False shows no marker
    // and the release drops nothing. A place inside or beside a dragged row, or
    // beside a row with no parent, is refused before this is asked.
    void setDropAcceptor(std::function<bool(const TreeDrop&)> acceptor);
    // True from the moment a row drag starts until its release or Escape.
    bool isDraggingItems() const;
    // The landing place the release would drop on. Empty items when there is none.
    TreeDrop getPendingDrop() const;

    // scrollTo(TreeItem) opens ancestors so the row can move into view.
    void scrollTo(int row);
    void scrollTo(const TreeItem* item);

    void handleScroll(ScrollEvent& event) override;
    void handleKey(KeyEvent& event) override;
    // NotAllowed while a drag has no landing place.
    Cursor cursorAt(double x, double y) const override;

protected:
    void layoutChildren() override;
    void visitChildren(const std::function<void(Node*)>& visitor) override;
    void detachChild(Node* child) override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;
    void renderChildren(UiRenderer& renderer, float opacity) override;

private:
    struct Impl;

    void watchRoot();
    void requestSync();
    void rebuild();
    void refreshChrome();
    void selectPointer(TreeItem* item);
    // Replaces the selection. primary is the selected item; null picks the last.
    void applySelection(std::vector<std::shared_ptr<TreeItem>> items, std::shared_ptr<TreeItem> primary);
    // Visible rows from the anchor through item, ending at item.
    std::vector<std::shared_ptr<TreeItem>> rangeTo(TreeItem* item) const;
    void extendTo(TreeItem* item, bool keep);
    void moveSelection(int delta, bool extend = false);
    // Selects row, or with extend in Multiple mode, the rows from the anchor to it.
    void pickRow(int row, bool extend);
    void revealRow(int row);
    bool containsItem(const TreeItem* item) const;
    int shownLevel(const TreeItem* item) const;
    double rowSize() const;
    void eachVisible(const std::function<void(TreeItem*)>& visit) const;
    std::shared_ptr<TreeItem> findShared(const TreeItem* item) const;
    void cancelDrag();
    std::vector<std::shared_ptr<TreeItem>> draggedItems(TreeItem& grabbed);
    // Finds the landing place under a point in window points.
    void aimDrop(double x, double y);
    bool dropAllowed(const TreeDrop& drop) const;
    // Opens a closed branch held under Into. Runs before the rows are laid out.
    void springOpen();
    // Re-aims at the pointer and scrolls near the edges. Runs after layout.
    void tickDrag();
    void renderDropMarker(UiRenderer& renderer, float opacity);
    void pressScrollBar(const MouseEvent& event);
    void dragScrollBar(const MouseEvent& event);
    void releaseScrollBar();

    std::unique_ptr<Impl> impl_;
};

}  // namespace jadefx

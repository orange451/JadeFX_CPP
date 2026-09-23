#pragma once

#include "jadefx/scene/Controls/Controls.hpp"
#include "jadefx/scene/Controls/TreeItem.hpp"

#include <functional>
#include <memory>

namespace jadefx {

class TreeScrollBar;

// Material tree, in the shape of JFoenix's JFXTreeView.
// Rows indent by level. A branch draws a disclosure arrow that turns down when
// the branch is open. The selected row keeps a thin bar on its left edge.
// A click selects the row. The arrow, or a second click on the row, opens or
// closes a branch. Arrow keys move the selection. The wheel and trackpad scroll
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

    TreeItem* getSelectedItem() const;
    int getSelectedIndex() const;
    void select(TreeItem* item);
    void select(int row);
    void clearSelection();
    void setOnSelectionChanged(std::function<void(TreeItem*)> handler);

    // scrollTo(TreeItem) opens ancestors so the row can move into view.
    void scrollTo(int row);
    void scrollTo(const TreeItem* item);

    void handleScroll(ScrollEvent& event) override;
    void handleKey(KeyEvent& event) override;

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
    void moveSelection(int delta);
    void revealRow(int row);
    bool containsItem(const TreeItem* item) const;
    int shownLevel(const TreeItem* item) const;
    double rowSize() const;
    void eachVisible(const std::function<void(TreeItem*)>& visit) const;
    std::shared_ptr<TreeItem> findShared(const TreeItem* item) const;
    void pressScrollBar(const MouseEvent& event);
    void dragScrollBar(const MouseEvent& event);
    void releaseScrollBar();

    std::unique_ptr<Impl> impl_;
};

}  // namespace jadefx

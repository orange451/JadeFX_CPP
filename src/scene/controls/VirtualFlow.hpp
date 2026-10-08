#pragma once

#include "jadefx/scene/controls/IndexedCell.hpp"
#include "jadefx/scene/layout/Region.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace jadefx {

class ScrollBar;
namespace scroll {
class ClipRegion;
}

// The virtualized body of ListView and TableView, in the shape of OpenJFX
// VirtualFlow. It keeps a cell only for each row on screen and moves cells to
// new rows as it scrolls, so a million rows cost what a screenful does.
// Every row is one length along the scroll axis: fixedCellSize, or the
// preferred length of the first row on screen. Across it, cells are as broad as
// the viewport, their widest preferred breadth, or fixedCellBreadth.
// A cell keeps its row while that row stays on screen, so its hover, focus, and
// editor survive a scroll. The binder moves a cell to a row, or to -1 to park it.
class VirtualFlow : public Region {
public:
    using CellFactory = std::function<std::shared_ptr<IndexedCell>()>;
    using CellBinder = std::function<void(IndexedCell& cell, int index)>;

    VirtualFlow();
    ~VirtualFlow() override;

    const char* getElementType() const override { return "virtual-flow"; }

    // Replacing the factory drops every cell; the next layout makes new ones.
    void setCellFactory(CellFactory factory);
    void setCellBinder(CellBinder binder) { binder_ = std::move(binder); }

    void setCellCount(int count);
    int getCellCount() const { return count_; }

    // Vertical rows stack downward; horizontal ones run left to right.
    void setVertical(bool vertical);
    bool isVertical() const { return vertical_; }
    // Zero or less measures the first row on screen. Only layoutChildren reads
    // this, so arranging again is enough.
    void setFixedCellSize(double size) { setLayoutValue(fixedSize_, size, LayoutDirt::Arrange); }
    // Zero or less makes cells as broad as the viewport or their widest preference.
    void setFixedCellBreadth(double breadth) { setLayoutValue(fixedBreadth_, breadth, LayoutDirt::Arrange); }

    // Rebinds every cell on screen at the next layout, for rows whose items changed.
    void refresh() {
        if (rebindAll_) {
            return;
        }
        rebindAll_ = true;
        markLayoutDirty(LayoutDirt::Arrange);
    }

    // Moves so row index is the first on screen, as far as the end allows.
    void scrollTo(int index);
    // Moves the least distance that shows all of row index.
    void show(int index);
    // Moves along the scroll axis by points. False when it could not move.
    bool scrollPixels(double delta);
    // How far the rows have scrolled along the scroll axis, in points.
    double getPosition() const { return position_; }
    // How far the cells have scrolled across the scroll axis, in points.
    double getBreadthOffset() const { return breadthOffset_; }
    // Moves across the scroll axis the least distance that shows start to end.
    void showBreadth(double start, double end);
    // Runs after getBreadthOffset changes, so a table header can follow the rows.
    void setOnBreadthOffsetChanged(std::function<void()> handler) { onBreadth_ = std::move(handler); }

    // The row length and the viewport's length as of the last layout.
    double getCellLength() const { return cellLength_; }
    double getViewportLength() const { return viewportLength_; }
    double getViewportBreadth() const { return viewportBreadth_; }
    // Rows at least partly on screen as of the last layout. -1 when there are none.
    int getFirstVisibleIndex() const;
    int getLastVisibleIndex() const;

    // The cell showing a row, or null when that row is off screen.
    IndexedCell* getVisibleCell(int index) const;
    void forEachVisibleCell(const std::function<void(IndexedCell&)>& visit) const;

    void handleScroll(ScrollEvent& event) override;

protected:
    void layoutChildren() override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;

private:
    struct Slot {
        int index = -1;
        std::shared_ptr<IndexedCell> cell;
    };

    // Gives each row from first to last a cell, keeping cells whose row stays.
    void bindRange(int first, int last);
    void park(Slot& slot);
    std::shared_ptr<IndexedCell> takeCell();
    double measureCellLength(double breadth);
    double maxPosition() const;
    double along(Size size) const { return vertical_ ? size.height : size.width; }
    double across(Size size) const { return vertical_ ? size.width : size.height; }
    Size sized(double alongLength, double acrossLength) const {
        return vertical_ ? Size{acrossLength, alongLength} : Size{alongLength, acrossLength};
    }
    void setBreadthOffset(double offset);

    std::shared_ptr<scroll::ClipRegion> sheet_;
    std::shared_ptr<ScrollBar> hbar_;
    std::shared_ptr<ScrollBar> vbar_;
    CellFactory factory_;
    CellBinder binder_;
    std::vector<Slot> active_;
    std::vector<std::shared_ptr<IndexedCell>> pile_;
    int count_ = 0;
    bool vertical_ = true;
    bool rebindAll_ = false;
    double fixedSize_ = 0;
    double fixedBreadth_ = 0;
    double position_ = 0;
    double breadthOffset_ = 0;
    double cellLength_ = 24;
    double viewportLength_ = 0;
    double viewportBreadth_ = 0;
    double contentBreadth_ = 0;
    // A scrollTo or show waiting for a layout that knows the row length.
    int pendingTop_ = -1;
    int pendingShow_ = -1;
    std::function<void()> onBreadth_;
};

}  // namespace jadefx

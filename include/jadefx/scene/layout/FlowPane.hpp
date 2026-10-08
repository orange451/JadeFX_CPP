#pragma once

#include "jadefx/scene/layout/Pane.hpp"

#include <vector>

namespace jadefx {

// Children in runs that wrap at the pane's edge, in the shape of OpenJFX FlowPane.
// Horizontal (the default) fills rows left to right and starts a new row when
// the next child would pass the right edge; vertical fills columns top to
// bottom. Children keep their preferred sizes. setAlignment places the runs in
// the pane and each run along it; rowValignment places a child within its row,
// and columnHalignment within its column. The preferred size wraps at
// prefWrapLength. CSS column-gap and row-gap, or gap, win over setHgap and setVgap.
class FlowPane : public Pane {
public:
    FlowPane();
    explicit FlowPane(Orientation orientation);
    FlowPane(double hgap, double vgap);
    FlowPane(Orientation orientation, double hgap, double vgap);

    const char* getElementType() const override { return "flowpane"; }

    void setOrientation(Orientation orientation);
    Orientation getOrientation() const { return orientation_; }
    void setHgap(double gap) { setLayoutValue(hgap_, gap); }
    double getHgap() const { return hgap_; }
    void setVgap(double gap) { setLayoutValue(vgap_, gap); }
    double getVgap() const { return vgap_; }
    // Where the preferred size wraps. The default is 400.
    void setPrefWrapLength(double length) { setLayoutValue(prefWrapLength_, length); }
    double getPrefWrapLength() const { return prefWrapLength_; }
    // Only layoutChildren reads these, not the preferred size, so Arrange is enough.
    void setRowValignment(VPos alignment) { setLayoutValue(rowValignment_, alignment, LayoutDirt::Arrange); }
    VPos getRowValignment() const { return rowValignment_; }
    void setColumnHalignment(HPos alignment) {
        setLayoutValue(columnHalignment_, alignment, LayoutDirt::Arrange);
    }
    HPos getColumnHalignment() const { return columnHalignment_; }

    // Space kept around a child.
    static void setMargin(Node& child, const Insets& margin);
    static Insets getMargin(const Node& child);
    static void clearConstraints(Node& child);

protected:
    void layoutChildren() override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;

private:
    struct Item;
    struct Run;

    bool horizontal() const { return orientation_ == Orientation::Horizontal; }
    double alongGap() const;
    double acrossGap() const;
    // The children in runs, wrapping when a run would pass limit along the flow.
    std::vector<Run> runs(double limit) const;
    // How far the runs reach along the flow and across it, gaps included.
    static double longestRun(const std::vector<Run>& runs);
    double runsBreadth(const std::vector<Run>& runs) const;

    Orientation orientation_ = Orientation::Horizontal;
    double hgap_ = 0;
    double vgap_ = 0;
    double prefWrapLength_ = 400;
    VPos rowValignment_ = VPos::Center;
    HPos columnHalignment_ = HPos::Left;
};

}  // namespace jadefx

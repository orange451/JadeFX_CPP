#include "jadefx/scene/layout/FlowPane.hpp"

#include "LayoutDetail.hpp"

#include <algorithm>

namespace jadefx {
namespace {

constexpr const char* kMargin = "flowpane-margin";

}  // namespace

// A child and its size along the flow and across it, margins included.
struct FlowPane::Item {
    Node* node = nullptr;
    Insets margin;
    double along = 0;
    double across = 0;
};

struct FlowPane::Run {
    std::vector<Item> items;
    double length = 0;
    double breadth = 0;
};

FlowPane::FlowPane() : FlowPane(Orientation::Horizontal, 0, 0) {}

FlowPane::FlowPane(Orientation orientation) : FlowPane(orientation, 0, 0) {}

FlowPane::FlowPane(double hgap, double vgap) : FlowPane(Orientation::Horizontal, hgap, vgap) {}

FlowPane::FlowPane(Orientation orientation, double hgap, double vgap) : hgap_(hgap), vgap_(vgap) {
    setAlignment(Pos::TopLeft);
    setOrientation(orientation);
}

void FlowPane::setOrientation(Orientation orientation) {
    orientation_ = orientation;
    setPseudoState("horizontal", orientation == Orientation::Horizontal);
    setPseudoState("vertical", orientation == Orientation::Vertical);
}

void FlowPane::setMargin(Node& child, const Insets& margin) { layout_detail::SetConstraint(child, kMargin, margin); }

Insets FlowPane::getMargin(const Node& child) {
    return layout_detail::GetConstraint<Insets>(child, kMargin).value_or(Insets{});
}

void FlowPane::clearConstraints(Node& child) { child.getProperties().erase(kMargin); }

double FlowPane::alongGap() const {
    const ComputedStyle& style = computedStyle();
    const float css = horizontal() ? style.columnGap : style.rowGap;
    return css >= 0.f ? static_cast<double>(css) : (horizontal() ? hgap_ : vgap_);
}

double FlowPane::acrossGap() const {
    const ComputedStyle& style = computedStyle();
    const float css = horizontal() ? style.rowGap : style.columnGap;
    return css >= 0.f ? static_cast<double>(css) : (horizontal() ? vgap_ : hgap_);
}

std::vector<FlowPane::Run> FlowPane::runs(double limit) const {
    const double gap = alongGap();
    std::vector<Run> out;
    for (const std::shared_ptr<Node>& child : getChildren()) {
        if (!child || !child->isVisible()) {
            continue;
        }
        Item item;
        item.node = child.get();
        item.margin = getMargin(*child);
        // A percentage width is of the flow's length in a row, and of the pane's width in a column.
        const double width = child->measuredWidth(horizontal() ? limit : contentWidth());
        const double height = child->measuredHeight(width, -1);
        const double fullWidth = width + item.margin.width();
        const double fullHeight = height + item.margin.height();
        item.along = horizontal() ? fullWidth : fullHeight;
        item.across = horizontal() ? fullHeight : fullWidth;
        // A run takes at least one child, however long it is.
        if (out.empty() || (!out.back().items.empty() && out.back().length + gap + item.along > limit)) {
            out.emplace_back();
        }
        Run& run = out.back();
        run.length += (run.items.empty() ? 0.0 : gap) + item.along;
        run.breadth = std::max(run.breadth, item.across);
        run.items.push_back(item);
    }
    return out;
}

double FlowPane::longestRun(const std::vector<Run>& runs) {
    double longest = 0;
    for (const Run& run : runs) {
        longest = std::max(longest, run.length);
    }
    return longest;
}

double FlowPane::runsBreadth(const std::vector<Run>& runs) const {
    double breadth = runs.empty() ? 0.0 : acrossGap() * static_cast<double>(runs.size() - 1);
    for (const Run& run : runs) {
        breadth += run.breadth;
    }
    return breadth;
}

void FlowPane::layoutChildren() {
    const double width = contentWidth();
    const double height = contentHeight();
    const double alongSpace = horizontal() ? width : height;
    const double acrossSpace = horizontal() ? height : width;
    const std::vector<Run> placed = runs(alongSpace);

    const Pos align = usingAlignment();
    const int alongMode = horizontal() ? layout_detail::Horizontal(align) : layout_detail::Vertical(align);
    const int acrossMode = horizontal() ? layout_detail::Vertical(align) : layout_detail::Horizontal(align);
    // A child within its run: rowValignment in rows, columnHalignment in columns.
    const int itemMode = horizontal()
                             ? (rowValignment_ == VPos::Center ? 1 : (rowValignment_ == VPos::Bottom ? 2 : 0))
                             : (columnHalignment_ == HPos::Center ? 1 : (columnHalignment_ == HPos::Right ? 2 : 0));
    const double gap = alongGap();
    double across = layout_detail::Align(acrossSpace, runsBreadth(placed), acrossMode);
    for (const Run& run : placed) {
        double along = layout_detail::Align(alongSpace, run.length, alongMode);
        for (const Item& item : run.items) {
            const double offset = layout_detail::Align(run.breadth, item.across, itemMode);
            const double alongPos = along + (horizontal() ? item.margin.left : item.margin.top);
            const double acrossPos = across + offset + (horizontal() ? item.margin.top : item.margin.left);
            const double itemWidth = (horizontal() ? item.along : item.across) - item.margin.width();
            const double itemHeight = (horizontal() ? item.across : item.along) - item.margin.height();
            const double x = contentLeft() + (horizontal() ? alongPos : acrossPos);
            const double y = contentTop() + (horizontal() ? acrossPos : alongPos);
            item.node->performLayout(x, y, std::max(0.0, itemWidth), std::max(0.0, itemHeight));
            along += item.along + gap;
        }
        across += run.breadth + acrossGap();
    }
}

double FlowPane::preferredContentWidth(double) const {
    const std::vector<Run> wrapped = runs(prefWrapLength_);
    return horizontal() ? longestRun(wrapped) : runsBreadth(wrapped);
}

double FlowPane::preferredContentHeight(double innerWidth) const {
    if (horizontal()) {
        return runsBreadth(runs(innerWidth > 0 ? innerWidth : prefWrapLength_));
    }
    return longestRun(runs(prefWrapLength_));
}

}  // namespace jadefx

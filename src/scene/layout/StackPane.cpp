#include "jadefx/scene/layout/StackPane.hpp"

#include "LayoutDetail.hpp"

namespace jadefx {

StackPane::StackPane() { setAlignment(Pos::Center); }

StackPane::StackPane(std::shared_ptr<Node> child) : StackPane() {
    if (child) {
        getChildren().add(std::move(child));
    }
}

void StackPane::layoutChildren() {
    const double innerWidth = contentWidth();
    const double innerHeight = contentHeight();
    const double left = contentLeft();
    const double top = contentTop();
    const Pos align = usingAlignment();
    const int horizontal = layout_detail::Horizontal(align);
    const int vertical = layout_detail::Vertical(align);
    for (Node* child : layout_detail::ChildPointers(*this)) {
        double width = child->measuredWidth(innerWidth);
        // A line wider than the box has to live in the box, so its label can end with an ellipsis.
        // Minimum width may still extend past the box.
        if (width > innerWidth) {
            const ComputedStyle& style = child->computedStyle();
            const double floor = layout_detail::LowerBound(style.minWidth, innerWidth, style.fontSize);
            width = std::max(innerWidth, floor);
        }
        const double height = child->measuredHeight(width, innerHeight);
        const double x = left + layout_detail::Align(innerWidth, width, horizontal);
        const double y = top + layout_detail::Align(innerHeight, height, vertical);
        child->performLayout(x, y, width, height);
    }
}

}  // namespace jadefx

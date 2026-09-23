#include "jadefx/scene/layout/HBox.hpp"

#include "LayoutDetail.hpp"

namespace jadefx {

double HBox::preferredContentWidth(double innerAvailable) const {
    double fixed = 0;
    double flex = 0;
    std::size_t count = 0;
    for (const std::shared_ptr<Node>& child : getChildren().items()) {
        if (!child) {
            continue;
        }
        const double measured = child->measuredWidth(innerAvailable);
        if (layout_detail::IsFlexible(child->computedStyle().width)) {
            flex += measured;
        } else {
            fixed += measured;
        }
        ++count;
    }
    if (count > 1) {
        fixed += static_cast<double>(computedStyle().spacing) * static_cast<double>(count - 1);
    }
    double width = fixed + flex;
    // Percentage children are of this box, so they must not make the box wider than the space it was offered.
    if (innerAvailable > 0.0 && width > innerAvailable) {
        width = std::max(innerAvailable, fixed);
    }
    return width;
}

double HBox::preferredContentHeight(double innerWidth) const {
    return Pane::preferredContentHeight(innerWidth);
}

void HBox::layoutChildren() {
    const double innerWidth = contentWidth();
    const double innerHeight = contentHeight();
    const Pos align = usingAlignment();
    std::vector<layout_detail::BoxItem> items;
    for (Node* child : layout_detail::ChildPointers(*this)) {
        layout_detail::BoxItem item;
        item.node = child;
        const ComputedStyle& style = child->computedStyle();
        item.width = child->measuredWidth(innerWidth);
        item.flex = layout_detail::IsFlexible(style.width);
        item.floor = layout_detail::LowerBound(style.minWidth, innerWidth, style.fontSize);
        items.push_back(item);
    }
    const double gap = computedStyle().spacing;
    const double gaps = items.size() > 1 ? gap * static_cast<double>(items.size() - 1) : 0.0;
    layout_detail::ShrinkFlex(items, gaps, innerWidth, true);
    double used = gaps;
    for (layout_detail::BoxItem& item : items) {
        item.height = item.node->measuredHeight(item.width, innerHeight);
        used += item.width;
    }
    double x = contentLeft();
    const HPos pack = hpos(align);
    if (pack == HPos::Center) {
        x += (innerWidth - used) * 0.5;
    } else if (pack == HPos::Right) {
        x += innerWidth - used;
    }
    const int vertical = layout_detail::Vertical(align);
    for (const layout_detail::BoxItem& item : items) {
        const double y = contentTop() + layout_detail::Align(innerHeight, item.height, vertical);
        item.node->performLayout(x, y, item.width, item.height);
        x += item.width + gap;
    }
}

}  // namespace jadefx

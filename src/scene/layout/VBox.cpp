#include "jadefx/scene/layout/VBox.hpp"

#include "LayoutDetail.hpp"

#include <vector>

namespace jadefx {

double VBox::preferredContentWidth(double innerAvailable) const {
    return Pane::preferredContentWidth(innerAvailable);
}

double VBox::preferredContentHeight(double innerWidth) const {
    const auto nodes = [this]() {
        std::vector<Node*> result;
        for (const std::shared_ptr<Node>& child : getChildren().items()) {
            if (child) {
                result.push_back(child.get());
            }
        }
        return result;
    }();
    double height = 0;
    for (Node* child : nodes) {
        const double width = child->measuredWidth(innerWidth);
        height += child->measuredHeight(width, -1);
    }
    if (nodes.size() > 1) {
        height += static_cast<double>(computedStyle().spacing) * static_cast<double>(nodes.size() - 1);
    }
    return height;
}

void VBox::layoutChildren() {
    const double innerWidth = contentWidth();
    const double innerHeight = contentHeight();
    const Pos align = usingAlignment();
    std::vector<layout_detail::BoxItem> items;
    for (Node* child : layout_detail::ChildPointers(*this)) {
        layout_detail::BoxItem item;
        item.node = child;
        item.width = child->measuredWidth(innerWidth);
        item.height = child->measuredHeight(item.width, innerHeight);
        const ComputedStyle& style = child->computedStyle();
        item.flex = layout_detail::IsFlexible(style.height);
        item.floor = layout_detail::LowerBound(style.minHeight, innerHeight, style.fontSize);
        items.push_back(item);
    }
    const double gap = computedStyle().spacing;
    const double gaps = items.size() > 1 ? gap * static_cast<double>(items.size() - 1) : 0.0;
    layout_detail::ShrinkFlex(items, gaps, innerHeight, false);
    double used = gaps;
    for (const layout_detail::BoxItem& item : items) {
        used += item.height;
    }
    double y = contentTop();
    const VPos pack = vpos(align);
    if (pack == VPos::Center) {
        y += (innerHeight - used) * 0.5;
    } else if (pack == VPos::Bottom) {
        y += innerHeight - used;
    }
    const int horizontal = layout_detail::Horizontal(align);
    for (const layout_detail::BoxItem& item : items) {
        const double x = contentLeft() + layout_detail::Align(innerWidth, item.width, horizontal);
        item.node->performLayout(x, y, item.width, item.height);
        y += item.height + gap;
    }
}

}  // namespace jadefx

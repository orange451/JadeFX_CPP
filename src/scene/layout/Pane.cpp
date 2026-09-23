#include "jadefx/scene/layout/Pane.hpp"

#include "LayoutDetail.hpp"

namespace jadefx {

double Pane::preferredContentWidth(double innerAvailable) const {
    double width = 0;
    for (const std::shared_ptr<Node>& child : getChildren().items()) {
        if (child) {
            width = std::max(width, child->measuredWidth(innerAvailable));
        }
    }
    return width;
}

double Pane::preferredContentHeight(double innerWidth) const {
    double height = 0;
    for (const std::shared_ptr<Node>& child : getChildren().items()) {
        if (!child) {
            continue;
        }
        const double width = child->measuredWidth(innerWidth);
        height = std::max(height, child->measuredHeight(width, -1));
    }
    return height;
}

void Pane::layoutChildren() {
    const double innerWidth = contentWidth();
    const double innerHeight = contentHeight();
    const double left = contentLeft();
    const double top = contentTop();
    for (Node* child : layout_detail::ChildPointers(*this)) {
        const double width = child->measuredWidth(innerWidth);
        const double height = child->measuredHeight(width, innerHeight);
        child->performLayout(left, top, width, height);
    }
}

}  // namespace jadefx

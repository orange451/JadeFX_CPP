#include "jadefx/scene/layout/BorderPane.hpp"

#include "LayoutDetail.hpp"

#include <utility>

namespace jadefx {

BorderPane::BorderPane() = default;

BorderPane::~BorderPane() {
    auto release = [](std::shared_ptr<Node>& node) {
        if (node) {
            node->setParent(nullptr);
        }
        node.reset();
    };
    release(center_);
    release(top_);
    release(bottom_);
    release(left_);
    release(right_);
}

void BorderPane::setSpacing(double spacing) { regionSpacing_ = spacing; }

void BorderPane::releaseSlot(std::shared_ptr<Node>& slot, Node* child) {
    if (slot.get() != child) {
        return;
    }
    std::shared_ptr<Node> held = std::move(slot);
    held->setParent(nullptr);
}

void BorderPane::detachChild(Node* child) {
    releaseSlot(center_, child);
    releaseSlot(top_, child);
    releaseSlot(bottom_, child);
    releaseSlot(left_, child);
    releaseSlot(right_, child);
    Pane::detachChild(child);
}

void BorderPane::adopt(std::shared_ptr<Node>& slot, std::shared_ptr<Node> node) {
    if (slot.get() == node.get()) {
        return;
    }
    if (node && node->getParent() != nullptr) {
        node->getParent()->detachChild(node.get());
    }
    if (slot) {
        slot->setParent(nullptr);
        slot.reset();
    }
    slot = std::move(node);
    if (slot) {
        slot->setParent(this);
    }
}

void BorderPane::setCenter(std::shared_ptr<Node> node) { adopt(center_, std::move(node)); }
void BorderPane::setTop(std::shared_ptr<Node> node) { adopt(top_, std::move(node)); }
void BorderPane::setBottom(std::shared_ptr<Node> node) { adopt(bottom_, std::move(node)); }
void BorderPane::setLeft(std::shared_ptr<Node> node) { adopt(left_, std::move(node)); }
void BorderPane::setRight(std::shared_ptr<Node> node) { adopt(right_, std::move(node)); }

void BorderPane::visitChildren(const std::function<void(Node*)>& visitor) {
    StackPane::visitChildren(visitor);
    if (top_) {
        visitor(top_.get());
    }
    if (left_) {
        visitor(left_.get());
    }
    if (center_) {
        visitor(center_.get());
    }
    if (right_) {
        visitor(right_.get());
    }
    if (bottom_) {
        visitor(bottom_.get());
    }
}

double BorderPane::preferredContentWidth(double innerAvailable) const {
    double width = Pane::preferredContentWidth(innerAvailable);
    double middle = 0;
    if (left_) {
        middle += left_->measuredWidth(innerAvailable);
    }
    if (center_) {
        middle += center_->measuredWidth(innerAvailable);
    }
    if (right_) {
        middle += right_->measuredWidth(innerAvailable);
    }
    const int gaps = (left_ ? 1 : 0) + (right_ ? 1 : 0);
    middle += regionSpacing_ * static_cast<double>(gaps);
    width = std::max(width, middle);
    if (top_) {
        width = std::max(width, top_->measuredWidth(innerAvailable));
    }
    if (bottom_) {
        width = std::max(width, bottom_->measuredWidth(innerAvailable));
    }
    return width;
}

double BorderPane::preferredContentHeight(double innerWidth) const {
    double height = 0;
    if (top_) {
        height += top_->measuredHeight(top_->measuredWidth(innerWidth), -1);
    }
    if (bottom_) {
        height += bottom_->measuredHeight(bottom_->measuredWidth(innerWidth), -1);
    }
    double middle = center_ ? center_->measuredHeight(center_->measuredWidth(innerWidth), -1) : 0;
    if (left_) {
        middle = std::max(middle, left_->measuredHeight(left_->measuredWidth(innerWidth), -1));
    }
    if (right_) {
        middle = std::max(middle, right_->measuredHeight(right_->measuredWidth(innerWidth), -1));
    }
    height += middle;
    int gaps = 0;
    if (top_) {
        ++gaps;
    }
    if (bottom_) {
        ++gaps;
    }
    height += regionSpacing_ * gaps;
    return height;
}

void BorderPane::layoutChildren() {
    Pane::layoutChildren();

    const double innerWidth = contentWidth();
    const double innerHeight = contentHeight();
    const double left = contentLeft();
    const double top = contentTop();
    const double gap = regionSpacing_;

    double topHeight = top_ ? top_->measuredHeight(innerWidth, innerHeight) : 0;
    double bottomHeight = bottom_ ? bottom_->measuredHeight(innerWidth, innerHeight) : 0;
    const double topFloor = top_ ? layout_detail::LowerBound(top_->computedStyle().minHeight, innerHeight,
                                                             top_->computedStyle().fontSize)
                                 : 0;
    const double bottomFloor = bottom_ ? layout_detail::LowerBound(bottom_->computedStyle().minHeight, innerHeight,
                                                                   bottom_->computedStyle().fontSize)
                                       : 0;
    double middleFloor = 0;
    if (center_) {
        middleFloor = std::max(middleFloor, layout_detail::LowerBound(center_->computedStyle().minHeight, innerHeight,
                                                                      center_->computedStyle().fontSize));
    }
    if (left_) {
        middleFloor = std::max(middleFloor, layout_detail::LowerBound(left_->computedStyle().minHeight, innerHeight,
                                                                      left_->computedStyle().fontSize));
    }
    if (right_) {
        middleFloor = std::max(middleFloor, layout_detail::LowerBound(right_->computedStyle().minHeight, innerHeight,
                                                                      right_->computedStyle().fontSize));
    }
    const int verticalGaps = (top_ ? 1 : 0) + (bottom_ ? 1 : 0);
    const double verticalLimit = std::max(0.0, innerHeight - gap * static_cast<double>(verticalGaps) - middleFloor);
    layout_detail::ShareShrink(topHeight, topFloor, bottomHeight, bottomFloor, verticalLimit);
    const double middleHeight = std::max(0.0, innerHeight - topHeight - bottomHeight - gap * verticalGaps);

    double leftWidth = left_ ? left_->measuredWidth(innerWidth) : 0;
    double rightWidth = right_ ? right_->measuredWidth(innerWidth) : 0;
    const double leftFloor = left_ ? layout_detail::LowerBound(left_->computedStyle().minWidth, innerWidth,
                                                               left_->computedStyle().fontSize)
                                   : 0;
    const double rightFloor = right_ ? layout_detail::LowerBound(right_->computedStyle().minWidth, innerWidth,
                                                                 right_->computedStyle().fontSize)
                                     : 0;
    const double centerFloor = center_ ? layout_detail::LowerBound(center_->computedStyle().minWidth, innerWidth,
                                                                   center_->computedStyle().fontSize)
                                       : 0;
    const int horizontalGaps = (left_ ? 1 : 0) + (right_ ? 1 : 0);
    const double horizontalLimit = std::max(0.0, innerWidth - gap * static_cast<double>(horizontalGaps) - centerFloor);
    layout_detail::ShareShrink(leftWidth, leftFloor, rightWidth, rightFloor, horizontalLimit);
    const double middleWidth = std::max(0.0, innerWidth - leftWidth - rightWidth - gap * horizontalGaps);

    const Pos align = usingAlignment();
    const int horizontal = layout_detail::Horizontal(align);
    const int vertical = layout_detail::Vertical(align);
    auto place = [&](Node* node, double slotX, double slotY, double slotW, double slotH, bool flexWidth,
                     bool flexHeight) {
        if (node == nullptr) {
            return;
        }
        const double width = flexWidth ? layout_detail::AxisSize(node, slotW, true) : slotW;
        const double height = flexHeight ? layout_detail::AxisSize(node, slotH, false) : slotH;
        const double x = slotX + layout_detail::Align(slotW, width, horizontal);
        const double y = slotY + layout_detail::Align(slotH, height, vertical);
        node->performLayout(x, y, width, height);
    };

    const double middleY = top + topHeight + (top_ ? gap : 0.0);
    const double middleX = left + leftWidth + (left_ ? gap : 0.0);
    place(top_.get(), left, top, innerWidth, topHeight, true, false);
    place(left_.get(), left, middleY, leftWidth, middleHeight, false, true);
    place(center_.get(), middleX, middleY, middleWidth, middleHeight, true, true);
    place(right_.get(), left + innerWidth - rightWidth, middleY, rightWidth, middleHeight, false, true);
    place(bottom_.get(), left, middleY + middleHeight + (bottom_ ? gap : 0.0), innerWidth, bottomHeight, true, false);
}

}  // namespace jadefx

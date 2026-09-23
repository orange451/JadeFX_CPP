#pragma once

#include "jadefx/scene/layout/Pane.hpp"

#include <algorithm>
#include <vector>

namespace jadefx {
namespace layout_detail {

inline double Align(double space, double child, int mode) {
    const double extra = space - child;
    if (mode == 1) {
        return extra * 0.5;
    }
    if (mode == 2) {
        return extra;
    }
    return 0;
}

inline int Horizontal(Pos pos) {
    const HPos side = hpos(pos);
    if (side == HPos::Center) {
        return 1;
    }
    return side == HPos::Right ? 2 : 0;
}

inline int Vertical(Pos pos) {
    const VPos side = vpos(pos);
    if (side == VPos::Center) {
        return 1;
    }
    return side == VPos::Bottom ? 2 : 0;
}

inline std::vector<Node*> ChildPointers(Pane& pane) {
    std::vector<Node*> nodes;
    for (const std::shared_ptr<Node>& child : pane.getChildren().items()) {
        if (child) {
            nodes.push_back(child.get());
        }
    }
    return nodes;
}

inline bool IsFlexible(const SizeSpec& spec) {
    return spec.kind == SizeKind::Percent || spec.kind == SizeKind::Calc;
}

inline double LowerBound(const SizeSpec& minimum, double available, double fontSize) {
    if (!minimum.set()) {
        return 0;
    }
    return std::max(0.0, resolveSize(minimum, available, fontSize));
}

// Same order as Node's measured size: minimum, then maximum, then zero.
inline double BoundSize(double value, const SizeSpec& minimum, const SizeSpec& maximum, double available,
                        double fontSize) {
    if (minimum.set()) {
        value = std::max(value, resolveSize(minimum, available, fontSize));
    }
    if (maximum.set()) {
        value = std::min(value, resolveSize(maximum, available, fontSize));
    }
    return std::max(0.0, value);
}

// Size along a stretched edge. An explicit length wins; otherwise the node fills the slot.
// Minimum size may extend past the slot. Maximum size and the slot itself cap the rest.
inline double AxisSize(const Node* node, double slot, bool horizontal) {
    const ComputedStyle& style = node->computedStyle();
    const SizeSpec& spec = horizontal ? style.width : style.height;
    const SizeSpec& minimum = horizontal ? style.minWidth : style.minHeight;
    const SizeSpec& maximum = horizontal ? style.maxWidth : style.maxHeight;
    const double font = style.fontSize;
    double size = spec.set() ? resolveSize(spec, slot, font) : slot;
    size = BoundSize(size, minimum, maximum, slot, font);
    const double floor = LowerBound(minimum, slot, font);
    if (size > slot) {
        size = std::max(slot, floor);
    }
    return std::max(0.0, size);
}

struct BoxItem {
    Node* node = nullptr;
    double width = 0;
    double height = 0;
    double floor = 0;
    bool flex = false;
};

// Pull flexible main-axis sizes down so the line fits. Fixed sizes and minimums stay put.
inline void ShrinkFlex(std::vector<BoxItem>& items, double gapTotal, double limit, bool horizontal) {
    double used = gapTotal;
    double room = 0;
    for (const BoxItem& item : items) {
        const double size = horizontal ? item.width : item.height;
        used += size;
        if (item.flex) {
            room += std::max(0.0, size - item.floor);
        }
    }
    const double overflow = used - limit;
    if (overflow <= 0.0 || room <= 0.0) {
        return;
    }
    const double shrink = std::min(overflow, room);
    for (BoxItem& item : items) {
        if (!item.flex) {
            continue;
        }
        double& size = horizontal ? item.width : item.height;
        const double share = std::max(0.0, size - item.floor);
        size = std::max(item.floor, size - shrink * (share / room));
    }
}

// Shrink two bands toward their minimums until they fit in limit.
inline void ShareShrink(double& first, double firstFloor, double& second, double secondFloor, double limit) {
    if (first + second <= limit) {
        return;
    }
    const double overflow = first + second - limit;
    const double firstRoom = std::max(0.0, first - firstFloor);
    const double secondRoom = std::max(0.0, second - secondFloor);
    const double room = firstRoom + secondRoom;
    if (room <= 0.0) {
        return;
    }
    const double shrink = std::min(overflow, room);
    first = std::max(firstFloor, first - shrink * (firstRoom / room));
    second = std::max(secondFloor, second - shrink * (secondRoom / room));
}

}  // namespace layout_detail
}  // namespace jadefx

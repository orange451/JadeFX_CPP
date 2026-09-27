#include "ScrollSupport.hpp"

#include "jadefx/scene/Scene.hpp"
#include "gl/UiRenderer.hpp"

#include <algorithm>

namespace jadefx::scroll {

void ClipRegion::detachChild(Node* child) {
    Region::detachChild(child);
    if (onDetached_) {
        onDetached_(child);
    }
}

void ClipRegion::renderChildren(UiRenderer& renderer, float opacity) {
    renderer.pushClip(static_cast<float>(getAbsoluteX()), static_cast<float>(getAbsoluteY()),
                      static_cast<float>(getWidth()), static_cast<float>(getHeight()));
    Region::renderChildren(renderer, opacity);
    renderer.popClip();
}

namespace {

bool Shows(ScrollBarPolicy policy, double content, double viewport) {
    switch (policy) {
        case ScrollBarPolicy::Always:
            return true;
        case ScrollBarPolicy::Never:
            return false;
        case ScrollBarPolicy::AsNeeded:
            return content > viewport + 0.5;
    }
    return false;
}

}  // namespace

BarLayout ResolveBars(ScrollBarPolicy horizontal, ScrollBarPolicy vertical, Size box,
                      const std::function<Size(Size viewport)>& measure) {
    const double bar = ScrollBar::kThickness;
    BarLayout out;
    out.horizontal = horizontal == ScrollBarPolicy::Always;
    out.vertical = vertical == ScrollBarPolicy::Always;
    auto viewportFor = [&](bool h, bool v) {
        return Size{std::max(0.0, box.width - (v ? bar : 0.0)), std::max(0.0, box.height - (h ? bar : 0.0))};
    };
    // Two rounds settle it; the third only confirms.
    for (int round = 0; round < 3; ++round) {
        out.viewport = viewportFor(out.horizontal, out.vertical);
        out.content = measure(out.viewport);
        const bool nextH = Shows(horizontal, out.content.width, out.viewport.width);
        const bool nextV = Shows(vertical, out.content.height, out.viewport.height);
        if (nextH == out.horizontal && nextV == out.vertical) {
            return out;
        }
        out.horizontal = nextH;
        out.vertical = nextV;
    }
    out.viewport = viewportFor(out.horizontal, out.vertical);
    out.content = measure(out.viewport);
    return out;
}

void PlaceBar(ScrollBar& bar, const BarPlacement& placement) {
    bar.setVisible(placement.shown);
    if (!placement.shown) {
        bar.performLayout(0, 0, 0, 0);
        return;
    }
    const double travel = placement.content - placement.viewport;
    bar.setMin(placement.min);
    bar.setMax(placement.max);
    bar.setVisiblePortion(placement.viewport, placement.content);
    bar.setUnitIncrement(travel > 0 ? (placement.max - placement.min) * kLineStep / travel : 0.0);
    bar.setValue(placement.value);
    const double breadth = ScrollBar::kThickness + ScrollBar::kHitSlop;
    if (bar.getOrientation() == Orientation::Horizontal) {
        bar.performLayout(placement.left, placement.top + placement.box.height - ScrollBar::kHitSlop,
                          placement.box.width, breadth);
    } else {
        bar.performLayout(placement.left + placement.box.width - ScrollBar::kHitSlop, placement.top, breadth,
                          placement.box.height);
    }
}

Size WheelPoints(const Node& owner, const ScrollEvent& event) {
    const bool shift = owner.getScene() != nullptr && (owner.getScene()->modifierMask() & Key::ModShift) != 0;
    const double dx = shift ? event.deltaY + event.deltaX : event.deltaX;
    const double dy = shift ? 0.0 : event.deltaY;
    return {-dx * kWheelStep, -dy * kWheelStep};
}

std::shared_ptr<ScrollBar> MakeOwnedBar(Orientation orientation, std::function<void(double)> onValue) {
    auto bar = std::make_shared<ScrollBar>(orientation);
    bar->setFocusTraversable(false);
    bar->setVisible(false);
    ScrollBar* raw = bar.get();
    bar->setOnValueChanged([raw, onValue = std::move(onValue)] {
        if (onValue) {
            onValue(raw->getValue());
        }
    });
    return bar;
}

}  // namespace jadefx::scroll

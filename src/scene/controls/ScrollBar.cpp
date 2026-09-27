#include "jadefx/scene/controls/ScrollBar.hpp"

#include <algorithm>
#include <cmath>

namespace jadefx {

ScrollBar::ScrollBar() : ScrollBar(Orientation::Horizontal) {}

ScrollBar::ScrollBar(Orientation orientation) : orientation_(orientation) {
    getClassList().add("scroll-bar");
    setDefaultCursor(Cursor::Default);
    syncPseudos();
}

void ScrollBar::setOrientation(Orientation orientation) {
    orientation_ = orientation;
    syncPseudos();
}

void ScrollBar::syncPseudos() {
    const bool vertical = orientation_ == Orientation::Vertical;
    setPseudoState("vertical", vertical);
    setPseudoState("horizontal", !vertical);
}

void ScrollBar::setMin(double value) {
    if (!std::isfinite(value)) {
        return;
    }
    min_ = value;
    max_ = std::max(max_, min_);
    adjustValue(value_);
}

void ScrollBar::setMax(double value) {
    if (!std::isfinite(value)) {
        return;
    }
    max_ = value;
    min_ = std::min(min_, max_);
    adjustValue(value_);
}

void ScrollBar::setValue(double value) { adjustValue(value); }

void ScrollBar::setVisibleAmount(double value) { visible_ = std::isfinite(value) ? std::max(0.0, value) : 0.0; }

void ScrollBar::setVisiblePortion(double viewportLength, double contentLength) {
    // The range spans how far the content can move, contentLength - viewportLength.
    const double range = max_ - min_;
    const double travel = contentLength - viewportLength;
    setVisibleAmount(contentLength > 0 ? range * std::min(1.0, viewportLength / contentLength) : range);
    block_ = travel > 0 ? range * viewportLength / travel : range;
}

void ScrollBar::adjustValue(double value) {
    if (!std::isfinite(value)) {
        return;
    }
    const double next = std::clamp(value, min_, max_);
    if (next == value_) {
        return;
    }
    value_ = next;
    if (onChanged_) {
        onChanged_();
    }
}

void ScrollBar::increment() { adjustValue(value_ + unit_); }

void ScrollBar::decrement() { adjustValue(value_ - unit_); }

double ScrollBar::fraction() const {
    const double range = max_ - min_;
    return range > 0 ? (value_ - min_) / range : 0.0;
}

ScrollTrack ScrollBar::track() const {
    const bool vertical = orientation_ == Orientation::Vertical;
    const float length = static_cast<float>(vertical ? contentHeight() : contentWidth());
    const double range = max_ - min_;
    // The track stands in for the visible amount, so the whole range is that
    // many tracks long. A zero visible amount leaves the thumb at its minimum.
    const float content = visible_ > 0 ? static_cast<float>(static_cast<double>(length) * range / visible_)
                                       : (range > 0 ? length * 1e6f : 0.f);
    const double offset = fraction() * static_cast<double>(std::max(0.f, content - length));
    const float breadth = static_cast<float>(vertical ? contentWidth() : contentHeight());
    const float thickness = std::min(breadth, ScrollTrack::kThickness);
    const float cross = breadth - thickness;
    ScrollTrack bar = vertical ? ScrollTrack::vertical(cross, 0.f, length, content, length, offset)
                               : ScrollTrack::horizontal(0.f, cross, length, content, length, offset);
    bar.thickness = thickness;
    return bar;
}

void ScrollBar::handleMousePressed(const MouseEvent& event) {
    if (event.button != 0) {
        return;
    }
    const ScrollTrack bar = track();
    const float localX = static_cast<float>(event.x - getAbsoluteX() - contentLeft());
    const float localY = static_cast<float>(event.y - getAbsoluteY() - contentTop());
    const ScrollTrack::Part part = bar.part(localX, localY);
    if (part == ScrollTrack::Part::None) {
        return;
    }
    if (part == ScrollTrack::Part::Thumb) {
        dragging_ = true;
        grab_ = (bar.sideways ? localX : localY) - bar.thumb;
        return;
    }
    adjustValue(value_ + (part == ScrollTrack::Part::After ? block_ : -block_));
}

void ScrollBar::handleMouseDragged(const MouseEvent& event) {
    if (!dragging_) {
        return;
    }
    const ScrollTrack bar = track();
    const float localX = static_cast<float>(event.x - getAbsoluteX() - contentLeft());
    const float localY = static_cast<float>(event.y - getAbsoluteY() - contentTop());
    const double limit = static_cast<double>(bar.maxOffset());
    if (limit <= 0) {
        return;
    }
    const double offset = bar.offsetFromDrag(localX, localY, grab_);
    adjustValue(min_ + (max_ - min_) * offset / limit);
}

void ScrollBar::handleMouseReleased(const MouseEvent&) { dragging_ = false; }

void ScrollBar::handleKey(KeyEvent& event) {
    if (!event.pressed) {
        return;
    }
    const bool vertical = orientation_ == Orientation::Vertical;
    switch (event.key) {
        case Key::Left:
        case Key::Up:
            if ((event.key == Key::Up) != vertical) {
                return;
            }
            decrement();
            break;
        case Key::Right:
        case Key::Down:
            if ((event.key == Key::Down) != vertical) {
                return;
            }
            increment();
            break;
        case Key::PageUp:
            adjustValue(value_ - block_);
            break;
        case Key::PageDown:
            adjustValue(value_ + block_);
            break;
        case Key::Home:
            adjustValue(min_);
            break;
        case Key::End:
            adjustValue(max_);
            break;
        default:
            return;
    }
    event.consume();
}

double ScrollBar::preferredContentWidth(double) const {
    return orientation_ == Orientation::Vertical ? kThickness : 100.0;
}

double ScrollBar::preferredContentHeight(double) const {
    return orientation_ == Orientation::Vertical ? 100.0 : kThickness;
}

void ScrollBar::renderContent(UiRenderer& renderer, float opacity) {
    track().draw(renderer, static_cast<float>(getAbsoluteX() + contentLeft()),
                 static_cast<float>(getAbsoluteY() + contentTop()), opacity, themeColor(ThemeColor::Scrollbar));
}

}  // namespace jadefx

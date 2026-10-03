#include "jadefx/scene/controls/ScrollPane.hpp"

#include "ScrollSupport.hpp"
#include "jadefx/scene/Scene.hpp"
#include "jadefx/scene/controls/TextField.hpp"

#include <algorithm>
#include <cmath>

namespace jadefx {

namespace {

// Keeps min <= max by moving the end that was not just set.
void Order(double& min, double& max, bool minChanged) {
    if (min <= max) {
        return;
    }
    if (minChanged) {
        max = min;
    } else {
        min = max;
    }
}

}  // namespace

ScrollPane::ScrollPane() {
    getClassList().add("scroll-pane");
    viewportNode_ = std::make_shared<scroll::ClipRegion>("viewport");
    viewportNode_->setOnChildDetached([this](Node* child) {
        if (content_ && content_.get() == child) {
            content_.reset();
        }
    });
    hbar_ = scroll::MakeOwnedBar(Orientation::Horizontal, [this](double value) { changeHvalue(value); });
    vbar_ = scroll::MakeOwnedBar(Orientation::Vertical, [this](double value) { changeVvalue(value); });
    children().add(viewportNode_);
    children().add(hbar_);
    children().add(vbar_);
}

ScrollPane::ScrollPane(std::shared_ptr<Node> content) : ScrollPane() { setContent(std::move(content)); }

ScrollPane::~ScrollPane() {
    viewportNode_->setOnChildDetached(nullptr);
    hbar_->setOnValueChanged(nullptr);
    vbar_->setOnValueChanged(nullptr);
}

void ScrollPane::setContent(std::shared_ptr<Node> content) {
    if (content == content_) {
        return;
    }
    ObservableList<std::shared_ptr<Node>>& items = viewportNode_->items();
    items.clear();
    content_ = std::move(content);
    if (content_) {
        items.add(content_);
    }
}

void ScrollPane::changeHvalue(double value) {
    value = std::clamp(value, hmin_, hmax_);
    if (value == hvalue_) {
        return;
    }
    hvalue_ = value;
    if (onHvalue_) {
        onHvalue_();
    }
}

void ScrollPane::changeVvalue(double value) {
    value = std::clamp(value, vmin_, vmax_);
    if (value == vvalue_) {
        return;
    }
    vvalue_ = value;
    if (onVvalue_) {
        onVvalue_();
    }
}

void ScrollPane::setHvalue(double value) {
    if (std::isfinite(value)) {
        changeHvalue(value);
    }
}

void ScrollPane::setVvalue(double value) {
    if (std::isfinite(value)) {
        changeVvalue(value);
    }
}

void ScrollPane::setHmin(double value) {
    if (std::isfinite(value)) {
        hmin_ = value;
        Order(hmin_, hmax_, true);
        changeHvalue(hvalue_);
    }
}

void ScrollPane::setHmax(double value) {
    if (std::isfinite(value)) {
        hmax_ = value;
        Order(hmin_, hmax_, false);
        changeHvalue(hvalue_);
    }
}

void ScrollPane::setVmin(double value) {
    if (std::isfinite(value)) {
        vmin_ = value;
        Order(vmin_, vmax_, true);
        changeVvalue(vvalue_);
    }
}

void ScrollPane::setVmax(double value) {
    if (std::isfinite(value)) {
        vmax_ = value;
        Order(vmin_, vmax_, false);
        changeVvalue(vvalue_);
    }
}

double ScrollPane::valueForOffset(double offset, double maxOffset, bool horizontal) const {
    const double min = horizontal ? hmin_ : vmin_;
    const double max = horizontal ? hmax_ : vmax_;
    return maxOffset > 0 ? min + (max - min) * std::clamp(offset / maxOffset, 0.0, 1.0) : min;
}

double ScrollPane::offsetForValue(double value, double maxOffset, bool horizontal) const {
    const double min = horizontal ? hmin_ : vmin_;
    const double max = horizontal ? hmax_ : vmax_;
    return max > min ? maxOffset * (value - min) / (max - min) : 0.0;
}

bool ScrollPane::scrollBy(double dx, double dy) {
    const double beforeH = hvalue_;
    const double beforeV = vvalue_;
    if (dx != 0 && scrollRange_.width > 0) {
        const double offset = offsetForValue(hvalue_, scrollRange_.width, true) + dx;
        changeHvalue(valueForOffset(offset, scrollRange_.width, true));
    }
    if (dy != 0 && scrollRange_.height > 0) {
        const double offset = offsetForValue(vvalue_, scrollRange_.height, false) + dy;
        changeVvalue(valueForOffset(offset, scrollRange_.height, false));
    }
    return hvalue_ != beforeH || vvalue_ != beforeV;
}

void ScrollPane::handleScroll(ScrollEvent& event) {
    const Size delta = scroll::WheelPoints(*this, event);
    if (scrollBy(delta.width, delta.height)) {
        event.consume();
    }
}

void ScrollPane::handleKey(KeyEvent& event) {
    if (!event.pressed) {
        return;
    }
    // A text field leaves Up and Down for a combo box or spinner around it, but
    // they move its caret, so they do not scroll the pane as well.
    const bool vertical = event.key == Key::Up || event.key == Key::Down;
    if (vertical && getScene() != nullptr && dynamic_cast<TextField*>(getScene()->focusedNode()) != nullptr) {
        return;
    }
    bool moved = false;
    switch (event.key) {
        case Key::Up:
            moved = scrollBy(0, -scroll::kLineStep);
            break;
        case Key::Down:
            moved = scrollBy(0, scroll::kLineStep);
            break;
        case Key::Left:
            moved = scrollBy(-scroll::kLineStep, 0);
            break;
        case Key::Right:
            moved = scrollBy(scroll::kLineStep, 0);
            break;
        case Key::PageUp:
            moved = scrollBy(0, -viewport_.height);
            break;
        case Key::PageDown:
            moved = scrollBy(0, viewport_.height);
            break;
        case Key::Home:
            moved = scrollBy(0, -scrollRange_.height);
            break;
        case Key::End:
            moved = scrollBy(0, scrollRange_.height);
            break;
        default:
            break;
    }
    if (moved) {
        event.consume();
    }
}

void ScrollPane::layoutChildren() {
    const double left = contentLeft();
    const double top = contentTop();
    const scroll::BarLayout bars =
        scroll::ResolveBars(hbarPolicy_, vbarPolicy_, {contentWidth(), contentHeight()}, [this](Size view) {
            Size size;
            if (!content_) {
                return size;
            }
            size.width = fitToWidth_ ? std::max(view.width, content_->getMinWidth()) : content_->measuredWidth(view.width);
            size.height = fitToHeight_ ? std::max(view.height, content_->getMinHeight())
                                       : content_->measuredHeight(size.width, view.height);
            return size;
        });
    viewport_ = bars.viewport;
    contentSize_ = bars.content;
    scrollRange_ = {std::max(0.0, contentSize_.width - viewport_.width),
                    std::max(0.0, contentSize_.height - viewport_.height)};

    viewportNode_->performLayout(left, top, viewport_.width, viewport_.height);
    if (content_) {
        const double offsetX = offsetForValue(hvalue_, scrollRange_.width, true);
        const double offsetY = offsetForValue(vvalue_, scrollRange_.height, false);
        content_->performLayout(-std::round(offsetX), -std::round(offsetY), contentSize_.width, contentSize_.height);
    }
    scroll::PlaceBar(*hbar_, {bars.horizontal, hmin_, hmax_, hvalue_, viewport_.width, contentSize_.width, left, top,
                              viewport_});
    scroll::PlaceBar(*vbar_, {bars.vertical, vmin_, vmax_, vvalue_, viewport_.height, contentSize_.height, left, top,
                              viewport_});
}

double ScrollPane::preferredContentWidth(double innerAvailable) const {
    const double bar = vbarPolicy_ == ScrollBarPolicy::Always ? ScrollBar::kThickness : 0.0;
    if (prefViewportWidth_ > 0) {
        return prefViewportWidth_ + bar;
    }
    return (content_ ? content_->measuredWidth(std::max(0.0, innerAvailable - bar)) : 0.0) + bar;
}

double ScrollPane::preferredContentHeight(double innerWidth) const {
    const double vbar = vbarPolicy_ == ScrollBarPolicy::Always ? ScrollBar::kThickness : 0.0;
    const double hbar = hbarPolicy_ == ScrollBarPolicy::Always ? ScrollBar::kThickness : 0.0;
    if (prefViewportHeight_ > 0) {
        return prefViewportHeight_ + hbar;
    }
    return (content_ ? content_->measuredHeight(std::max(0.0, innerWidth - vbar), -1) : 0.0) + hbar;
}

}  // namespace jadefx

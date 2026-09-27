#include "jadefx/scene/controls/ScrollPane.hpp"

#include "jadefx/scene/Scene.hpp"
#include "jadefx/scene/controls/ScrollBar.hpp"
#include "gl/UiRenderer.hpp"

#include <algorithm>
#include <cmath>

namespace jadefx {

// Holds the content and clips it. Picking stops at its bounds too, so a part of
// the content scrolled out of view cannot take a click.
class ScrollPaneViewport : public Region {
public:
    explicit ScrollPaneViewport(ScrollPane& pane) : pane_(&pane) { getClassList().add("viewport"); }

    const char* getElementType() const override { return "viewport"; }

    void release() { pane_ = nullptr; }
    ObservableList<std::shared_ptr<Node>>& items() { return children(); }

    void detachChild(Node* child) override {
        Region::detachChild(child);
        if (pane_ != nullptr) {
            pane_->contentDetached();
        }
    }

protected:
    void renderChildren(UiRenderer& renderer, float opacity) override {
        renderer.pushClip(static_cast<float>(getAbsoluteX()), static_cast<float>(getAbsoluteY()),
                          static_cast<float>(getWidth()), static_cast<float>(getHeight()));
        Region::renderChildren(renderer, opacity);
        renderer.popClip();
    }

private:
    ScrollPane* pane_ = nullptr;
};

namespace {

// Points per wheel notch. A trackpad sends fractions of a notch.
constexpr double kWheelStep = 40;
// Points per arrow key.
constexpr double kLineStep = 20;

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
    viewportNode_ = std::make_shared<ScrollPaneViewport>(*this);
    hbar_ = std::make_shared<ScrollBar>(Orientation::Horizontal);
    vbar_ = std::make_shared<ScrollBar>(Orientation::Vertical);
    for (ScrollBar* bar : {hbar_.get(), vbar_.get()}) {
        bar->setFocusTraversable(false);
        bar->setVisible(false);
    }
    hbar_->setOnValueChanged([this] { changeHvalue(hbar_->getValue()); });
    vbar_->setOnValueChanged([this] { changeVvalue(vbar_->getValue()); });
    children().add(viewportNode_);
    children().add(hbar_);
    children().add(vbar_);
}

ScrollPane::ScrollPane(std::shared_ptr<Node> content) : ScrollPane() { setContent(std::move(content)); }

ScrollPane::~ScrollPane() {
    viewportNode_->release();
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

void ScrollPane::contentDetached() {
    if (content_ && content_->getParent() != viewportNode_.get()) {
        content_.reset();
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
    const bool shift = getScene() != nullptr && (getScene()->modifierMask() & Key::ModShift) != 0;
    const double dx = shift ? event.deltaY + event.deltaX : event.deltaX;
    const double dy = shift ? 0.0 : event.deltaY;
    if (scrollBy(-dx * kWheelStep, -dy * kWheelStep)) {
        event.consume();
    }
}

void ScrollPane::handleKey(KeyEvent& event) {
    if (!event.pressed) {
        return;
    }
    bool moved = false;
    switch (event.key) {
        case Key::Up:
            moved = scrollBy(0, -kLineStep);
            break;
        case Key::Down:
            moved = scrollBy(0, kLineStep);
            break;
        case Key::Left:
            moved = scrollBy(-kLineStep, 0);
            break;
        case Key::Right:
            moved = scrollBy(kLineStep, 0);
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

bool ScrollPane::showsBar(ScrollBarPolicy policy, double content, double viewport) const {
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

void ScrollPane::layoutChildren() {
    const double left = contentLeft();
    const double top = contentTop();
    const double width = contentWidth();
    const double height = contentHeight();
    const double bar = ScrollBar::kThickness;

    auto measure = [this](double viewWidth, double viewHeight) {
        Size size;
        if (!content_) {
            return size;
        }
        size.width = fitToWidth_ ? std::max(viewWidth, content_->getMinWidth()) : content_->measuredWidth(viewWidth);
        size.height = fitToHeight_ ? std::max(viewHeight, content_->getMinHeight())
                                   : content_->measuredHeight(size.width, viewHeight);
        return size;
    };

    // Each bar narrows the other axis, which can bring in the other bar. Two
    // rounds settle it; the third only confirms.
    bool needH = hbarPolicy_ == ScrollBarPolicy::Always;
    bool needV = vbarPolicy_ == ScrollBarPolicy::Always;
    double viewWidth = width;
    double viewHeight = height;
    Size size;
    for (int round = 0; round < 3; ++round) {
        viewWidth = std::max(0.0, width - (needV ? bar : 0.0));
        viewHeight = std::max(0.0, height - (needH ? bar : 0.0));
        size = measure(viewWidth, viewHeight);
        const bool nextH = showsBar(hbarPolicy_, size.width, viewWidth);
        const bool nextV = showsBar(vbarPolicy_, size.height, viewHeight);
        if (nextH == needH && nextV == needV) {
            break;
        }
        needH = nextH;
        needV = nextV;
    }
    viewWidth = std::max(0.0, width - (needV ? bar : 0.0));
    viewHeight = std::max(0.0, height - (needH ? bar : 0.0));

    viewport_ = {viewWidth, viewHeight};
    contentSize_ = size;
    scrollRange_ = {std::max(0.0, size.width - viewWidth), std::max(0.0, size.height - viewHeight)};

    viewportNode_->performLayout(left, top, viewWidth, viewHeight);
    if (content_) {
        const double offsetX = offsetForValue(hvalue_, scrollRange_.width, true);
        const double offsetY = offsetForValue(vvalue_, scrollRange_.height, false);
        content_->performLayout(-std::round(offsetX), -std::round(offsetY), size.width, size.height);
    }

    auto place = [&](ScrollBar& scrollBar, bool shown, bool horizontal) {
        scrollBar.setVisible(shown);
        if (!shown) {
            scrollBar.performLayout(0, 0, 0, 0);
            return;
        }
        const double range = horizontal ? scrollRange_.width : scrollRange_.height;
        const double span = (horizontal ? hmax_ - hmin_ : vmax_ - vmin_);
        scrollBar.setMin(horizontal ? hmin_ : vmin_);
        scrollBar.setMax(horizontal ? hmax_ : vmax_);
        scrollBar.setVisiblePortion(horizontal ? viewWidth : viewHeight, horizontal ? size.width : size.height);
        scrollBar.setUnitIncrement(range > 0 ? span * kLineStep / range : 0.0);
        scrollBar.setValue(horizontal ? hvalue_ : vvalue_);
        // The bar reaches kHitSlop over the viewport's edge, so it is easier to grab.
        const double breadth = bar + ScrollBar::kHitSlop;
        if (horizontal) {
            scrollBar.performLayout(left, top + viewHeight - ScrollBar::kHitSlop, viewWidth, breadth);
        } else {
            scrollBar.performLayout(left + viewWidth - ScrollBar::kHitSlop, top, breadth, viewHeight);
        }
    };
    place(*hbar_, needH, true);
    place(*vbar_, needV, false);
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

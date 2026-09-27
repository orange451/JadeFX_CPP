#include "VirtualFlow.hpp"

#include "ScrollSupport.hpp"

#include <algorithm>
#include <cmath>

namespace jadefx {

VirtualFlow::VirtualFlow() {
    getClassList().add("virtual-flow");
    sheet_ = std::make_shared<scroll::ClipRegion>("clipped-container");
    sheet_->setFocusTraversable(false);
    hbar_ = scroll::MakeOwnedBar(Orientation::Horizontal, [this](double value) {
        if (vertical_) {
            setBreadthOffset(value);
        } else {
            position_ = value;
        }
    });
    vbar_ = scroll::MakeOwnedBar(Orientation::Vertical, [this](double value) {
        if (vertical_) {
            position_ = value;
        } else {
            setBreadthOffset(value);
        }
    });
    children().add(sheet_);
    children().add(hbar_);
    children().add(vbar_);
}

VirtualFlow::~VirtualFlow() {
    hbar_->setOnValueChanged(nullptr);
    vbar_->setOnValueChanged(nullptr);
}

void VirtualFlow::setCellFactory(CellFactory factory) {
    for (Slot& slot : active_) {
        park(slot);
    }
    active_.clear();
    pile_.clear();
    sheet_->items().clear();
    factory_ = std::move(factory);
}

void VirtualFlow::setCellCount(int count) { count_ = std::max(0, count); }

void VirtualFlow::setVertical(bool vertical) {
    if (vertical == vertical_) {
        return;
    }
    vertical_ = vertical;
    position_ = 0;
    breadthOffset_ = 0;
    rebindAll_ = true;
}

void VirtualFlow::scrollTo(int index) {
    pendingTop_ = index;
    pendingShow_ = -1;
}

void VirtualFlow::show(int index) {
    pendingShow_ = index;
    pendingTop_ = -1;
}

double VirtualFlow::maxPosition() const {
    return std::max(0.0, static_cast<double>(count_) * cellLength_ - viewportLength_);
}

bool VirtualFlow::scrollPixels(double delta) {
    const double before = position_;
    position_ = std::clamp(position_ + delta, 0.0, maxPosition());
    return position_ != before;
}

void VirtualFlow::setBreadthOffset(double offset) {
    if (offset == breadthOffset_) {
        return;
    }
    breadthOffset_ = offset;
    if (onBreadth_) {
        onBreadth_();
    }
}

int VirtualFlow::getFirstVisibleIndex() const { return active_.empty() ? -1 : active_.front().index; }

int VirtualFlow::getLastVisibleIndex() const { return active_.empty() ? -1 : active_.back().index; }

IndexedCell* VirtualFlow::getVisibleCell(int index) const {
    for (const Slot& slot : active_) {
        if (slot.index == index) {
            return slot.cell.get();
        }
    }
    return nullptr;
}

void VirtualFlow::forEachVisibleCell(const std::function<void(IndexedCell&)>& visit) const {
    for (const Slot& slot : active_) {
        visit(*slot.cell);
    }
}

void VirtualFlow::handleScroll(ScrollEvent& event) {
    const Size delta = scroll::WheelPoints(*this, event);
    // A vertical wheel runs a horizontal flow along its rows too.
    const double alongDelta = vertical_ ? delta.height : (delta.width != 0 ? delta.width : delta.height);
    const double acrossDelta = vertical_ ? delta.width : 0.0;
    const bool moved = scrollPixels(alongDelta);
    const double before = breadthOffset_;
    setBreadthOffset(std::clamp(breadthOffset_ + acrossDelta, 0.0, std::max(0.0, contentBreadth_ - viewportBreadth_)));
    if (moved || breadthOffset_ != before) {
        event.consume();
    }
}

std::shared_ptr<IndexedCell> VirtualFlow::takeCell() {
    if (!pile_.empty()) {
        std::shared_ptr<IndexedCell> cell = std::move(pile_.back());
        pile_.pop_back();
        cell->setVisible(true);
        return cell;
    }
    std::shared_ptr<IndexedCell> cell = factory_ ? factory_() : nullptr;
    if (cell) {
        sheet_->items().add(cell);
    }
    return cell;
}

void VirtualFlow::park(Slot& slot) {
    if (!slot.cell) {
        return;
    }
    if (binder_) {
        binder_(*slot.cell, -1);
    }
    slot.cell->setVisible(false);
    slot.cell->performLayout(0, 0, 0, 0);
    pile_.push_back(std::move(slot.cell));
}

void VirtualFlow::bindRange(int first, int last) {
    last = std::min(last, count_ - 1);
    std::vector<std::shared_ptr<IndexedCell>> kept(first <= last ? static_cast<std::size_t>(last - first + 1) : 0);
    for (Slot& slot : active_) {
        if (slot.index >= first && slot.index <= last) {
            kept[static_cast<std::size_t>(slot.index - first)] = std::move(slot.cell);
        } else {
            park(slot);
        }
    }
    active_.clear();
    for (int index = first; index <= last; ++index) {
        std::shared_ptr<IndexedCell>& cell = kept[static_cast<std::size_t>(index - first)];
        const bool fresh = !cell;
        if (fresh) {
            cell = takeCell();
            if (!cell) {
                break;
            }
        }
        if (fresh || rebindAll_) {
            if (binder_) {
                binder_(*cell, index);
            }
            // A new row may bring new text or a new graphic. Style it now so it measures right.
            cell->applyCss();
        }
        active_.push_back({index, std::move(cell)});
    }
    rebindAll_ = false;
}

double VirtualFlow::measureCellLength(double breadth) {
    if (fixedSize_ > 0) {
        return fixedSize_;
    }
    if (count_ == 0) {
        return cellLength_;
    }
    if (active_.empty()) {
        const int first = std::clamp(static_cast<int>(position_ / std::max(1.0, cellLength_)), 0, count_ - 1);
        bindRange(first, first);
    }
    if (active_.empty()) {
        return cellLength_;
    }
    const IndexedCell& probe = *active_.front().cell;
    const double length = vertical_ ? probe.measuredHeight(breadth, -1) : probe.measuredWidth(breadth);
    return std::max(1.0, length);
}

void VirtualFlow::layoutChildren() {
    const double left = contentLeft();
    const double top = contentTop();
    const Size box{contentWidth(), contentHeight()};
    auto rangeFor = [this](double viewport, int& first, int& last) {
        first = static_cast<int>(std::floor(position_ / cellLength_));
        last = static_cast<int>(std::ceil((position_ + viewport) / cellLength_)) - 1;
        first = std::clamp(first, 0, std::max(0, count_ - 1));
        last = std::min(last, count_ - 1);
    };

    // Measure against the whole box first. The bars can only take room away.
    cellLength_ = measureCellLength(across(box));
    viewportLength_ = along(box);
    position_ = std::clamp(position_, 0.0, maxPosition());
    int first = 0;
    int last = -1;
    rangeFor(viewportLength_, first, last);
    bindRange(first, last);
    double preferredBreadth = 0;
    for (const Slot& slot : active_) {
        const double breadth =
            vertical_ ? slot.cell->measuredWidth(across(box)) : slot.cell->measuredHeight(cellLength_, -1);
        preferredBreadth = std::max(preferredBreadth, breadth);
    }
    const double totalLength = static_cast<double>(count_) * cellLength_;
    const scroll::BarLayout bars = scroll::ResolveBars(
        ScrollBarPolicy::AsNeeded, ScrollBarPolicy::AsNeeded, box, [&](Size view) {
            const double breadth = fixedBreadth_ > 0 ? fixedBreadth_ : std::max(across(view), preferredBreadth);
            return sized(totalLength, breadth);
        });
    viewportLength_ = along(bars.viewport);
    viewportBreadth_ = across(bars.viewport);
    contentBreadth_ = across(bars.content);

    if (pendingTop_ >= 0) {
        position_ = static_cast<double>(pendingTop_) * cellLength_;
    } else if (pendingShow_ >= 0) {
        const double start = static_cast<double>(pendingShow_) * cellLength_;
        if (start < position_) {
            position_ = start;
        } else if (start + cellLength_ > position_ + viewportLength_) {
            position_ = start + cellLength_ - viewportLength_;
        }
    }
    pendingTop_ = -1;
    pendingShow_ = -1;
    position_ = std::clamp(position_, 0.0, maxPosition());
    setBreadthOffset(std::clamp(breadthOffset_, 0.0, std::max(0.0, contentBreadth_ - viewportBreadth_)));
    rangeFor(viewportLength_, first, last);
    bindRange(first, last);

    sheet_->performLayout(left, top, bars.viewport.width, bars.viewport.height);
    const double shift = -std::round(breadthOffset_);
    for (const Slot& slot : active_) {
        const double at = std::round(static_cast<double>(slot.index) * cellLength_ - position_);
        if (vertical_) {
            slot.cell->performLayout(shift, at, contentBreadth_, cellLength_);
        } else {
            slot.cell->performLayout(at, shift, cellLength_, contentBreadth_);
        }
    }

    const double breadthRange = std::max(0.0, contentBreadth_ - viewportBreadth_);
    ScrollBar& alongBar = vertical_ ? *vbar_ : *hbar_;
    ScrollBar& acrossBar = vertical_ ? *hbar_ : *vbar_;
    const bool alongShown = vertical_ ? bars.vertical : bars.horizontal;
    const bool acrossShown = vertical_ ? bars.horizontal : bars.vertical;
    scroll::PlaceBar(alongBar, {alongShown, 0, maxPosition(), position_, viewportLength_, totalLength, left, top,
                                bars.viewport});
    scroll::PlaceBar(acrossBar, {acrossShown, 0, breadthRange, breadthOffset_, viewportBreadth_, contentBreadth_, left,
                                 top, bars.viewport});
}

double VirtualFlow::preferredContentWidth(double) const { return 0; }

double VirtualFlow::preferredContentHeight(double) const { return 0; }

}  // namespace jadefx

#include "jadefx/scene/controls/SelectionModel.hpp"

#include <algorithm>

namespace jadefx {

void MultipleSelectionModel::setSelectionMode(SelectionMode mode) {
    if (mode == mode_) {
        return;
    }
    mode_ = mode;
    if (mode_ == SelectionMode::Single && selected_.size() > 1) {
        assign({selectedIndex_}, selectedIndex_, selectedIndex_);
    }
}

void MultipleSelectionModel::assign(std::vector<int> indices, int selectedIndex, int anchor) {
    std::vector<int> unique;
    unique.reserve(indices.size());
    for (const int index : indices) {
        if (std::find(unique.begin(), unique.end(), index) == unique.end()) {
            unique.push_back(index);
        }
    }
    anchor_ = anchor;
    if (unique == selected_ && selectedIndex == selectedIndex_) {
        return;
    }
    selected_ = std::move(unique);
    selectedIndex_ = selectedIndex;
    // A listener may add or remove listeners, so the round runs on a copy.
    const std::vector<Entry> round = listeners_;
    for (const Entry& entry : round) {
        if (entry.listener) {
            entry.listener();
        }
    }
}

void MultipleSelectionModel::select(int index) {
    if (!valid(index)) {
        return;
    }
    if (mode_ == SelectionMode::Single) {
        assign({index}, index, index);
        return;
    }
    std::vector<int> next = selected_;
    next.push_back(index);
    assign(std::move(next), index, index);
}

void MultipleSelectionModel::clearAndSelect(int index) {
    if (valid(index)) {
        assign({index}, index, index);
    }
}

void MultipleSelectionModel::selectIndices(const std::vector<int>& indices) {
    std::vector<int> next = mode_ == SelectionMode::Multiple ? selected_ : std::vector<int>{};
    int last = -1;
    for (const int index : indices) {
        if (valid(index)) {
            next.push_back(index);
            last = index;
        }
    }
    if (last < 0) {
        return;
    }
    if (mode_ == SelectionMode::Single) {
        next = {last};
    }
    assign(std::move(next), last, last);
}

void MultipleSelectionModel::selectRange(int start, int end) {
    std::vector<int> range;
    if (start <= end) {
        for (int index = start; index < end; ++index) {
            range.push_back(index);
        }
    } else {
        for (int index = start; index > end; --index) {
            range.push_back(index);
        }
    }
    std::vector<int> next = mode_ == SelectionMode::Multiple ? selected_ : std::vector<int>{};
    int last = -1;
    for (const int index : range) {
        if (valid(index)) {
            next.push_back(index);
            last = index;
        }
    }
    if (last < 0) {
        return;
    }
    if (mode_ == SelectionMode::Single) {
        next = {last};
    }
    assign(std::move(next), last, anchor_);
}

void MultipleSelectionModel::selectAll() {
    const int count = itemCount();
    if (mode_ != SelectionMode::Multiple || count == 0) {
        return;
    }
    std::vector<int> all(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        all[static_cast<std::size_t>(index)] = index;
    }
    assign(std::move(all), selectedIndex_ >= 0 ? selectedIndex_ : count - 1, anchor_ >= 0 ? anchor_ : 0);
}

void MultipleSelectionModel::selectFirst() { clearAndSelect(0); }

void MultipleSelectionModel::selectLast() { clearAndSelect(itemCount() - 1); }

void MultipleSelectionModel::selectPrevious() {
    if (selectedIndex_ > 0) {
        clearAndSelect(selectedIndex_ - 1);
    }
}

void MultipleSelectionModel::selectNext() { clearAndSelect(selectedIndex_ + 1); }

void MultipleSelectionModel::clearSelection() { assign({}, -1, -1); }

void MultipleSelectionModel::clearSelection(int index) {
    if (!isSelected(index)) {
        return;
    }
    std::vector<int> next = selected_;
    next.erase(std::find(next.begin(), next.end(), index));
    const int selectedIndex = index == selectedIndex_ ? (next.empty() ? -1 : next.back()) : selectedIndex_;
    assign(std::move(next), selectedIndex, anchor_);
}

bool MultipleSelectionModel::isSelected(int index) const {
    return std::find(selected_.begin(), selected_.end(), index) != selected_.end();
}

MultipleSelectionModel::ListenerId MultipleSelectionModel::addListener(Listener listener) {
    listeners_.push_back({++lastListener_, std::move(listener)});
    return lastListener_;
}

void MultipleSelectionModel::removeListener(ListenerId id) {
    listeners_.erase(std::remove_if(listeners_.begin(), listeners_.end(),
                                    [id](const Entry& entry) { return entry.id == id; }),
                     listeners_.end());
}

void MultipleSelectionModel::itemsInserted(int index, int count) {
    if (count <= 0) {
        return;
    }
    auto shift = [index, count](int value) { return value >= index ? value + count : value; };
    std::vector<int> next = selected_;
    for (int& value : next) {
        value = shift(value);
    }
    assign(std::move(next), shift(selectedIndex_), shift(anchor_));
}

void MultipleSelectionModel::itemsRemoved(int index, int count) {
    if (count <= 0) {
        return;
    }
    const int end = index + count;
    auto gone = [index, end](int value) { return value >= index && value < end; };
    auto shift = [end, count](int value) { return value >= end ? value - count : value; };
    std::vector<int> next;
    for (const int value : selected_) {
        if (!gone(value)) {
            next.push_back(shift(value));
        }
    }
    int selectedIndex = gone(selectedIndex_) ? (next.empty() ? -1 : next.back()) : shift(selectedIndex_);
    int anchor = anchor_;
    if (gone(anchor)) {
        anchor = std::min(index, itemCount() - 1);
    } else {
        anchor = shift(anchor);
    }
    assign(std::move(next), selectedIndex, anchor);
}

void FocusModel::focus(int index) {
    if (index < -1 || index >= itemCount()) {
        index = -1;
    }
    if (index == focused_) {
        return;
    }
    focused_ = index;
    if (onChanged_) {
        onChanged_();
    }
}

void FocusModel::focusPrevious() {
    if (focused_ > 0) {
        focus(focused_ - 1);
    } else if (focused_ < 0 && itemCount() > 0) {
        focus(0);
    }
}

void FocusModel::focusNext() {
    if (focused_ + 1 < itemCount()) {
        focus(focused_ + 1);
    }
}

void FocusModel::itemsInserted(int index, int count) {
    if (count > 0 && focused_ >= index) {
        focused_ += count;
        if (onChanged_) {
            onChanged_();
        }
    }
}

void FocusModel::itemsRemoved(int index, int count) {
    if (count <= 0 || focused_ < index) {
        return;
    }
    // A removed row hands the focus to the row that takes its place.
    focused_ = focused_ < index + count ? std::min(index, itemCount() - 1) : focused_ - count;
    if (onChanged_) {
        onChanged_();
    }
}

}  // namespace jadefx

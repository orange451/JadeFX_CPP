#pragma once

#include <cstddef>
#include <functional>
#include <vector>

namespace jadefx {

// Single keeps one row selected. Multiple adds Ctrl or Command and a click to
// add or remove a row, and Shift and a click, or Shift and an arrow key, to
// select every row from the anchor to that one.
enum class SelectionMode { Single, Multiple };

// Row selection by index, in the shape of OpenJFX MultipleSelectionModel.
// ListView and TableView own one each and keep it in step with their items: a
// row inserted above a selected row moves that selection down, and a removed
// row leaves it. selectedIndex is the row picked last, or -1 when none is.
// The anchor is where Shift ranges start: the last row picked without Shift.
class MultipleSelectionModel {
public:
    using Listener = std::function<void()>;
    using ListenerId = std::size_t;

    virtual ~MultipleSelectionModel() = default;

    void setSelectionMode(SelectionMode mode);
    SelectionMode getSelectionMode() const { return mode_; }

    // Adds the row in Multiple mode, and replaces the selection in Single mode.
    void select(int index);
    // Replaces the selection with this row.
    void clearAndSelect(int index);
    // Adds each row in order. The last becomes selectedIndex. Single mode keeps only the last.
    void selectIndices(const std::vector<int>& indices);
    // Rows from start up to end, not including end. start may be past end, to
    // select upward; selectedIndex is then the smaller end. Single mode keeps the last.
    void selectRange(int start, int end);
    void selectAll();
    void selectFirst();
    void selectLast();
    // The row before or after selectedIndex, alone.
    void selectPrevious();
    void selectNext();
    void clearSelection();
    void clearSelection(int index);

    bool isSelected(int index) const;
    bool isEmpty() const { return selected_.empty(); }
    int getSelectedIndex() const { return selectedIndex_; }
    // In the order they were selected.
    const std::vector<int>& getSelectedIndices() const { return selected_; }
    int getAnchor() const { return anchor_; }
    void setAnchor(int index) { anchor_ = index; }

    // Runs after the selection changes. The id removes the listener again.
    ListenerId addListener(Listener listener);
    void removeListener(ListenerId id);

    // The owning view reports its row count and changes to its rows.
    void setItemCount(std::function<int()> count) { count_ = std::move(count); }
    void itemsInserted(int index, int count);
    void itemsRemoved(int index, int count);

private:
    int itemCount() const { return count_ ? count_() : 0; }
    bool valid(int index) const { return index >= 0 && index < itemCount(); }
    // Replaces the whole selection and tells the listeners once, if it changed.
    void assign(std::vector<int> indices, int selectedIndex, int anchor);

    SelectionMode mode_ = SelectionMode::Single;
    std::vector<int> selected_;
    int selectedIndex_ = -1;
    int anchor_ = -1;
    std::function<int()> count_;
    struct Entry {
        ListenerId id = 0;
        Listener listener;
    };
    std::vector<Entry> listeners_;
    ListenerId lastListener_ = 0;
};

// The row the keyboard acts on, in the shape of OpenJFX FocusModel. In
// Multiple mode, Ctrl or Command with an arrow key moves it without selecting.
class FocusModel {
public:
    using Listener = std::function<void()>;

    void focus(int index);
    int getFocusedIndex() const { return focused_; }
    bool isFocused(int index) const { return index >= 0 && index == focused_; }
    void focusPrevious();
    void focusNext();

    // Runs after the focused row changes.
    void setOnFocusChanged(Listener listener) { onChanged_ = std::move(listener); }

    void setItemCount(std::function<int()> count) { count_ = std::move(count); }
    void itemsInserted(int index, int count);
    void itemsRemoved(int index, int count);

private:
    int itemCount() const { return count_ ? count_() : 0; }

    int focused_ = -1;
    std::function<int()> count_;
    Listener onChanged_;
};

}  // namespace jadefx

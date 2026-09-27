#include "jadefx/scene/controls/ListView.hpp"

#include "VirtualFlow.hpp"

namespace jadefx {

namespace {

// JavaFX's ListView asks for this much room unless told otherwise.
constexpr double kPrefBreadth = 250;
constexpr double kPrefLength = 400;

}  // namespace

ListViewBase::ListViewBase(std::unique_ptr<MultipleSelectionModel> selection)
    : RowViewBase(std::move(selection), Orientation::Vertical) {
    getClassList().add("list-view");
}

void ListViewBase::edit(int index) {
    if (index >= itemCount() || (index >= 0 && !isEditable())) {
        index = -1;
    }
    if (index == editingIndex_) {
        return;
    }
    const int previous = editingIndex_;
    editingIndex_ = index;
    if (IndexedCell* cell = flow().getVisibleCell(previous); cell != nullptr && cell->isEditing()) {
        cell->cancelEdit();
    }
    if (index < 0) {
        return;
    }
    // The row starts editing when it is bound, if it is not on screen yet.
    flow().show(index);
    if (IndexedCell* cell = flow().getVisibleCell(index); cell != nullptr && !cell->isEditing()) {
        cell->startEdit();
    }
}

void ListViewBase::syncEditing(IndexedCell& row) {
    const int index = row.getIndex();
    if (index >= 0 && index == editingIndex_ && !row.isEditing()) {
        row.startEdit();
    } else if (row.isEditing() && index != editingIndex_) {
        row.cancelEdit();
    }
}

double ListViewBase::preferredContentWidth(double) const {
    return rowOrientation() == Orientation::Vertical ? kPrefBreadth : kPrefLength;
}

double ListViewBase::preferredContentHeight(double) const {
    return rowOrientation() == Orientation::Vertical ? kPrefLength : kPrefBreadth;
}

}  // namespace jadefx

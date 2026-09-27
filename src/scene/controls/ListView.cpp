#include "jadefx/scene/controls/ListView.hpp"

#include "VirtualFlow.hpp"

#include <algorithm>
#include <cmath>

namespace jadefx {

namespace {

// JavaFX's ListView asks for this much room unless told otherwise.
constexpr double kPrefBreadth = 250;
constexpr double kPrefLength = 400;

}  // namespace

ListViewBase::ListViewBase(std::unique_ptr<MultipleSelectionModel> selection) : selection_(std::move(selection)) {
    getClassList().add("list-view");
    setBackground(Color::white());
    flow_ = std::make_shared<VirtualFlow>();
    flow_->setFocusTraversable(false);
    installCellFactory();
    flow_->setCellBinder([this](IndexedCell& cell, int index) {
        bindCell(cell, index);
        syncCell(cell);
    });
    selection_->setItemCount([this] { return itemCount(); });
    focus_.setItemCount([this] { return itemCount(); });
    selectionListener_ = selection_->addListener([this] { syncCells(); });
    focus_.setOnFocusChanged([this] { syncCells(); });
    children().add(flow_);
}

ListViewBase::~ListViewBase() {
    // The typed view is gone, so its cells must not be bound again.
    flow_->setCellBinder(nullptr);
    selection_->removeListener(selectionListener_);
    focus_.setOnFocusChanged(nullptr);
}

void ListViewBase::setOrientation(Orientation orientation) {
    orientation_ = orientation;
    flow_->setVertical(orientation == Orientation::Vertical);
    setPseudoState("vertical", orientation == Orientation::Vertical);
    setPseudoState("horizontal", orientation == Orientation::Horizontal);
}

void ListViewBase::setFixedCellSize(double size) {
    fixedCellSize_ = size;
    flow_->setFixedCellSize(size);
}

void ListViewBase::setPlaceholder(std::shared_ptr<Node> placeholder) {
    if (placeholder_) {
        Node* old = placeholder_.get();
        children().removeIf([old](const std::shared_ptr<Node>& child) { return child.get() == old; });
    }
    placeholder_ = std::move(placeholder);
    if (placeholder_) {
        children().add(placeholder_);
    }
}

void ListViewBase::setEditable(bool editable) {
    editable_ = editable;
    if (!editable_) {
        edit(-1);
    }
}

void ListViewBase::edit(int index) {
    if (index >= itemCount() || (index >= 0 && !editable_)) {
        index = -1;
    }
    if (index == editingIndex_) {
        return;
    }
    const int previous = editingIndex_;
    editingIndex_ = index;
    if (IndexedCell* cell = flow_->getVisibleCell(previous); cell != nullptr && cell->isEditing()) {
        cell->cancelEdit();
    }
    if (index < 0) {
        return;
    }
    // The row starts editing when it is bound, if it is not on screen yet.
    flow_->show(index);
    if (IndexedCell* cell = flow_->getVisibleCell(index); cell != nullptr && !cell->isEditing()) {
        cell->startEdit();
    }
}

void ListViewBase::scrollTo(int index) { flow_->scrollTo(index); }

void ListViewBase::refresh() { flow_->refresh(); }

void ListViewBase::installCellFactory() {
    // A press on a cell focuses the view, which owns the keys.
    flow_->setCellFactory([this] {
        std::shared_ptr<IndexedCell> cell = createCell();
        if (cell) {
            cell->setFocusTraversable(false);
        }
        return cell;
    });
}

void ListViewBase::cellFactoryChanged() {
    edit(-1);
    installCellFactory();
}

void ListViewBase::itemsInserted(int index, int count) {
    selection_->itemsInserted(index, count);
    focus_.itemsInserted(index, count);
    if (editingIndex_ >= index) {
        edit(-1);
    }
    flow_->setCellCount(itemCount());
    flow_->refresh();
}

void ListViewBase::itemsRemoved(int index, int count) {
    if (editingIndex_ >= index) {
        edit(-1);
    }
    selection_->itemsRemoved(index, count);
    focus_.itemsRemoved(index, count);
    flow_->setCellCount(itemCount());
    flow_->refresh();
}

void ListViewBase::itemReplaced(int) { flow_->refresh(); }

void ListViewBase::itemsReset() {
    edit(-1);
    selection_->clearSelection();
    focus_.focus(-1);
    flow_->setCellCount(itemCount());
    flow_->refresh();
}

void ListViewBase::syncCell(IndexedCell& cell) {
    const int index = cell.getIndex();
    const bool filled = index >= 0 && index < itemCount();
    cell.updateSelected(filled && selection_->isSelected(index));
    cell.updateFocused(filled && focus_.isFocused(index) && isFocused());
    if (filled && index == editingIndex_ && !cell.isEditing()) {
        cell.startEdit();
    } else if (cell.isEditing() && index != editingIndex_) {
        cell.cancelEdit();
    }
}

void ListViewBase::syncCells() {
    flow_->forEachVisibleCell([this](IndexedCell& cell) { syncCell(cell); });
}

void ListViewBase::handleFocusGained() { syncCells(); }

void ListViewBase::handleFocusLost() { syncCells(); }

void ListViewBase::cellPressed(IndexedCell& cell, const MouseEvent& event) {
    if (event.button != 0 || isDisabled()) {
        return;
    }
    const int index = cell.getIndex();
    if (cell.isEmpty() || index < 0 || index >= itemCount()) {
        return;
    }
    const bool multiple = selection_->getSelectionMode() == SelectionMode::Multiple;
    if (multiple && event.shortcut()) {
        if (selection_->isSelected(index)) {
            selection_->clearSelection(index);
        } else {
            selection_->select(index);
        }
    } else if (multiple && event.shift() && selection_->getAnchor() >= 0) {
        const int anchor = selection_->getAnchor();
        selection_->clearSelection();
        selection_->setAnchor(anchor);
        selection_->selectRange(anchor, index >= anchor ? index + 1 : index - 1);
    } else {
        selection_->clearAndSelect(index);
    }
    focus_.focus(index);
    if (event.clickCount == 2 && editable_) {
        edit(index);
    }
}

int ListViewBase::pageStep() const {
    const double length = flow_->getCellLength();
    return std::max(1, static_cast<int>(std::floor(flow_->getViewportLength() / std::max(1.0, length))) - 1);
}

void ListViewBase::moveTo(int index, const KeyEvent& event) {
    const int count = itemCount();
    if (count == 0) {
        return;
    }
    index = std::clamp(index, 0, count - 1);
    const bool multiple = selection_->getSelectionMode() == SelectionMode::Multiple;
    if (multiple && event.shortcut()) {
        focus_.focus(index);
    } else if (multiple && event.shift) {
        int anchor = selection_->getAnchor();
        if (anchor < 0) {
            anchor = std::max(0, focus_.getFocusedIndex());
        }
        selection_->clearSelection();
        selection_->setAnchor(anchor);
        selection_->selectRange(anchor, index >= anchor ? index + 1 : index - 1);
        focus_.focus(index);
    } else {
        selection_->clearAndSelect(index);
        focus_.focus(index);
    }
    flow_->show(index);
}

void ListViewBase::handleKey(KeyEvent& event) {
    if (!event.pressed || isDisabled()) {
        return;
    }
    if (editingIndex_ >= 0) {
        if (event.key == Key::Escape) {
            edit(-1);
            event.consume();
        }
        return;
    }
    const bool vertical = orientation_ == Orientation::Vertical;
    const int focused = focus_.getFocusedIndex();
    const int count = itemCount();
    const bool multiple = selection_->getSelectionMode() == SelectionMode::Multiple;
    switch (event.key) {
        case Key::Up:
        case Key::Left:
            if ((event.key == Key::Up) != vertical) {
                return;
            }
            moveTo(focused < 0 ? 0 : focused - 1, event);
            break;
        case Key::Down:
        case Key::Right:
            if ((event.key == Key::Down) != vertical) {
                return;
            }
            moveTo(focused + 1, event);
            break;
        case Key::Home:
            moveTo(0, event);
            break;
        case Key::End:
            moveTo(count - 1, event);
            break;
        case Key::PageUp:
            moveTo(std::max(0, focused) - pageStep(), event);
            break;
        case Key::PageDown:
            moveTo(std::max(0, focused) + pageStep(), event);
            break;
        case Key::Space:
            if (focused < 0) {
                return;
            }
            if (multiple && event.shortcut() && selection_->isSelected(focused)) {
                selection_->clearSelection(focused);
            } else if (multiple && event.shortcut()) {
                selection_->select(focused);
            } else {
                selection_->clearAndSelect(focused);
            }
            break;
        case Key::A:
            if (!event.shortcut() || !multiple) {
                return;
            }
            selection_->selectAll();
            break;
        case Key::F2:
            if (!editable_ || focused < 0) {
                return;
            }
            edit(focused);
            break;
        default:
            return;
    }
    event.consume();
}

void ListViewBase::layoutChildren() {
    const double left = contentLeft();
    const double top = contentTop();
    const double width = contentWidth();
    const double height = contentHeight();
    flow_->performLayout(left, top, width, height);
    if (!placeholder_) {
        return;
    }
    const bool empty = itemCount() == 0;
    placeholder_->setVisible(empty);
    if (!empty) {
        placeholder_->performLayout(0, 0, 0, 0);
        return;
    }
    const double placeholderWidth = std::min(width, placeholder_->measuredWidth(width));
    const double placeholderHeight = std::min(height, placeholder_->measuredHeight(placeholderWidth, height));
    placeholder_->performLayout(left + (width - placeholderWidth) * 0.5, top + (height - placeholderHeight) * 0.5,
                                placeholderWidth, placeholderHeight);
}

double ListViewBase::preferredContentWidth(double) const {
    return orientation_ == Orientation::Vertical ? kPrefBreadth : kPrefLength;
}

double ListViewBase::preferredContentHeight(double) const {
    return orientation_ == Orientation::Vertical ? kPrefLength : kPrefBreadth;
}

}  // namespace jadefx

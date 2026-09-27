#include "jadefx/scene/controls/RowViewBase.hpp"

#include "VirtualFlow.hpp"

#include <algorithm>
#include <cmath>

namespace jadefx {

RowViewBase::RowViewBase(std::unique_ptr<MultipleSelectionModel> selection, Orientation orientation)
    : selection_(std::move(selection)) {
    setBackground(Color::white());
    flow_ = std::make_shared<VirtualFlow>();
    flow_->setFocusTraversable(false);
    installRowFactory();
    flow_->setCellBinder([this](IndexedCell& row, int index) {
        bindRow(row, index);
        syncRow(row);
    });
    selection_->setItemCount([this] { return itemCount(); });
    focus_.setItemCount([this] { return itemCount(); });
    selectionListener_ = selection_->addListener([this] { syncRows(); });
    focus_.setOnFocusChanged([this] { syncRows(); });
    children().add(flow_);
    setRowOrientation(orientation);
}

RowViewBase::~RowViewBase() {
    // The typed view is gone, so its rows must not be bound again.
    flow_->setCellBinder(nullptr);
    selection_->removeListener(selectionListener_);
    focus_.setOnFocusChanged(nullptr);
}

void RowViewBase::installRowFactory() {
    // A press on a row focuses the view, which owns the keys.
    flow_->setCellFactory([this] {
        std::shared_ptr<IndexedCell> row = createRow();
        if (row) {
            row->setFocusTraversable(false);
        }
        return row;
    });
}

void RowViewBase::rowFactoryChanged() {
    cancelEditing();
    installRowFactory();
}

void RowViewBase::setRowOrientation(Orientation orientation) {
    orientation_ = orientation;
    flow_->setVertical(orientation == Orientation::Vertical);
    setPseudoState("vertical", orientation == Orientation::Vertical);
    setPseudoState("horizontal", orientation == Orientation::Horizontal);
}

void RowViewBase::setFixedCellSize(double size) {
    fixedCellSize_ = size;
    flow_->setFixedCellSize(size);
}

void RowViewBase::setPlaceholder(std::shared_ptr<Node> placeholder) {
    if (placeholder_) {
        Node* old = placeholder_.get();
        children().removeIf([old](const std::shared_ptr<Node>& child) { return child.get() == old; });
    }
    placeholder_ = std::move(placeholder);
    if (placeholder_) {
        children().add(placeholder_);
    }
}

void RowViewBase::setEditable(bool editable) {
    editable_ = editable;
    if (!editable_) {
        cancelEditing();
    }
}

void RowViewBase::scrollTo(int index) { flow_->scrollTo(index); }

void RowViewBase::refresh() { flow_->refresh(); }

void RowViewBase::itemsInserted(int index, int count) {
    cancelEditing();
    selection_->itemsInserted(index, count);
    focus_.itemsInserted(index, count);
    flow_->setCellCount(itemCount());
    flow_->refresh();
}

void RowViewBase::itemsRemoved(int index, int count) {
    cancelEditing();
    selection_->itemsRemoved(index, count);
    focus_.itemsRemoved(index, count);
    flow_->setCellCount(itemCount());
    flow_->refresh();
}

void RowViewBase::itemReplaced(int) { flow_->refresh(); }

void RowViewBase::itemsReset(bool keepSelection) {
    cancelEditing();
    if (!keepSelection) {
        selection_->clearSelection();
        focus_.focus(-1);
    }
    flow_->setCellCount(itemCount());
    flow_->refresh();
}

void RowViewBase::syncRow(IndexedCell& row) {
    const int index = row.getIndex();
    const bool filled = index >= 0 && index < itemCount();
    row.updateSelected(filled && selection_->isSelected(index));
    row.updateFocused(filled && focus_.isFocused(index) && isFocused());
    syncEditing(row);
}

void RowViewBase::syncRows() {
    flow_->forEachVisibleCell([this](IndexedCell& row) { syncRow(row); });
}

void RowViewBase::handleFocusGained() { syncRows(); }

void RowViewBase::handleFocusLost() { syncRows(); }

void RowViewBase::rowPressed(int index, const MouseEvent& event) {
    if (event.button != 0 || isDisabled() || index < 0 || index >= itemCount()) {
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
        rowActivated(index);
    }
}

int RowViewBase::pageStep() const {
    const double length = flow_->getCellLength();
    return std::max(1, static_cast<int>(std::floor(flow_->getViewportLength() / std::max(1.0, length))) - 1);
}

void RowViewBase::moveTo(int index, const KeyEvent& event) {
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

void RowViewBase::handleKey(KeyEvent& event) {
    if (!event.pressed || isDisabled()) {
        return;
    }
    if (isEditingAny()) {
        if (event.key == Key::Escape) {
            cancelEditing();
            event.consume();
        }
        return;
    }
    const bool vertical = orientation_ == Orientation::Vertical;
    const int focused = focus_.getFocusedIndex();
    const int count = itemCount();
    const bool multiple = selection_->getSelectionMode() == SelectionMode::Multiple;
    const int back = vertical ? Key::Up : Key::Left;
    const int forward = vertical ? Key::Down : Key::Right;
    if (event.key == back) {
        moveTo(focused < 0 ? 0 : focused - 1, event);
    } else if (event.key == forward) {
        moveTo(focused + 1, event);
    } else if (event.key == Key::Up || event.key == Key::Down || event.key == Key::Left || event.key == Key::Right) {
        if (!handleCrossKey(event.key)) {
            return;
        }
    } else if (event.key == Key::Home) {
        moveTo(0, event);
    } else if (event.key == Key::End) {
        moveTo(count - 1, event);
    } else if (event.key == Key::PageUp) {
        moveTo(std::max(0, focused) - pageStep(), event);
    } else if (event.key == Key::PageDown) {
        moveTo(std::max(0, focused) + pageStep(), event);
    } else if (event.key == Key::Space && focused >= 0) {
        if (multiple && event.shortcut() && selection_->isSelected(focused)) {
            selection_->clearSelection(focused);
        } else if (multiple && event.shortcut()) {
            selection_->select(focused);
        } else {
            selection_->clearAndSelect(focused);
        }
    } else if (event.key == Key::A && event.shortcut() && multiple) {
        selection_->selectAll();
    } else if (event.key == Key::F2 && editable_ && focused >= 0) {
        editFocused();
    } else {
        return;
    }
    event.consume();
}

void RowViewBase::layoutRows(double x, double y, double width, double height) {
    flow_->performLayout(x, y, width, height);
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
    placeholder_->performLayout(x + (width - placeholderWidth) * 0.5, y + (height - placeholderHeight) * 0.5,
                                placeholderWidth, placeholderHeight);
}

void RowViewBase::layoutChildren() { layoutRows(contentLeft(), contentTop(), contentWidth(), contentHeight()); }

}  // namespace jadefx

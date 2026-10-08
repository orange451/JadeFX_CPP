#pragma once

#include "jadefx/scene/controls/RowViewBase.hpp"
#include "jadefx/scene/controls/TableColumn.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <numeric>
#include <vector>

namespace jadefx {

// How a TableView sizes its columns, as OpenJFX's resize policies do.
// Unconstrained gives each column its preferred width and scrolls sideways when
// they are wider than the table. Constrained stretches or shrinks them, in
// proportion to their preferred widths, to fill the table's width exactly.
enum class ColumnResizePolicy { Unconstrained, Constrained };

// A cell of a table: a row and a column. A row of -1 is no cell.
struct TablePosition {
    int row = -1;
    TableColumnBase* column = nullptr;
};

class TableColumnHeader;

// The part of TableView that does not depend on the row type: columns, the
// header, widths, sorting, and which cell is being edited. TableView<S> below
// is the class to use.
class TableViewBase : public RowViewBase {
public:
    ~TableViewBase() override;

    const char* getElementType() const override { return "table"; }

    // The columns in order. Adding one shows it; removing one hides it.
    ObservableList<std::shared_ptr<TableColumnBase>>& getColumns() { return columns_; }
    const ObservableList<std::shared_ptr<TableColumnBase>>& getColumns() const { return columns_; }
    std::vector<TableColumnBase*> getVisibleLeafColumns() const;

    // Only fitColumns, in this table's own layoutChildren, reads the policy.
    void setColumnResizePolicy(ColumnResizePolicy policy) {
        setLayoutValue(resizePolicy_, policy, LayoutDirt::Arrange);
    }
    ColumnResizePolicy getColumnResizePolicy() const { return resizePolicy_; }

    // The columns the rows are sorted by, first to last. A header click sorts by
    // its column alone, and Shift and a click adds it to the order. A column
    // sorts by its sortType.
    const std::vector<TableColumnBase*>& getSortOrder() const { return sortOrder_; }
    // Sorts by these columns. Columns not in the table are skipped.
    void setSortOrder(std::vector<TableColumnBase*> order);
    // Sorts the items again by the sort order, as after a change to the items.
    void sort();
    // Runs after each sort.
    void setOnSort(std::function<void()> handler) { onSort_ = std::move(handler); }

    // Starts editing a cell, scrolling it into view. A row of -1 cancels the edit.
    void edit(int row, TableColumnBase* column);
    TablePosition getEditingCell() const { return {editRow_, editColumn_}; }
    // The column the keyboard acts on: Left and Right move it, and F2 edits in it.
    TableColumnBase* getFocusedColumn() const { return focusedColumn_; }

    // A press on a cell. Cells call this.
    void cellPressed(int row, TableColumnBase* column, const MouseEvent& event);

protected:
    explicit TableViewBase(std::unique_ptr<MultipleSelectionModel> selection);

    // Puts the items in the order sort chose. The typed table does this.
    virtual void applySort(const std::vector<TableColumnBase*>& order) = 0;
    // Moves the selection and focus after a sort: where[i] is the new row of old row i.
    void remapRows(const std::vector<int>& where);

    std::shared_ptr<IndexedCell> createRow() override;
    void bindRow(IndexedCell& row, int index) override;
    void syncEditing(IndexedCell& row) override;
    void editFocused() override;
    void rowActivated(int index) override { edit(index, focusedColumn_); }
    void cancelEditing() override { edit(-1, nullptr); }
    bool isEditingAny() const override { return editRow_ >= 0; }
    bool handleCrossKey(int key) override;

    void layoutChildren() override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;

private:
    friend class TableColumnHeader;
    struct Header;

    void columnAdded(const std::shared_ptr<TableColumnBase>& column);
    // The column list or a column changed: lays out and binds the rows again.
    void columnsChanged();
    void columnRemoved(const std::shared_ptr<TableColumnBase>& column);
    // Sets each visible column's width by the resize policy.
    void fitColumns(const std::vector<TableColumnBase*>& visible, double available);
    // Rows rebuild their cells when the visible columns or their cell factories change.
    void checkColumns(const std::vector<TableColumnBase*>& visible);
    void layoutHeader(const std::vector<TableColumnBase*>& visible, double left, double top, double width,
                      double height);

    // Header gestures: resizing a column, sorting by a click, dragging a column
    // to another place.
    void headerPressed(TableColumnBase& column, bool resize, const MouseEvent& event);
    void headerDragged(const MouseEvent& event);
    void headerReleased(const MouseEvent& event);
    void toggleSort(TableColumnBase& column, bool add);
    // Scrolls sideways the least distance that shows the whole column.
    void showColumn(const TableColumnBase& column);
    // Where a dragged column would land: the index among visible columns.
    int dropIndex(double x) const;

    ObservableList<std::shared_ptr<TableColumnBase>> columns_;
    std::unique_ptr<Header> header_;
    ColumnResizePolicy resizePolicy_ = ColumnResizePolicy::Unconstrained;
    std::vector<TableColumnBase*> sortOrder_;
    std::function<void()> onSort_;
    int editRow_ = -1;
    TableColumnBase* editColumn_ = nullptr;
    TableColumnBase* focusedColumn_ = nullptr;
};

// Rows of items under a row of column headers, in the shape of OpenJFX
// TableView. S is the row item. getColumns holds TableColumn<S, T>s, each of
// which reads one value out of a row. Rows are virtualized like ListView's, and
// selection, focus, and the keyboard are RowViewBase's, by row.
// A header click sorts by that column, again for descending, and a third time
// for no sort; Shift and a click sorts by several columns. Dragging a header's
// right edge resizes the column, and dragging the header moves it.
// With setEditable, a double-click or F2 edits the focused cell through its
// column's cell, such as TextFieldTableCell, and Left and Right pick the column.
// The table's type is table, its header row thead, a header th, a row tr, and a
// cell td, as in HTML. The JavaFX names table-view, column-header,
// table-row-cell, and table-cell are there as classes too.
template <typename S>
class TableView : public TableViewBase {
public:
    TableView() : TableView(nullptr) {}

    explicit TableView(std::shared_ptr<ObservableList<S>> items)
        : TableViewBase(std::make_unique<ItemSelectionModel<S>>([this] { return &items_.get(); })) {
        items_.set(std::move(items));
    }

    ObservableList<S>& getItems() { return items_.get(); }
    const ObservableList<S>& getItems() const { return items_.get(); }
    const std::shared_ptr<ObservableList<S>>& getItemsList() const { return items_.shared(); }
    // Shows another list. Null shows an empty one. The selection is cleared.
    void setItems(std::shared_ptr<ObservableList<S>> items) { items_.set(std::move(items)); }

    ItemSelectionModel<S>& getSelectionModel() {
        return static_cast<ItemSelectionModel<S>&>(getBaseSelectionModel());
    }

protected:
    int itemCount() const override { return static_cast<int>(items_.get().size()); }

    void applySort(const std::vector<TableColumnBase*>& order) override {
        const int count = itemCount();
        if (count < 2 || order.empty()) {
            return;
        }
        std::vector<int> rows(static_cast<std::size_t>(count));
        std::iota(rows.begin(), rows.end(), 0);
        std::stable_sort(rows.begin(), rows.end(), [&order](int a, int b) {
            for (const TableColumnBase* column : order) {
                const int result = column->compareRows(a, b);
                if (result != 0) {
                    return column->getSortType() == SortType::Ascending ? result < 0 : result > 0;
                }
            }
            return false;
        });
        std::vector<S> sorted;
        sorted.reserve(rows.size());
        std::vector<int> where(rows.size());
        for (std::size_t i = 0; i < rows.size(); ++i) {
            sorted.push_back(items_.get()[static_cast<std::size_t>(rows[i])]);
            where[static_cast<std::size_t>(rows[i])] = static_cast<int>(i);
        }
        items_.reorder(std::move(sorted));
        remapRows(where);
    }

private:
    ItemsBinding<S> items_{*this};
};

template <typename S, typename T>
void TableCell<S, T>::handleMousePressed(const MouseEvent& event) {
    if (TableView<S>* table = getTableView(); table != nullptr && !this->isEmpty()) {
        table->cellPressed(this->getIndex(), column_, event);
    }
}

template <typename S, typename T>
bool TableCell<S, T>::canEdit() const {
    TableView<S>* table = getTableView();
    return table != nullptr && table->isEditable() && column_->isEditable();
}

template <typename S, typename T>
void TableCell<S, T>::requestEdit(bool editing) {
    TableView<S>* table = getTableView();
    if (table == nullptr) {
        return;
    }
    const TablePosition at = table->getEditingCell();
    const bool here = at.row == this->getIndex() && at.column == column_;
    if (editing && !here) {
        table->edit(this->getIndex(), column_);
    } else if (!editing && here) {
        table->edit(-1, nullptr);
    }
}

template <typename S, typename T>
void TableCell<S, T>::editEvent(CellEdit kind, std::optional<T> value) {
    column_->fireEdit(kind, this->getIndex(), std::move(value));
}

template <typename S, typename T>
Node* TableCell<S, T>::ownerControl() const {
    return getTableView();
}

}  // namespace jadefx

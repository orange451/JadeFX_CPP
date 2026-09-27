#pragma once

#include "jadefx/collections/ObservableList.hpp"
#include "jadefx/scene/controls/ItemCell.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>

namespace jadefx {

class TableViewBase;
template <typename S>
class TableView;
template <typename S, typename T>
class TableColumn;

enum class SortType { Ascending, Descending };

// A column of a TableView, whatever its types, in the shape of OpenJFX
// TableColumnBase. A column is not a node: the table draws its header and its
// cells. Widths are in points: prefWidth is what the column asks for, and the
// table's resize policy decides getWidth within minWidth and maxWidth.
// getStyleClass names go onto the column's header and each of its cells.
class TableColumnBase {
public:
    explicit TableColumnBase(std::string text) : text_(std::move(text)) {}
    virtual ~TableColumnBase() = default;

    TableColumnBase(const TableColumnBase&) = delete;
    TableColumnBase& operator=(const TableColumnBase&) = delete;

    void setText(std::string text) { text_ = std::move(text); }
    const std::string& getText() const { return text_; }
    // Drawn beside the header text.
    void setGraphic(std::shared_ptr<Node> graphic) { graphic_ = std::move(graphic); }
    const std::shared_ptr<Node>& getGraphic() const { return graphic_; }
    void setId(std::string id) { id_ = std::move(id); }
    const std::string& getId() const { return id_; }
    ObservableList<std::string>& getStyleClass() { return styleClass_; }
    const ObservableList<std::string>& getStyleClass() const { return styleClass_; }

    void setPrefWidth(double width) { prefWidth_ = width; }
    double getPrefWidth() const { return prefWidth_; }
    void setMinWidth(double width) { minWidth_ = width; }
    double getMinWidth() const { return minWidth_; }
    void setMaxWidth(double width) { maxWidth_ = width; }
    double getMaxWidth() const { return maxWidth_; }
    // The width as of the table's last layout.
    double getWidth() const { return width_; }

    void setVisible(bool visible) { visible_ = visible; }
    bool isVisible() const { return visible_; }
    // Lets the header's right edge be dragged.
    void setResizable(bool resizable) { resizable_ = resizable; }
    bool isResizable() const { return resizable_; }
    // Lets a header click sort by this column.
    void setSortable(bool sortable) { sortable_ = sortable; }
    bool isSortable() const { return sortable_; }
    // Lets the header be dragged to another place.
    void setReorderable(bool reorderable) { reorderable_ = reorderable; }
    bool isReorderable() const { return reorderable_; }
    // Cells in this column edit when the table is editable too.
    void setEditable(bool editable) { editable_ = editable; }
    bool isEditable() const { return editable_; }
    void setSortType(SortType type) { sortType_ = type; }
    SortType getSortType() const { return sortType_; }

    // The table this column is in, or null.
    TableViewBase* getTableViewBase() const { return table_; }

    // The table uses these. A column makes a cell, binds it to a row, and
    // compares two rows by its value.
    virtual std::shared_ptr<IndexedCell> createCell() = 0;
    virtual void bindCell(IndexedCell& cell, int row) = 0;
    virtual int compareRows(int a, int b) const = 0;
    // Bumps when the cell factory changes, so the table makes new cells.
    int getCellGeneration() const { return generation_; }

protected:
    void cellFactoryChanged() { ++generation_; }

private:
    friend class TableViewBase;

    std::string text_;
    std::shared_ptr<Node> graphic_;
    std::string id_;
    ObservableList<std::string> styleClass_;
    double prefWidth_ = 80;
    double minWidth_ = 10;
    double maxWidth_ = 5000;
    double width_ = 0;
    bool visible_ = true;
    bool resizable_ = true;
    bool sortable_ = true;
    bool reorderable_ = true;
    bool editable_ = true;
    SortType sortType_ = SortType::Ascending;
    int generation_ = 0;
    TableViewBase* table_ = nullptr;
};

// One cell of a TableView<S>, in the shape of OpenJFX TableCell. The table moves
// cells between rows as it scrolls and calls updateItem with the column's value
// for each row. The default shows a Node value as the graphic and anything else
// as text.
template <typename S, typename T>
class TableCell : public ItemCell<T> {
public:
    TableCell() { this->getClassList().add("table-cell"); }

    const char* getElementType() const override { return "td"; }

    TableColumn<S, T>* getTableColumn() const { return column_; }
    TableView<S>* getTableView() const { return column_ != nullptr ? column_->getTableView() : nullptr; }

    void updateIndex(int index) override {
        IndexedCell::updateIndex(index);
        TableView<S>* table = getTableView();
        const bool valid = table != nullptr && index >= 0 && index < static_cast<int>(table->getItems().size());
        this->bindItem(valid ? std::optional<T>(column_->getCellData(index)) : std::nullopt);
    }

    void handleMousePressed(const MouseEvent& event) override;

protected:
    bool canEdit() const override;
    void requestEdit(bool editing) override;
    void editEvent(CellEdit kind, std::optional<T> value) override;
    Node* ownerControl() const override;

private:
    friend class TableColumn<S, T>;

    TableColumn<S, T>* column_ = nullptr;
};

// A typed column: S is the row item and T the value this column shows, in the
// shape of OpenJFX TableColumn. setCellValueFactory reads T out of a row. The
// default cell shows it as text; setCellFactory draws cells another way, such
// as TextFieldTableCell for editing. setComparator orders rows when sorting,
// and without one T's operator< does. An edit commit goes to setOnEditCommit,
// or with no handler to setCellValueSetter, which writes the value back into a
// copy of the row item and replaces it in the table's list.
template <typename S, typename T>
class TableColumn : public TableColumnBase {
public:
    using CellValueFactory = std::function<T(const S&)>;
    using CellValueSetter = std::function<void(S&, const T&)>;
    using CellFactory = std::function<std::shared_ptr<TableCell<S, T>>(TableColumn<S, T>&)>;
    // Negative when a comes first, positive when b does, zero when they tie.
    using Comparator = std::function<int(const T&, const T&)>;

    // An edit starting, committing, or cancelling. newValue is set on a commit.
    struct CellEditEvent {
        int row = -1;
        TableColumn<S, T>* column = nullptr;
        std::optional<T> newValue;
    };
    using EditHandler = std::function<void(const CellEditEvent&)>;

    explicit TableColumn(std::string text = {}) : TableColumnBase(std::move(text)) {}

    TableView<S>* getTableView() const { return static_cast<TableView<S>*>(getTableViewBase()); }

    void setCellValueFactory(CellValueFactory factory) {
        valueFactory_ = std::move(factory);
        cellFactoryChanged();
    }
    void setCellValueSetter(CellValueSetter setter) { valueSetter_ = std::move(setter); }
    // Null restores the default cell.
    void setCellFactory(CellFactory factory) {
        cellFactory_ = std::move(factory);
        cellFactoryChanged();
    }
    void setComparator(Comparator comparator) { comparator_ = std::move(comparator); }

    void setOnEditStart(EditHandler handler) { onEditStart_ = std::move(handler); }
    void setOnEditCommit(EditHandler handler) { onEditCommit_ = std::move(handler); }
    void setOnEditCancel(EditHandler handler) { onEditCancel_ = std::move(handler); }

    // This column's value in a row. T{} without a value factory or a table.
    T getCellData(int row) const {
        TableView<S>* table = getTableView();
        if (!valueFactory_ || table == nullptr || row < 0 || row >= static_cast<int>(table->getItems().size())) {
            return T{};
        }
        return valueFactory_(table->getItems()[static_cast<std::size_t>(row)]);
    }

    std::shared_ptr<IndexedCell> createCell() override {
        std::shared_ptr<TableCell<S, T>> cell =
            cellFactory_ ? cellFactory_(*this) : std::make_shared<TableCell<S, T>>();
        if (cell) {
            cell->column_ = this;
        }
        return cell;
    }

    void bindCell(IndexedCell& cell, int row) override { cell.updateIndex(row); }

    int compareRows(int a, int b) const override {
        const T left = getCellData(a);
        const T right = getCellData(b);
        if (comparator_) {
            return comparator_(left, right);
        }
        if constexpr (detail::LessComparable<T>::value) {
            return left < right ? -1 : (right < left ? 1 : 0);
        } else {
            return 0;
        }
    }

    // A cell reports an edit.
    void fireEdit(CellEdit kind, int row, std::optional<T> value) {
        CellEditEvent event{row, this, std::move(value)};
        if (kind == CellEdit::Start && onEditStart_) {
            onEditStart_(event);
        } else if (kind == CellEdit::Cancel && onEditCancel_) {
            onEditCancel_(event);
        } else if (kind == CellEdit::Commit) {
            if (onEditCommit_) {
                onEditCommit_(event);
            } else if (valueSetter_ && event.newValue) {
                writeBack(row, *event.newValue);
            }
        }
    }

private:
    void writeBack(int row, const T& value) {
        TableView<S>* table = getTableView();
        if (table == nullptr || row < 0 || row >= static_cast<int>(table->getItems().size())) {
            return;
        }
        S item = table->getItems()[static_cast<std::size_t>(row)];
        valueSetter_(item, value);
        table->getItems().set(static_cast<std::size_t>(row), std::move(item));
    }

    CellValueFactory valueFactory_;
    CellValueSetter valueSetter_;
    CellFactory cellFactory_;
    Comparator comparator_;
    EditHandler onEditStart_;
    EditHandler onEditCommit_;
    EditHandler onEditCancel_;
};

// A table cell that edits its value in a TextField, in the shape of OpenJFX
// TextFieldTableCell. See TextFieldCell.
template <typename S, typename T>
class TextFieldTableCell : public TextFieldCell<TableCell<S, T>, T> {
public:
    using TextFieldCell<TableCell<S, T>, T>::TextFieldCell;

    // A cell factory for TableColumn::setCellFactory.
    static typename TableColumn<S, T>::CellFactory forTableColumn(StringConverter<T> converter = {}) {
        return [converter](TableColumn<S, T>&) { return std::make_shared<TextFieldTableCell<S, T>>(converter); };
    }
};

}  // namespace jadefx

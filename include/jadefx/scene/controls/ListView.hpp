#pragma once

#include "jadefx/collections/ObservableList.hpp"
#include "jadefx/scene/controls/Controls.hpp"
#include "jadefx/scene/controls/IndexedCell.hpp"
#include "jadefx/scene/controls/SelectionModel.hpp"
#include "jadefx/scene/controls/TextField.hpp"
#include "jadefx/util/StringConverter.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

namespace jadefx {

class VirtualFlow;

// What every ListView shares, whatever its item type: the virtualized rows,
// selection and focus, the keyboard, the placeholder, and edit state.
// ListView<T> below is the class to use.
class ListViewBase : public Controls {
public:
    ~ListViewBase() override;

    const char* getElementType() const override { return "listview"; }

    MultipleSelectionModel& getBaseSelectionModel() { return *selection_; }
    FocusModel& getFocusModel() { return focus_; }

    // Vertical rows stack downward; horizontal ones run left to right.
    void setOrientation(Orientation orientation);
    Orientation getOrientation() const { return orientation_; }
    // The length of every row. Zero or less measures the first row on screen.
    void setFixedCellSize(double size);
    double getFixedCellSize() const { return fixedCellSize_; }
    // Shown in place of the rows while there are none. Null shows nothing.
    void setPlaceholder(std::shared_ptr<Node> placeholder);
    const std::shared_ptr<Node>& getPlaceholder() const { return placeholder_; }

    // Lets cells edit. A double-click or F2 starts an edit of the focused row.
    void setEditable(bool editable);
    bool isEditable() const { return editable_; }
    // Starts editing a row, scrolling it into view. -1 cancels the edit in progress.
    void edit(int index);
    int getEditingIndex() const { return editingIndex_; }

    // Scrolls so the row is at the top, as far as the end allows.
    void scrollTo(int index);
    // Rebinds every row on screen, for items that changed without the list knowing.
    void refresh();

    // A press on a cell. Cells call this.
    void cellPressed(IndexedCell& cell, const MouseEvent& event);

    void handleKey(KeyEvent& event) override;

protected:
    explicit ListViewBase(std::unique_ptr<MultipleSelectionModel> selection);

    virtual int itemCount() const = 0;
    virtual std::shared_ptr<IndexedCell> createCell() = 0;
    // Moves a cell to a row, or to -1 to park it, and binds that row's item.
    virtual void bindCell(IndexedCell& cell, int index) = 0;
    // The cell factory changed. Cells are made again at the next layout.
    void cellFactoryChanged();

    // The typed view reports changes to its items.
    void itemsInserted(int index, int count);
    void itemsRemoved(int index, int count);
    void itemReplaced(int index);
    // The whole list was swapped.
    void itemsReset();

    void layoutChildren() override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;
    void handleFocusGained() override;
    void handleFocusLost() override;

private:
    void installCellFactory();
    // Selection, focus, and edit state on one cell, from the models.
    void syncCell(IndexedCell& cell);
    void syncCells();
    // Moves the focused row by step for a key: selecting, extending, or focus only.
    void moveTo(int index, const KeyEvent& event);
    int pageStep() const;

    std::unique_ptr<MultipleSelectionModel> selection_;
    FocusModel focus_;
    std::shared_ptr<VirtualFlow> flow_;
    std::shared_ptr<Node> placeholder_;
    Orientation orientation_ = Orientation::Vertical;
    double fixedCellSize_ = 0;
    bool editable_ = false;
    int editingIndex_ = -1;
    MultipleSelectionModel::ListenerId selectionListener_ = 0;
};

template <typename T>
class ListView;

// One row of a ListView<T>, in the shape of OpenJFX ListCell. The view moves a
// cell between rows as it scrolls and calls updateItem with each row's item.
// The default shows a Node item as the graphic and anything else as text; a
// subclass overrides updateItem to draw its own way, and calls the base first.
// T must be default-constructible: an empty cell gets T{} with empty true.
template <typename T>
class ListCell : public IndexedCell {
public:
    ListCell() { getClassList().add("list-cell"); }

    const char* getElementType() const override { return "list-cell"; }

    ListView<T>* getListView() const { return view_; }
    // The row's item, or nothing while the cell is empty.
    const std::optional<T>& getItem() const { return item_; }

    void updateIndex(int index) override {
        IndexedCell::updateIndex(index);
        const bool valid = view_ != nullptr && index >= 0 && index < static_cast<int>(view_->getItems().size());
        if (valid) {
            item_ = view_->getItems()[static_cast<std::size_t>(index)];
            updateEmpty(false);
            updateItem(*item_, false);
        } else {
            item_.reset();
            updateEmpty(true);
            updateItem(T{}, true);
        }
    }

    // Asks the view to edit this row. Does nothing unless the view is editable.
    void startEdit() override {
        if (view_ == nullptr || !view_->isEditable() || isEmpty() || isEditing()) {
            return;
        }
        IndexedCell::startEdit();
        if (view_->getEditingIndex() != getIndex()) {
            view_->edit(getIndex());
        }
        view_->fireEdit(EditKind::Start, getIndex(), std::nullopt);
    }

    void cancelEdit() override {
        if (!isEditing()) {
            return;
        }
        finishEdit();
        if (view_ != nullptr) {
            const int index = getIndex();
            if (view_->getEditingIndex() == index) {
                view_->edit(-1);
            }
            view_->fireEdit(EditKind::Cancel, index, std::nullopt);
        }
    }

    // Ends the edit with a new value. The view's onEditCommit handler stores it;
    // without one, the value replaces the item in the list.
    void commitEdit(const T& value) {
        if (!isEditing() || view_ == nullptr) {
            return;
        }
        const int index = getIndex();
        finishEdit();
        if (view_->getEditingIndex() == index) {
            view_->edit(-1);
        }
        view_->fireEdit(EditKind::Commit, index, value);
    }

    void handleMousePressed(const MouseEvent& event) override {
        if (view_ != nullptr) {
            view_->cellPressed(*this, event);
        }
    }

protected:
    virtual void updateItem(const T& item, bool empty) {
        if (empty) {
            setText({});
            setGraphic(nullptr);
            return;
        }
        if constexpr (std::is_convertible_v<T, std::shared_ptr<Node>>) {
            setText({});
            setGraphic(item);
        } else {
            setText(toDisplayString(item));
        }
    }

private:
    friend class ListView<T>;
    enum class EditKind { Start, Commit, Cancel };

    ListView<T>* view_ = nullptr;
    std::optional<T> item_;
};

// A ListView's selection, with the items the selected rows hold.
template <typename T>
class ListSelectionModel : public MultipleSelectionModel {
public:
    explicit ListSelectionModel(const ObservableList<T>* const* items) : items_(items) {}

    // The item at selectedIndex, or nothing when no row is selected.
    std::optional<T> getSelectedItem() const {
        const int index = getSelectedIndex();
        if (index < 0 || *items_ == nullptr || index >= static_cast<int>((*items_)->size())) {
            return std::nullopt;
        }
        return (**items_)[static_cast<std::size_t>(index)];
    }

    // In the order they were selected.
    std::vector<T> getSelectedItems() const {
        std::vector<T> out;
        for (const int index : getSelectedIndices()) {
            if (*items_ != nullptr && index >= 0 && index < static_cast<int>((*items_)->size())) {
                out.push_back((**items_)[static_cast<std::size_t>(index)]);
            }
        }
        return out;
    }

    using MultipleSelectionModel::select;
    // Selects the first row whose item equals this one.
    void select(const T& item) {
        if (*items_ == nullptr) {
            return;
        }
        const std::vector<T>& all = (*items_)->items();
        for (std::size_t i = 0; i < all.size(); ++i) {
            if (all[i] == item) {
                select(static_cast<int>(i));
                return;
            }
        }
    }

private:
    // The view's current list, which setItems can swap.
    const ObservableList<T>* const* items_;
};

// A scrolling list of items, in the shape of OpenJFX ListView.
// getItems is the list shown, and a change to it shows at the next layout.
// Two views can share one list through setItems. Only the rows on screen have
// cells, so a list of any length scrolls at the same speed.
// Click selects a row. In Multiple mode Ctrl or Command and a click adds or
// removes one, and Shift and a click selects a range. The arrow keys, Home,
// End, Page Up, and Page Down move the selection, with Shift to extend it and
// Ctrl or Command to move the focus alone; Ctrl or Command and Space toggles
// the focused row and Ctrl or Command and A selects all.
// With setEditable, a double-click or F2 edits a row through its cell, such as
// TextFieldListCell; Escape cancels.
// The view type is listview and a row is list-cell, so list-cell:selected,
// list-cell:hover, list-cell:empty, and list-cell:nth-child(odd) style rows.
template <typename T>
class ListView : public ListViewBase {
public:
    using CellFactory = std::function<std::shared_ptr<ListCell<T>>(ListView<T>&)>;

    // An edit starting, committing, or cancelling. newValue is set on a commit.
    struct EditEvent {
        int index = -1;
        std::optional<T> newValue;
    };
    using EditHandler = std::function<void(const EditEvent&)>;

    ListView() : ListView(std::make_shared<ObservableList<T>>()) {}

    explicit ListView(std::shared_ptr<ObservableList<T>> items)
        : ListViewBase(std::make_unique<ListSelectionModel<T>>(&itemsRaw_)) {
        setItems(std::move(items));
    }

    ~ListView() override { unwatch(); }

    ObservableList<T>& getItems() { return *items_; }
    const ObservableList<T>& getItems() const { return *items_; }
    // Shows another list. Null shows an empty one. The selection is cleared.
    void setItems(std::shared_ptr<ObservableList<T>> items) {
        unwatch();
        items_ = items ? std::move(items) : std::make_shared<ObservableList<T>>();
        itemsRaw_ = items_.get();
        watch_ = items_->addListener([this](const typename ObservableList<T>::Change& change) {
            const int index = static_cast<int>(change.index);
            switch (change.kind) {
                case ObservableList<T>::Change::Kind::Added:
                    itemsInserted(index, 1);
                    break;
                case ObservableList<T>::Change::Kind::Removed:
                    itemsRemoved(index, 1);
                    break;
                case ObservableList<T>::Change::Kind::Replaced:
                    itemReplaced(index);
                    break;
            }
        });
        itemsReset();
    }

    ListSelectionModel<T>& getSelectionModel() { return static_cast<ListSelectionModel<T>&>(getBaseSelectionModel()); }

    // Makes the cells. Null restores the default, which shows items as text.
    void setCellFactory(CellFactory factory) {
        factory_ = std::move(factory);
        cellFactoryChanged();
    }

    void setOnEditStart(EditHandler handler) { onEditStart_ = std::move(handler); }
    void setOnEditCommit(EditHandler handler) { onEditCommit_ = std::move(handler); }
    void setOnEditCancel(EditHandler handler) { onEditCancel_ = std::move(handler); }

protected:
    int itemCount() const override { return static_cast<int>(items_->size()); }

    std::shared_ptr<IndexedCell> createCell() override {
        std::shared_ptr<ListCell<T>> cell = factory_ ? factory_(*this) : std::make_shared<ListCell<T>>();
        if (cell) {
            cell->view_ = this;
        }
        return cell;
    }

    void bindCell(IndexedCell& cell, int index) override { cell.updateIndex(index); }

private:
    friend class ListCell<T>;
    using EditKind = typename ListCell<T>::EditKind;

    void fireEdit(EditKind kind, int index, std::optional<T> value) {
        const EditEvent event{index, std::move(value)};
        switch (kind) {
            case EditKind::Start:
                if (onEditStart_) {
                    onEditStart_(event);
                }
                break;
            case EditKind::Cancel:
                if (onEditCancel_) {
                    onEditCancel_(event);
                }
                break;
            case EditKind::Commit:
                if (onEditCommit_) {
                    onEditCommit_(event);
                } else if (event.newValue && index >= 0 && index < itemCount()) {
                    items_->set(static_cast<std::size_t>(index), *event.newValue);
                }
                break;
        }
    }

    void unwatch() {
        if (items_ && watch_ != 0) {
            items_->removeListener(watch_);
        }
        watch_ = 0;
    }

    std::shared_ptr<ObservableList<T>> items_;
    const ObservableList<T>* itemsRaw_ = nullptr;
    typename ObservableList<T>::ListenerId watch_ = 0;
    CellFactory factory_;
    EditHandler onEditStart_;
    EditHandler onEditCommit_;
    EditHandler onEditCancel_;
};

// A cell that edits its item in a TextField, in the shape of OpenJFX
// TextFieldListCell. Enter commits the text through the converter, and Escape
// or leaving the field cancels. Text the converter cannot read cancels too.
template <typename T>
class TextFieldListCell : public ListCell<T> {
public:
    explicit TextFieldListCell(StringConverter<T> converter = {}) : converter_(std::move(converter)) {}

    // A cell factory for ListView::setCellFactory.
    static typename ListView<T>::CellFactory forListView(StringConverter<T> converter = {}) {
        return [converter](ListView<T>&) { return std::make_shared<TextFieldListCell<T>>(converter); };
    }

    const StringConverter<T>& getConverter() const { return converter_; }

    void startEdit() override {
        ListCell<T>::startEdit();
        if (!this->isEditing()) {
            return;
        }
        if (!field_) {
            field_ = std::make_shared<TextField>();
            // The field spans the row and fits inside its padding.
            field_->setPrefWidthRatio(1);
            field_->setStyle("padding: 0 4px;");
            field_->setOnAction([this](ActionEvent&) {
                if (std::optional<T> value = converter_.parse(field_->getText())) {
                    this->commitEdit(*value);
                    showItem();
                } else {
                    this->cancelEdit();
                }
            });
            field_->setOnFocusChanged([this](bool focused) {
                if (!focused) {
                    this->cancelEdit();
                }
            });
        }
        field_->setText(this->getItem() ? converter_.format(*this->getItem()) : std::string());
        this->setText({});
        this->setGraphic(field_);
        field_->selectAll();
        field_->requestFocus();
    }

    void cancelEdit() override {
        ListCell<T>::cancelEdit();
        showItem();
    }

    void handleKey(KeyEvent& event) override {
        if (event.pressed && event.key == Key::Escape && this->isEditing()) {
            this->cancelEdit();
            event.consume();
        }
    }

protected:
    void updateItem(const T& item, bool empty) override {
        ListCell<T>::updateItem(item, empty);
        if (!empty && this->isEditing() && field_) {
            this->setText({});
            this->setGraphic(field_);
            return;
        }
        if (!empty) {
            this->setGraphic(nullptr);
            this->setText(converter_.format(item));
        }
    }

private:
    void showItem() {
        if (this->isEditing()) {
            return;
        }
        // The field leaves the cell, so the view takes the keys back.
        const bool typing = field_ && field_->isFocused();
        this->setGraphic(nullptr);
        this->setText(this->getItem() ? converter_.format(*this->getItem()) : std::string());
        if (typing && this->getListView() != nullptr) {
            this->getListView()->requestFocus();
        }
    }

    StringConverter<T> converter_;
    std::shared_ptr<TextField> field_;
};

}  // namespace jadefx

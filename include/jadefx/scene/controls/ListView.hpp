#pragma once

#include "jadefx/scene/controls/ItemCell.hpp"
#include "jadefx/scene/controls/RowViewBase.hpp"

#include <functional>
#include <memory>
#include <optional>

namespace jadefx {

// The part of ListView that does not depend on the item type: orientation and
// which row is being edited. ListView<T> below is the class to use.
class ListViewBase : public RowViewBase {
public:
    const char* getElementType() const override { return "listview"; }

    // Vertical rows stack downward; horizontal ones run left to right.
    void setOrientation(Orientation orientation) { setRowOrientation(orientation); }
    Orientation getOrientation() const { return rowOrientation(); }

    // Starts editing a row, scrolling it into view. -1 cancels the edit in progress.
    void edit(int index);
    int getEditingIndex() const { return editingIndex_; }

protected:
    explicit ListViewBase(std::unique_ptr<MultipleSelectionModel> selection);

    void syncEditing(IndexedCell& row) override;
    void editFocused() override { edit(getFocusModel().getFocusedIndex()); }
    void rowActivated(int index) override { edit(index); }
    void cancelEditing() override { edit(-1); }
    bool isEditingAny() const override { return editingIndex_ >= 0; }
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;

private:
    int editingIndex_ = -1;
};

template <typename T>
class ListView;

// One row of a ListView<T>, in the shape of OpenJFX ListCell. The view moves a
// cell between rows as it scrolls and calls updateItem with each row's item.
// The default shows a Node item as the graphic and anything else as text; a
// subclass overrides updateItem to draw its own way, and calls the base first.
template <typename T>
class ListCell : public ItemCell<T> {
public:
    ListCell() { this->getClassList().add("list-cell"); }

    const char* getElementType() const override { return "list-cell"; }

    ListView<T>* getListView() const { return view_; }

    void updateIndex(int index) override {
        IndexedCell::updateIndex(index);
        const bool valid = view_ != nullptr && index >= 0 && index < static_cast<int>(view_->getItems().size());
        this->bindItem(valid ? std::optional<T>(view_->getItems()[static_cast<std::size_t>(index)]) : std::nullopt);
    }

    void handleMousePressed(const MouseEvent& event) override {
        if (view_ != nullptr && !this->isEmpty()) {
            view_->rowPressed(this->getIndex(), event);
        }
    }

protected:
    bool canEdit() const override { return view_ != nullptr && view_->isEditable(); }

    void requestEdit(bool editing) override {
        const int index = this->getIndex();
        if (editing && view_->getEditingIndex() != index) {
            view_->edit(index);
        } else if (!editing && view_->getEditingIndex() == index) {
            view_->edit(-1);
        }
    }

    void editEvent(CellEdit kind, std::optional<T> value) override { view_->fireEdit(kind, this->getIndex(), value); }

    Node* ownerControl() const override { return view_; }

private:
    friend class ListView<T>;

    ListView<T>* view_ = nullptr;
};

// A scrolling list of items, in the shape of OpenJFX ListView.
// getItems is the list shown, and a change to it shows at the next layout.
// Two views can share one list through setItems. Only the rows on screen have
// cells, so a list of any length scrolls at the same speed. Selection, focus,
// and the keyboard are RowViewBase's.
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

    ListView() : ListView(nullptr) {}

    explicit ListView(std::shared_ptr<ObservableList<T>> items)
        : ListViewBase(std::make_unique<ItemSelectionModel<T>>([this] { return &items_.get(); })) {
        items_.set(std::move(items));
    }

    ObservableList<T>& getItems() { return items_.get(); }
    const ObservableList<T>& getItems() const { return items_.get(); }
    const std::shared_ptr<ObservableList<T>>& getItemsList() const { return items_.shared(); }
    // Shows another list. Null shows an empty one. The selection is cleared.
    void setItems(std::shared_ptr<ObservableList<T>> items) { items_.set(std::move(items)); }

    ItemSelectionModel<T>& getSelectionModel() {
        return static_cast<ItemSelectionModel<T>&>(getBaseSelectionModel());
    }

    // Makes the cells. Null restores the default, which shows items as text.
    void setCellFactory(CellFactory factory) {
        factory_ = std::move(factory);
        rowFactoryChanged();
    }

    void setOnEditStart(EditHandler handler) { onEditStart_ = std::move(handler); }
    void setOnEditCommit(EditHandler handler) { onEditCommit_ = std::move(handler); }
    void setOnEditCancel(EditHandler handler) { onEditCancel_ = std::move(handler); }

protected:
    int itemCount() const override { return static_cast<int>(items_.get().size()); }

    std::shared_ptr<IndexedCell> createRow() override {
        std::shared_ptr<ListCell<T>> cell = factory_ ? factory_(*this) : std::make_shared<ListCell<T>>();
        if (cell) {
            cell->view_ = this;
        }
        return cell;
    }

    void bindRow(IndexedCell& row, int index) override { row.updateIndex(index); }

private:
    friend class ListCell<T>;
    void fireEdit(CellEdit kind, int index, std::optional<T> value) {
        const EditEvent event{index, std::move(value)};
        if (kind == CellEdit::Start && onEditStart_) {
            onEditStart_(event);
        } else if (kind == CellEdit::Cancel && onEditCancel_) {
            onEditCancel_(event);
        } else if (kind == CellEdit::Commit) {
            if (onEditCommit_) {
                onEditCommit_(event);
            } else if (event.newValue && index >= 0 && index < itemCount()) {
                items_.get().set(static_cast<std::size_t>(index), *event.newValue);
            }
        }
    }

    ItemsBinding<T> items_{*this};
    CellFactory factory_;
    EditHandler onEditStart_;
    EditHandler onEditCommit_;
    EditHandler onEditCancel_;
};

// A list cell that edits its item in a TextField, in the shape of OpenJFX
// TextFieldListCell. See TextFieldCell.
template <typename T>
class TextFieldListCell : public TextFieldCell<ListCell<T>, T> {
public:
    using TextFieldCell<ListCell<T>, T>::TextFieldCell;

    // A cell factory for ListView::setCellFactory.
    static typename ListView<T>::CellFactory forListView(StringConverter<T> converter = {}) {
        return [converter](ListView<T>&) { return std::make_shared<TextFieldListCell<T>>(converter); };
    }
};

}  // namespace jadefx

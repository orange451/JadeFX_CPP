#pragma once

#include "jadefx/collections/ObservableList.hpp"
#include "jadefx/scene/controls/Controls.hpp"
#include "jadefx/scene/controls/IndexedCell.hpp"
#include "jadefx/scene/controls/SelectionModel.hpp"

#include <memory>
#include <vector>

namespace jadefx {

class VirtualFlow;
template <typename T>
class ItemsBinding;

// What ListView and TableView share, whatever their item type: the virtualized
// rows, row selection and focus, the mouse and keyboard, the placeholder, and
// keeping all of that in step as items come and go.
// Click selects a row. In Multiple mode Ctrl or Command and a click adds or
// removes one, and Shift and a click selects a range. The arrow keys, Home,
// End, Page Up, and Page Down move the selection, with Shift to extend it and
// Ctrl or Command to move the focus alone; Ctrl or Command and Space toggles
// the focused row and Ctrl or Command and A selects all. A double-click or F2
// asks the view to edit, and Escape cancels.
class RowViewBase : public Controls {
public:
    ~RowViewBase() override;

    MultipleSelectionModel& getBaseSelectionModel() { return *selection_; }
    FocusModel& getFocusModel() { return focus_; }

    // The length of every row. Zero or less measures the first row on screen.
    void setFixedCellSize(double size);
    double getFixedCellSize() const { return fixedCellSize_; }
    // Shown in place of the rows while there are none. Null shows nothing.
    void setPlaceholder(std::shared_ptr<Node> placeholder);
    const std::shared_ptr<Node>& getPlaceholder() const { return placeholder_; }
    // Lets cells edit.
    void setEditable(bool editable);
    bool isEditable() const { return editable_; }

    // Scrolls so the row is at the top, as far as the end allows.
    void scrollTo(int index);
    // Rebinds every row on screen, for items that changed without the list knowing.
    void refresh();

    // A press on a row, or on a cell in a row. Rows and cells call this.
    void rowPressed(int index, const MouseEvent& event);

    void handleKey(KeyEvent& event) override;

protected:
    template <typename T>
    friend class ItemsBinding;

    RowViewBase(std::unique_ptr<MultipleSelectionModel> selection, Orientation orientation);

    virtual int itemCount() const = 0;
    // The row cell the flow shows: a ListCell for a list, a TableRow for a table.
    virtual std::shared_ptr<IndexedCell> createRow() = 0;
    // Moves a row to an item, or to -1 to park it.
    virtual void bindRow(IndexedCell& row, int index) = 0;
    // Brings a row's edit state in line with the view's. Runs after every bind and selection change.
    virtual void syncEditing(IndexedCell&) {}
    // Starts editing the focused row, for F2.
    virtual void editFocused() {}
    // A second click on a row, which can start an edit.
    virtual void rowActivated(int) {}
    // Ends any edit without committing. Runs before rows move.
    virtual void cancelEditing() {}
    virtual bool isEditingAny() const { return false; }
    // Left and Right in a vertical view. False leaves the key to the ancestors.
    virtual bool handleCrossKey(int) { return false; }

    // Rows are made again at the next layout, after the row factory changed.
    void rowFactoryChanged();
    // The typed view reports changes to its items.
    void itemsInserted(int index, int count);
    void itemsRemoved(int index, int count);
    void itemReplaced(int index);
    // The whole list was swapped, or reordered.
    void itemsReset(bool keepSelection = false);

    void setRowOrientation(Orientation orientation);
    Orientation rowOrientation() const { return orientation_; }
    VirtualFlow& flow() const { return *flow_; }
    // Row state from the models on every row on screen.
    void syncRows();

    // Places the rows and the placeholder in a rectangle, in local points.
    void layoutRows(double x, double y, double width, double height);
    void layoutChildren() override;
    void handleFocusGained() override;
    void handleFocusLost() override;

private:
    void installRowFactory();
    void syncRow(IndexedCell& row);
    // Moves to a row for a key: selecting, extending, or moving the focus alone.
    void moveTo(int index, const KeyEvent& event);
    int pageStep() const;

    std::unique_ptr<MultipleSelectionModel> selection_;
    FocusModel focus_;
    std::shared_ptr<VirtualFlow> flow_;
    std::shared_ptr<Node> placeholder_;
    Orientation orientation_ = Orientation::Vertical;
    double fixedCellSize_ = 0;
    bool editable_ = false;
    MultipleSelectionModel::ListenerId selectionListener_ = 0;
};

// The item list a typed view shows, shared through a std::shared_ptr so two
// views can show one list. Each change to the list moves the view's rows,
// selection, and focus with it.
template <typename T>
class ItemsBinding {
public:
    explicit ItemsBinding(RowViewBase& view) : view_(view) {}
    ~ItemsBinding() { unwatch(); }

    ItemsBinding(const ItemsBinding&) = delete;
    ItemsBinding& operator=(const ItemsBinding&) = delete;

    ObservableList<T>& get() const { return *items_; }
    const std::shared_ptr<ObservableList<T>>& shared() const { return items_; }

    // Shows another list. Null shows an empty one. The selection is cleared.
    void set(std::shared_ptr<ObservableList<T>> items) {
        unwatch();
        items_ = items ? std::move(items) : std::make_shared<ObservableList<T>>();
        watch_ = items_->addListener([this](const typename ObservableList<T>::Change& change) {
            if (muted_) {
                return;
            }
            const int index = static_cast<int>(change.index);
            switch (change.kind) {
                case ObservableList<T>::Change::Kind::Added:
                    view_.itemsInserted(index, 1);
                    break;
                case ObservableList<T>::Change::Kind::Removed:
                    view_.itemsRemoved(index, 1);
                    break;
                case ObservableList<T>::Change::Kind::Replaced:
                    view_.itemReplaced(index);
                    break;
            }
        });
        view_.itemsReset();
    }

    // Rewrites the list in a new order without the view hearing each move,
    // then shows it. The caller moves the selection itself.
    void reorder(std::vector<T> items) {
        muted_ = true;
        items_->setAll(std::move(items));
        muted_ = false;
        view_.itemsReset(true);
    }

private:
    void unwatch() {
        if (items_ && watch_ != 0) {
            items_->removeListener(watch_);
        }
        watch_ = 0;
    }

    RowViewBase& view_;
    std::shared_ptr<ObservableList<T>> items_;
    typename ObservableList<T>::ListenerId watch_ = 0;
    bool muted_ = false;
};

}  // namespace jadefx

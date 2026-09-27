#pragma once

#include "jadefx/scene/controls/IndexedCell.hpp"
#include "jadefx/scene/controls/TextField.hpp"
#include "jadefx/util/StringConverter.hpp"

#include <memory>
#include <optional>
#include <type_traits>

namespace jadefx {

// Which step of an edit a cell reports to its view.
enum class CellEdit { Start, Commit, Cancel };

// A cell that shows one item of type T, the part ListCell and TableCell share,
// as OpenJFX's Cell<T> is. The owning view binds a row's item with bindItem and
// updateItem draws it: by default a Node item is the graphic and anything else
// is text. startEdit, cancelEdit, and commitEdit run the edit protocol and tell
// the view through the three hooks a ListCell or a TableCell provides.
// T must be default-constructible: an empty cell gets T{} with empty true.
template <typename T>
class ItemCell : public IndexedCell {
public:
    // The row's item, or nothing while the cell is empty.
    const std::optional<T>& getItem() const { return item_; }

    // Asks the view to edit this cell. Does nothing unless the view lets it.
    void startEdit() override {
        if (!canEdit() || isEmpty() || isEditing()) {
            return;
        }
        IndexedCell::startEdit();
        requestEdit(true);
        editEvent(CellEdit::Start, std::nullopt);
    }

    void cancelEdit() override {
        if (!isEditing()) {
            return;
        }
        finishEdit();
        requestEdit(false);
        editEvent(CellEdit::Cancel, std::nullopt);
    }

    // Ends the edit with a new value, which the view's commit handler stores.
    void commitEdit(const T& value) {
        if (!isEditing()) {
            return;
        }
        finishEdit();
        requestEdit(false);
        editEvent(CellEdit::Commit, value);
    }

protected:
    // Binds an item, or nothing to empty the cell, then draws it.
    void bindItem(std::optional<T> item) {
        item_ = std::move(item);
        updateEmpty(!item_);
        if (item_) {
            updateItem(*item_, false);
        } else {
            updateItem(T{}, true);
        }
    }

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
            setGraphic(nullptr);
            setText(toDisplayString(item));
        }
    }

    // The view lets this cell edit.
    virtual bool canEdit() const = 0;
    // This cell starts (true) or stops (false) being the one the view edits.
    virtual void requestEdit(bool editing) = 0;
    // Reports an edit to the view's handlers. value is set on a commit.
    virtual void editEvent(CellEdit kind, std::optional<T> value) = 0;
    // The control that takes the keys back when an editor closes.
    virtual Node* ownerControl() const = 0;

private:
    std::optional<T> item_;
};

// Edits a cell's item in a TextField, as OpenJFX's TextFieldListCell and
// TextFieldTableCell do. Base is the cell it extends, such as ListCell<T>.
// Enter commits the text through the converter, and Escape or leaving the field
// cancels. Text the converter cannot read cancels too.
template <typename Base, typename T>
class TextFieldCell : public Base {
public:
    explicit TextFieldCell(StringConverter<T> converter = {}) : converter_(std::move(converter)) {}

    const StringConverter<T>& getConverter() const { return converter_; }

    void startEdit() override {
        Base::startEdit();
        if (!this->isEditing()) {
            return;
        }
        if (!field_) {
            field_ = std::make_shared<TextField>();
            // The field spans the cell and fits inside its padding.
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
        Base::cancelEdit();
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
        Base::updateItem(item, empty);
        if (empty) {
            return;
        }
        if (this->isEditing() && field_) {
            this->setText({});
            this->setGraphic(field_);
            return;
        }
        this->setText(converter_.format(item));
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
        if (typing && this->ownerControl() != nullptr) {
            this->ownerControl()->requestFocus();
        }
    }

    StringConverter<T> converter_;
    std::shared_ptr<TextField> field_;
};

}  // namespace jadefx

#pragma once

#include "jadefx/collections/ObservableList.hpp"
#include "jadefx/scene/controls/ComboBoxBase.hpp"
#include "jadefx/scene/controls/TextField.hpp"

#include <memory>
#include <string>

namespace jadefx {

class ComboPopup;
class ComboRow;

// A string combo box. The arrow opens a list of items. Choosing a row, or
// pressing Enter on the highlighted row, fires the action. setValue and select
// do not. An editable box commits its text field on Enter, and again when a
// click or Escape outside the list is noticed on the next layout.
// The Up and Down keys move the selection while the list is closed.
class ComboBox : public ComboBoxBase {
    friend class ComboPopup;
    friend class ComboRow;

public:
    ComboBox();
    ~ComboBox() override;

    const char* getElementType() const override { return "combobox"; }

    ObservableList<std::string>& getItems() { return items_; }
    const ObservableList<std::string>& getItems() const { return items_; }

    // Does not fire. A value that equals an item selects the first match.
    // Any other string is kept and the index becomes -1.
    void setValue(std::string value);
    const std::string& getValue() const { return value_; }

    int getSelectionIndex() const { return selection_; }
    // Does not fire. An index outside the list, including -1, clears the value.
    void select(int index);

    void setVisibleRowCount(int rows);
    int getVisibleRowCount() const { return visibleRowCount_; }

protected:
    std::shared_ptr<Node> createPopupContent() override;
    bool canShowPopup() const override;
    void popupShowing() override;
    void popupHidden() override;
    std::string valueText() const override { return value_; }
    void editorAction() override { commitEditor(); }
    void editorFocusLost() override;
    void layoutChildren() override;
    double preferredContentWidth(double innerAvailable) const override;
    void handleKey(KeyEvent& event) override;

private:
    // Closes the list without committing the editor, after an action already took its value.
    void closeCommitted();
    void onItemsChanged();
    void fire();
    void commitEditor();
    void commitEditorIfDirty();
    void activateRow(int index);
    void moveClosedSelection(int delta);
    void moveHighlight(int delta);
    void scrollBy(double deltaY);
    void revealHighlight();
    void clampScroll();
    void relayoutPopup();
    int indexOf(const std::string& value) const;
    double viewportHeight() const;
    double popupWidth() const;
    bool rowIsArmed(int index) const;

    ObservableList<std::string> items_;
    std::string value_;
    std::shared_ptr<ComboPopup> popup_;
    int selection_ = -1;
    int highlight_ = -1;
    int visibleRowCount_ = 10;
    double scroll_ = 0;
    bool commitSuppressed_ = false;
    bool editorActionDuringKey_ = false;
    bool committingEditor_ = false;
    bool closing_ = false;
};

}  // namespace jadefx

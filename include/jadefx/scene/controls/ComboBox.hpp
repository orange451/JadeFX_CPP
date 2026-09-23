#pragma once

#include "jadefx/collections/ObservableList.hpp"
#include "jadefx/scene/controls/Controls.hpp"
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
class ComboBox : public Controls {
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

    void setPromptText(std::string text);
    const std::string& getPromptText() const { return prompt_; }
    void setVisibleRowCount(int rows);
    int getVisibleRowCount() const { return visibleRowCount_; }
    void setEditable(bool editable);
    bool isEditable() const { return editable_; }

    // Also disables the editor. Node::setDisable is not virtual; call this on the combo.
    void setDisable(bool value);

    void setOnAction(ActionHandler handler) { onAction_ = std::move(handler); }
    void show();
    void hide();
    bool isShowing() const;

    // Non-null only while the combo is editable. The field is a child of the combo
    // and its text box fills the combo's height.
    TextField* getEditor() const { return editable_ ? editor_.get() : nullptr; }

protected:
    void layoutChildren() override;
    void render(UiRenderer& renderer, float opacity) override;
    void renderContent(UiRenderer& renderer, float opacity) override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;
    void handleMousePressed(const MouseEvent& event) override;
    void handleKey(KeyEvent& event) override;
    void sceneChanged(Scene* previous) override;

private:
    void ensurePopup();
    void presentPopup();
    void onItemsChanged();
    void syncEditor();
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
    void layoutEditor();
    int indexOf(const std::string& value) const;
    double viewportHeight() const;
    double popupWidth() const;
    bool rowIsArmed(int index) const;

    ObservableList<std::string> items_;
    std::string value_;
    std::string prompt_;
    ActionHandler onAction_;
    std::shared_ptr<TextField> editor_;
    std::shared_ptr<ComboPopup> popup_;
    int selection_ = -1;
    int highlight_ = -1;
    int visibleRowCount_ = 10;
    double scroll_ = 0;
    bool editable_ = false;
    bool popupArmed_ = false;
    bool commitSuppressed_ = false;
    bool syncingEditor_ = false;
    bool editorActionDuringKey_ = false;
    bool committingEditor_ = false;
    bool hiding_ = false;
    bool closing_ = false;
};

}  // namespace jadefx

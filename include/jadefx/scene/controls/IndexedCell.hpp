#pragma once

#include "jadefx/scene/controls/Labeled.hpp"

namespace jadefx {

// One row of a virtualized view, in the shape of OpenJFX IndexedCell.
// The view reuses cells as it scrolls: updateIndex moves a cell to another row,
// and index -1 parks it. A cell is a Labeled, so text and a graphic draw it.
// Stylesheets see these states:
//   :empty            no item, such as a parked cell
//   :selected         the row is selected
//   :focus-visible    the view's focused row, while the view has the focus
//   :nth-child(...)   matched against the row's index, so odd rows stay odd
//   :editing          the row is being edited
// The user-agent stylesheet colors a hovered or selected list-cell and tr with
// the theme's --row-hover-color and --selection-color, as TreeView's rows are,
// and the cell draws a thin --outline-color ring on the focused row.
class IndexedCell : public Labeled {
public:
    // Default padding: 4 points above and below, 10 at the sides. A table's
    // headers use the same sides so their text lines up with the cells'.
    static constexpr double kPaddingY = 4;
    static constexpr double kPaddingX = 10;

    int getIndex() const { return index_; }
    bool isEmpty() const { return empty_; }
    bool isEditing() const { return editing_; }

    // Moves the cell to a row. The view calls this, then binds the row's item.
    virtual void updateIndex(int index);
    void updateSelected(bool selected);
    void updateFocused(bool focused);

    // Starts and cancels an edit. The view asks for these; a subclass that can
    // edit calls the base first, then shows its editor. commitEdit lives on the
    // typed cell, which knows the value's type.
    virtual void startEdit();
    virtual void cancelEdit();

    int getNthChildIndex() const override { return index_ >= 0 ? index_ + 1 : 0; }

protected:
    IndexedCell();

    void updateEmpty(bool empty);
    // Ends the edit without asking the view, after a commit or a cancel.
    void finishEdit();
    void renderContent(UiRenderer& renderer, float opacity) override;

private:

    int index_ = -1;
    bool empty_ = true;
    bool editing_ = false;
    bool rowFocused_ = false;
};

}  // namespace jadefx

#include "jadefx/scene/controls/IndexedCell.hpp"

#include "gl/UiRenderer.hpp"

namespace jadefx {

IndexedCell::IndexedCell() : Labeled("") {
    setDefaultCursor(Cursor::Default);
    setAlignment(Pos::CenterLeft);
    setPadding(Insets::axes(kPaddingY, kPaddingX));
    updateEmpty(true);
}

void IndexedCell::updateIndex(int index) { index_ = index; }

void IndexedCell::updateEmpty(bool empty) {
    empty_ = empty;
    setPseudoState("empty", empty);
}

void IndexedCell::updateSelected(bool selected) {
    if (selected == isSelected()) {
        return;
    }
    setSelected(selected);
    updateChrome();
}

void IndexedCell::updateFocused(bool focused) {
    rowFocused_ = focused;
    setPseudoState("focus-visible", focused);
}

void IndexedCell::startEdit() {
    if (empty_) {
        return;
    }
    editing_ = true;
    setPseudoState("editing", true);
}

void IndexedCell::cancelEdit() { finishEdit(); }

void IndexedCell::finishEdit() {
    editing_ = false;
    setPseudoState("editing", false);
}

void IndexedCell::handleHoverChanged() { updateChrome(); }

void IndexedCell::updateChrome() {
    const bool hovered = isHovered() && !empty_;
    if (isSelected()) {
        setBackground(hovered ? Color::rgb8(210, 227, 252) : Color::rgb8(232, 240, 254));
    } else {
        setBackground(hovered ? Color::rgb8(245, 245, 245) : Color::transparent());
    }
}

void IndexedCell::renderContent(UiRenderer& renderer, float opacity) {
    Labeled::renderContent(renderer, opacity);
    if (!rowFocused_) {
        return;
    }
    // The ring sits inside the cell, so neighbouring rows do not cover it.
    const float radius[4] = {};
    const float sides[4] = {1.f, 1.f, 1.f, 1.f};
    Color ring = Color::rgb8(26, 115, 232);
    ring.a *= opacity;
    renderer.strokeRounded(static_cast<float>(getAbsoluteX()), static_cast<float>(getAbsoluteY()),
                           static_cast<float>(getWidth()), static_cast<float>(getHeight()), radius, sides, ring);
}

}  // namespace jadefx

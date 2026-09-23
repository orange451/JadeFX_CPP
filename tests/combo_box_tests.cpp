#include "jadefx/jadefx.hpp"
#include "jadefx/scene/Controls/ComboBox.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

void ExpectEq(const std::string& actual, const std::string& wanted, const char* message) {
    if (actual != wanted) {
        std::fprintf(stderr, "FAIL %s (got \"%s\", wanted \"%s\")\n", message, actual.c_str(), wanted.c_str());
        ++gFailures;
    }
}

bool Near(double actual, double wanted, double tolerance = 0.6) {
    return std::fabs(actual - wanted) <= tolerance;
}

void Click(jadefx::Scene& scene, double x, double y) {
    scene.noteButton(0, true, x, y);
    scene.noteButton(0, false, x, y);
}

void ClickNode(jadefx::Scene& scene, jadefx::Node& node) {
    Click(scene, node.getAbsoluteX() + node.getWidth() * 0.5, node.getAbsoluteY() + node.getHeight() * 0.5);
}

struct Box {
    std::shared_ptr<jadefx::Pane> root;
    std::shared_ptr<jadefx::ComboBox> combo;
    std::shared_ptr<jadefx::Scene> scene;
};

Box Place(const std::shared_ptr<jadefx::ComboBox>& combo, double width = 420, double height = 360) {
    Box box;
    box.combo = combo;
    box.root = jadefx::make<jadefx::Pane>();
    box.root->setPrefSize(width, height);
    box.root->getChildren().add(combo);
    box.scene = jadefx::make<jadefx::Scene>(box.root, width, height);
    box.scene->layout(width, height, 0);
    return box;
}

jadefx::Node* PopupOf(jadefx::Scene& scene, jadefx::ComboBox& combo) {
    const double x = combo.getAbsoluteX() + 8.0;
    const double probes[] = {combo.getAbsoluteY() + combo.getHeight() + 6.0, combo.getAbsoluteY() - 6.0};
    for (double y : probes) {
        for (jadefx::Node* node = scene.pick(x, y); node != nullptr; node = node->getParent()) {
            if (std::strcmp(node->getElementType(), "combo-row") == 0) {
                return node->getParent();
            }
        }
    }
    return nullptr;
}

jadefx::Node* RowById(jadefx::Scene& scene, jadefx::Node& popup, const char* id) {
    const double x = popup.getAbsoluteX() + 8.0;
    const double bottom = popup.getAbsoluteY() + popup.getHeight();
    for (double y = popup.getAbsoluteY() + 2.0; y < bottom; y += 6.0) {
        jadefx::Node* hit = scene.pick(x, y);
        if (hit != nullptr && hit->getElementId() == id) {
            return hit;
        }
    }
    return nullptr;
}

void TestPreferredSize() {
    auto combo = jadefx::make<jadefx::ComboBox>();
    combo->setPromptText("Choose something lengthy");
    combo->getItems().add("A");
    combo->getItems().add("Bee");
    Box box = Place(combo, 800, 240);
    Expect(std::strcmp(combo->getElementType(), "combobox") == 0, "element type is combobox");
    Expect(combo->getPrefHeight() == 32.0, "pref height is 32");
    Expect(Near(combo->getHeight(), 32.0), "laid out height is 32");
    Expect(combo->getWidth() + 0.5 >= 120.0, "width is at least 120");
    const jadefx::Font face(combo->computedStyle().fontFamily, combo->computedStyle().fontSize);
    double widest = face.measureWidth(combo->getPromptText());
    widest = std::max(widest, static_cast<double>(face.measureWidth("A")));
    widest = std::max(widest, static_cast<double>(face.measureWidth("Bee")));
    Expect(combo->getWidth() + 0.5 >= widest + 36.0, "width fits the prompt or items plus the arrow");
    Expect(combo->getVisibleRowCount() == 10, "ten rows are visible by default");
    combo->setVisibleRowCount(0);
    Expect(combo->getVisibleRowCount() == 1, "visible rows do not drop below 1");
    combo->setVisibleRowCount(-3);
    Expect(combo->getVisibleRowCount() == 1, "a negative row count becomes 1");
    Expect(combo->getEditor() == nullptr, "a non-editable combo has no editor");
    ExpectEq(combo->getPromptText(), "Choose something lengthy", "prompt text is stored");
}

void TestClickSelectsSecondRow() {
    auto combo = jadefx::make<jadefx::ComboBox>();
    combo->getItems().add("Apple");
    combo->getItems().add("Berry");
    combo->getItems().add("Cherry");
    Box box = Place(combo);
    int actions = 0;
    jadefx::Node* source = nullptr;
    combo->setOnAction([&](jadefx::ActionEvent& event) {
        ++actions;
        source = event.source;
    });
    ClickNode(*box.scene, *combo);
    Expect(combo->isShowing(), "clicking the combo opens it");
    Expect(actions == 0, "opening does not fire");
    jadefx::Node* popup = PopupOf(*box.scene, *combo);
    Expect(popup != nullptr, "the popup is on screen after show");
    if (popup == nullptr) {
        return;
    }
    Expect(Near(popup->getWidth(), combo->getWidth()), "the popup matches the combo width");
    jadefx::Node* row = RowById(*box.scene, *popup, "Berry");
    Expect(row != nullptr && std::strcmp(row->getElementType(), "combo-row") == 0, "the second row is a combo-row");
    if (row == nullptr) {
        return;
    }
    Expect(Near(row->getHeight(), 28.0), "a row is 28px tall");
    ClickNode(*box.scene, *row);
    ExpectEq(combo->getValue(), "Berry", "clicking the second row sets the value");
    Expect(combo->getSelectionIndex() == 1, "clicking the second row selects index 1");
    Expect(actions == 1 && source == combo.get(), "a row click fires the combo action once");
    Expect(!combo->isShowing(), "a row click hides the popup");
    Expect(combo->isFocused(), "the combo keeps focus after the row click");
}

void TestClickTogglesWithoutAction() {
    auto combo = jadefx::make<jadefx::ComboBox>();
    combo->getItems().add("One");
    combo->getItems().add("Two");
    Box box = Place(combo);
    int actions = 0;
    combo->setOnAction([&](jadefx::ActionEvent&) { ++actions; });
    ClickNode(*box.scene, *combo);
    Expect(combo->isShowing(), "the first click opens the popup");
    ClickNode(*box.scene, *combo);
    Expect(!combo->isShowing(), "the second click closes the popup");
    Expect(actions == 0, "opening and closing do not fire");

    combo->requestFocus();
    box.scene->noteKey(jadefx::Key::Space, true, false, 0);
    Expect(combo->isShowing(), "Space opens a closed non-editable combo");
    box.scene->noteKey(jadefx::Key::Escape, true, false, 0);
    Expect(!combo->isShowing(), "Escape closes the popup");
    Expect(actions == 0, "Space and Escape do not fire");
}

void TestDownMovesSelection() {
    auto combo = jadefx::make<jadefx::ComboBox>();
    combo->getItems().add("Red");
    combo->getItems().add("Green");
    combo->getItems().add("Blue");
    Box box = Place(combo);
    int actions = 0;
    combo->setOnAction([&](jadefx::ActionEvent&) { ++actions; });
    combo->requestFocus();
    box.scene->noteKey(jadefx::Key::Down, true, false, 0);
    ExpectEq(combo->getValue(), "Red", "Down from no selection selects the first item");
    Expect(combo->getSelectionIndex() == 0, "Down from no selection lands on index 0");
    Expect(actions == 1, "Down fires when the selection changes");
    box.scene->noteKey(jadefx::Key::Down, true, false, 0);
    ExpectEq(combo->getValue(), "Green", "Down moves to the next item");
    Expect(combo->getSelectionIndex() == 1, "Down advances the index");
    Expect(actions == 2, "the second Down fires again");
    box.scene->noteKey(jadefx::Key::Up, true, false, 0);
    Expect(combo->getSelectionIndex() == 0 && actions == 3, "Up moves back and fires");
    box.scene->noteKey(jadefx::Key::Up, true, false, 0);
    Expect(combo->getSelectionIndex() == 0 && actions == 3, "Up stops on the first item");
    combo->select(2);
    box.scene->noteKey(jadefx::Key::Down, true, false, 0);
    Expect(combo->getSelectionIndex() == 2 && actions == 3, "Down stops on the last item");
    combo->select(0);
    box.scene->noteKey(jadefx::Key::Down, true, true, 0);
    Expect(combo->getSelectionIndex() == 1 && actions == 4, "a repeated Down still moves the selection");

    combo->select(2);
    combo->show();
    const int before = actions;
    box.scene->noteKey(jadefx::Key::Up, true, false, 0);
    Expect(combo->isShowing() && combo->getSelectionIndex() == 2 && actions == before,
           "Up while open moves the highlight without committing");
    box.scene->noteKey(jadefx::Key::Enter, true, false, 0);
    ExpectEq(combo->getValue(), "Green", "Enter commits the highlighted row");
    Expect(combo->getSelectionIndex() == 1, "Enter selects the highlighted index");
    Expect(actions == before + 1, "Enter fires once");
    Expect(!combo->isShowing(), "Enter hides the popup");
}

void TestSelectAndSetValueDoNotFire() {
    auto combo = jadefx::make<jadefx::ComboBox>();
    combo->getItems().add("Red");
    combo->getItems().add("Blue");
    combo->getItems().add("Red");
    Box box = Place(combo);
    int actions = 0;
    combo->setOnAction([&](jadefx::ActionEvent&) { ++actions; });
    combo->select(0);
    ExpectEq(combo->getValue(), "Red", "select sets the value");
    Expect(combo->getSelectionIndex() == 0 && actions == 0, "select does not fire");
    combo->select(2);
    Expect(combo->getSelectionIndex() == 2 && combo->getValue() == "Red" && actions == 0,
           "select keeps the requested index");
    combo->select(9);
    Expect(combo->getValue().empty() && combo->getSelectionIndex() == -1 && actions == 0,
           "an out-of-range select clears the value");
    combo->setValue("Blue");
    Expect(combo->getSelectionIndex() == 1 && actions == 0, "setValue of an item selects it and does not fire");
    combo->setValue("other");
    ExpectEq(combo->getValue(), "other", "setValue keeps a string that is not an item");
    Expect(combo->getSelectionIndex() == -1 && actions == 0, "an unknown value has index -1 and does not fire");
    combo->setValue("Red");
    Expect(combo->getSelectionIndex() == 0, "setValue selects the first match");

    combo->select(1);
    combo->getItems().removeAt(0);
    ExpectEq(combo->getValue(), "Blue", "removing another item keeps the value");
    Expect(combo->getSelectionIndex() == 0, "the index follows the first match after a removal");
    combo->getItems().clear();
    ExpectEq(combo->getValue(), "Blue", "clearing the items keeps the string");
    Expect(combo->getSelectionIndex() == -1 && actions == 0, "a missing item clears the index without firing");
    combo->show();
    Expect(!combo->isShowing(), "show does nothing when the list is empty");
}

double TextBoxHeight(const jadefx::TextField& editor) {
    const jadefx::ComputedStyle& style = editor.computedStyle();
    return editor.getHeight() - style.padding.height() - style.border.height();
}

void TestEditorFillsComboHeight() {
    auto combo = jadefx::make<jadefx::ComboBox>();
    combo->setEditable(true);
    combo->setValue("Lisbon");
    Box box = Place(combo);
    jadefx::TextField* editor = combo->getEditor();
    Expect(editor != nullptr, "editable combo has an editor");
    if (editor == nullptr) {
        return;
    }
    Expect(Near(editor->getY(), 0.0), "the editor starts at the top of the combo");
    Expect(Near(editor->getHeight(), combo->getHeight()), "the editor is as tall as the combo");
    Expect(Near(TextBoxHeight(*editor), combo->getHeight()), "the text area fills the combo by default");
    const jadefx::Font face(editor->computedStyle().fontFamily, editor->computedStyle().fontSize);
    Expect(TextBoxHeight(*editor) + 0.5 >= face.lineHeight(), "the text area fits a line");

    box.scene->setStylesheet(
        "scene { font-size: 15px; }"
        "textfield, combobox { padding: 6px 8px; border-width: 1px; background-color: white; }"
        "textfield:focus { border-width: 2px; }");
    box.scene->layout(420, 360, 0);
    Expect(Near(combo->getHeight(), 32.0), "the combo keeps its preferred height");
    Expect(Near(editor->getY(), 0.0), "a textfield rule does not push the editor down");
    Expect(Near(editor->getHeight(), combo->getHeight()), "a textfield rule does not shrink the editor");
    Expect(Near(TextBoxHeight(*editor), combo->getHeight()), "the text area still fills the combo");
    Expect(editor->computedStyle().background.color.a == 0.f, "the editor stays transparent");
    const jadefx::Font styled(editor->computedStyle().fontFamily, editor->computedStyle().fontSize);
    Expect(TextBoxHeight(*editor) + 0.5 >= styled.lineHeight(), "styled text still fits a line");
}

void TestEditableEnter() {
    auto combo = jadefx::make<jadefx::ComboBox>();
    combo->setPromptText("Type");
    combo->getItems().add("Mint");
    combo->getItems().add("Sage");
    Box box = Place(combo);
    int actions = 0;
    combo->setOnAction([&](jadefx::ActionEvent&) { ++actions; });
    Expect(combo->getEditor() == nullptr, "the editor is absent before setEditable");
    combo->setEditable(true);
    box.scene->layout(420, 360, 0);
    jadefx::TextField* editor = combo->getEditor();
    Expect(editor != nullptr && combo->isEditable(), "getEditor is set while editable");
    if (editor == nullptr) {
        return;
    }
    ExpectEq(editor->getPromptText(), "Type", "the editor uses the combo prompt");
    Expect(editor->getAbsoluteX() + editor->getWidth() <= combo->getAbsoluteX() + combo->getWidth() - 28.0 + 0.5,
           "the editor stops before the arrow");
    editor->requestFocus();
    Expect(editor->isFocused(), "the editor can take focus");
    box.scene->noteKey(jadefx::Key::Space, true, false, 0);
    Expect(!combo->isShowing(), "Space does not open an editable combo");
    box.scene->noteText("Minted");
    Expect(combo->getValue().empty(), "typing does not commit");
    box.scene->noteKey(jadefx::Key::Enter, true, false, 0);
    ExpectEq(combo->getValue(), "Minted", "Enter commits the editor text");
    Expect(combo->getSelectionIndex() == -1, "committed text that is not an item has no index");
    Expect(actions == 1, "Enter fires once");

    actions = 0;
    combo->select(0);
    Expect(actions == 0 && editor->getText() == "Mint", "select copies into the editor without firing");
    editor->requestFocus();
    box.scene->noteKey(jadefx::Key::Down, true, false, 0);
    ExpectEq(combo->getValue(), "Sage", "Down while the editor is focused selects the next item");
    ExpectEq(editor->getText(), "Sage", "Down updates the editor text");
    Expect(actions == 1, "Down fires once");

    const double arrowX = combo->getAbsoluteX() + combo->getWidth() - 14.0;
    const double arrowY = combo->getAbsoluteY() + combo->getHeight() * 0.5;
    Click(*box.scene, arrowX, arrowY);
    Expect(combo->isShowing() && actions == 1, "the arrow opens the popup without firing");
    Click(*box.scene, arrowX, arrowY);
    Expect(!combo->isShowing() && actions == 1, "the arrow closes the popup without firing");

    editor->setText("typed");
    combo->show();
    jadefx::Node* popup = PopupOf(*box.scene, *combo);
    jadefx::Node* row = popup == nullptr ? nullptr : RowById(*box.scene, *popup, "Mint");
    Expect(row != nullptr, "an editable combo shows its rows");
    if (row != nullptr) {
        ClickNode(*box.scene, *row);
    }
    ExpectEq(combo->getValue(), "Mint", "a row click replaces the editor text");
    ExpectEq(editor->getText(), "Mint", "the editor shows the chosen item");
    Expect(combo->getSelectionIndex() == 0 && actions == 2, "a row click fires once");
    Expect(!combo->isShowing(), "a row click hides the editable popup");
}

void TestVisibleRowsScroll() {
    auto combo = jadefx::make<jadefx::ComboBox>();
    combo->setVisibleRowCount(2);
    combo->getItems().add("A");
    combo->getItems().add("B");
    combo->getItems().add("C");
    combo->getItems().add("D");
    combo->getItems().add("E");
    Box box = Place(combo);
    int actions = 0;
    combo->setOnAction([&](jadefx::ActionEvent&) { ++actions; });
    combo->show();
    box.scene->layout(420, 360, 0);
    jadefx::Node* popup = PopupOf(*box.scene, *combo);
    Expect(popup != nullptr, "a short list still opens");
    if (popup == nullptr) {
        return;
    }
    Expect(Near(popup->getHeight(), 56.0, 1.0), "two visible rows make a 56px popup");
    jadefx::Node* rowA = RowById(*box.scene, *popup, "A");
    Expect(rowA != nullptr, "the first row is visible before scrolling");
    if (rowA == nullptr) {
        return;
    }
    const double x = popup->getAbsoluteX() + 8.0;
    const double y = popup->getAbsoluteY() + 8.0;
    box.scene->noteScroll(x, y, 0, -10);
    Expect(Near(rowA->getY(), -10.0), "a pixel delta scrolls by that many pixels");
    combo->hide();
    combo->show();
    box.scene->layout(420, 360, 0);
    popup = PopupOf(*box.scene, *combo);
    if (popup == nullptr) {
        Expect(false, "the popup reopened");
        return;
    }
    box.scene->noteScroll(popup->getAbsoluteX() + 8.0, popup->getAbsoluteY() + 8.0, 0, -1);
    box.scene->layout(420, 360, 0);
    popup = PopupOf(*box.scene, *combo);
    Expect(popup != nullptr, "a notch leaves the popup open");
    if (popup == nullptr) {
        return;
    }
    jadefx::Node* rowB = RowById(*box.scene, *popup, "B");
    Expect(rowB != nullptr && Near(rowB->getY(), 0.0), "a notch of -1 scrolls one row");
    Expect(actions == 0 && combo->getValue().empty(), "scrolling does not select");
    if (rowB == nullptr) {
        return;
    }
    ClickNode(*box.scene, *rowB);
    ExpectEq(combo->getValue(), "B", "a click after scrolling selects that row");
    Expect(combo->getSelectionIndex() == 1 && actions == 1, "the scrolled row fires once");
    Expect(!combo->isShowing(), "the click hides the popup");

    actions = 0;
    combo->show();
    popup = PopupOf(*box.scene, *combo);
    if (popup == nullptr) {
        Expect(false, "the popup opened for the clamp check");
        return;
    }
    box.scene->noteScroll(popup->getAbsoluteX() + 8.0, popup->getAbsoluteY() + 8.0, 0, -10000);
    jadefx::Node* rowD = RowById(*box.scene, *popup, "D");
    Expect(rowD != nullptr, "the scroll offset clamps to the last rows");
    if (rowD != nullptr) {
        ClickNode(*box.scene, *rowD);
    }
    ExpectEq(combo->getValue(), "D", "a click at the clamped scroll selects D");
    Expect(combo->getSelectionIndex() == 3 && actions == 1, "the clamped row is index 3");
}

void TestEscapeAndOutsideCommit() {
    auto combo = jadefx::make<jadefx::ComboBox>();
    combo->getItems().add("One");
    combo->setEditable(true);
    Box box = Place(combo);
    int actions = 0;
    combo->setOnAction([&](jadefx::ActionEvent&) { ++actions; });
    box.scene->layout(420, 360, 0);
    jadefx::TextField* editor = combo->getEditor();
    Expect(editor != nullptr, "editable combo has an editor");
    if (editor == nullptr) {
        return;
    }
    editor->requestFocus();
    box.scene->noteText("qq");
    combo->show();
    Expect(combo->isShowing(), "the list is open before Escape");
    box.scene->noteKey(jadefx::Key::Escape, true, false, 0);
    Expect(!combo->isShowing(), "Escape closes the popup");
    Expect(combo->getValue().empty() && actions == 0, "Escape does not commit until layout");
    box.scene->layout(420, 360, 0);
    ExpectEq(combo->getValue(), "qq", "layout after Escape commits the editor");
    Expect(actions == 1, "the Escape dismiss fires once");

    actions = 0;
    editor->requestFocus();
    combo->setValue("");
    editor->setText("later");
    combo->show();
    Click(*box.scene, box.scene->getWidth() - 4.0, box.scene->getHeight() - 4.0);
    Expect(!combo->isShowing(), "an outside click closes the popup");
    box.scene->layout(420, 360, 0);
    ExpectEq(combo->getValue(), "later", "layout after an outside click commits the editor");
    Expect(actions == 1, "the outside click fires once");
}

void TestDisabledDoesNotOpen() {
    auto combo = jadefx::make<jadefx::ComboBox>();
    combo->getItems().add("One");
    combo->getItems().add("Two");
    combo->setEditable(true);
    Box box = Place(combo);
    int actions = 0;
    combo->setOnAction([&](jadefx::ActionEvent&) { ++actions; });
    box.scene->layout(420, 360, 0);
    jadefx::TextField* editor = combo->getEditor();
    Expect(editor != nullptr, "the editor exists before disable");
    combo->show();
    Expect(combo->isShowing(), "the combo opens while enabled");
    combo->setDisable(true);
    box.scene->layout(420, 360, 0);
    Expect(!combo->isShowing(), "disabling the combo hides the popup");
    Expect(editor != nullptr && editor->isDisable(), "disabling the combo disables the editor");
    ClickNode(*box.scene, *combo);
    Expect(!combo->isShowing(), "a disabled combo does not open");
    combo->requestFocus();
    box.scene->noteKey(jadefx::Key::Down, true, false, 0);
    box.scene->noteKey(jadefx::Key::Space, true, false, 0);
    Expect(combo->getValue().empty() && combo->getSelectionIndex() == -1 && !combo->isShowing(),
           "a disabled combo ignores the keyboard");
    Expect(actions == 0, "a disabled combo does not fire");
}

void TestLeavingTheSceneHides() {
    auto combo = jadefx::make<jadefx::ComboBox>();
    combo->getItems().add("One");
    Box box = Place(combo);
    combo->show();
    Expect(combo->isShowing(), "the popup is open before removal");
    box.root->getChildren().clear();
    Expect(!combo->isShowing(), "leaving the scene hides the popup");
}

}  // namespace

int RunComboBoxTests() {
    TestPreferredSize();
    TestClickSelectsSecondRow();
    TestClickTogglesWithoutAction();
    TestDownMovesSelection();
    TestSelectAndSetValueDoNotFire();
    TestEditorFillsComboHeight();
    TestEditableEnter();
    TestVisibleRowsScroll();
    TestEscapeAndOutsideCommit();
    TestDisabledDoesNotOpen();
    TestLeavingTheSceneHides();
    return gFailures;
}

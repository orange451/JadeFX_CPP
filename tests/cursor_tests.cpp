#include "jadefx/jadefx.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

void Hover(jadefx::Scene& scene, jadefx::Node& node, double width, double height) {
    scene.layout(width, height, 0);
    scene.noteMove(node.getAbsoluteX() + node.getWidth() * 0.5, node.getAbsoluteY() + node.getHeight() * 0.5);
    scene.layout(width, height, 0);
    scene.noteMove(node.getAbsoluteX() + node.getWidth() * 0.5, node.getAbsoluteY() + node.getHeight() * 0.5);
}

void TestControlDefaults() {
    auto button = jadefx::make<jadefx::Button>("Save");
    auto toggle = jadefx::make<jadefx::ToggleButton>("On");
    auto radio = jadefx::make<jadefx::RadioButton>("A");
    auto check = jadefx::make<jadefx::CheckBox>("Yes");
    auto field = jadefx::make<jadefx::TextField>("hello");
    auto area = jadefx::make<jadefx::StyledTextArea>();
    auto code = jadefx::make<jadefx::CodeArea>();
    auto label = jadefx::make<jadefx::Label>("Hi");
    auto box = jadefx::make<jadefx::VBox>();
    box->getChildren().add(button);
    box->getChildren().add(toggle);
    box->getChildren().add(radio);
    box->getChildren().add(check);
    box->getChildren().add(field);
    box->getChildren().add(area);
    box->getChildren().add(code);
    box->getChildren().add(label);
    auto scene = jadefx::make<jadefx::Scene>(box, 320, 900);
    scene->layout(320, 900, 0);

    Expect(button->getCursor() == jadefx::Cursor::Inherit, "a button has no explicit cursor until setCursor");
    Expect(button->computedStyle().cursor == jadefx::Cursor::Pointer, "a button uses the hand");
    Expect(toggle->computedStyle().cursor == jadefx::Cursor::Pointer, "a toggle uses the hand");
    Expect(radio->computedStyle().cursor == jadefx::Cursor::Pointer, "a radio button uses the hand");
    Expect(check->computedStyle().cursor == jadefx::Cursor::Pointer, "a check box uses the hand");
    Expect(field->computedStyle().cursor == jadefx::Cursor::Text, "a text field uses the I-beam");
    Expect(area->computedStyle().cursor == jadefx::Cursor::Text, "a text area uses the I-beam");
    Expect(code->computedStyle().cursor == jadefx::Cursor::Text, "a code area uses the I-beam");
    Expect(label->computedStyle().cursor == jadefx::Cursor::Default, "a label keeps the arrow");
    Expect(jadefx::cursorShape(jadefx::Cursor::Pointer) == jadefx::CursorShape::Hand, "pointer draws the hand");
    Expect(jadefx::cursorShape(jadefx::Cursor::Text) == jadefx::CursorShape::IBeam, "text draws the I-beam");
    Expect(jadefx::cursorShape(jadefx::Cursor::Grab) == jadefx::CursorShape::Hand, "grab draws the hand");
    Expect(jadefx::cursorShape(jadefx::Cursor::Wait) == jadefx::CursorShape::Arrow, "wait draws the arrow");
    Expect(jadefx::cursorShape(jadefx::Cursor::None) == jadefx::CursorShape::Hidden, "none hides the cursor");

    field->setEditable(false);
    scene->layout(320, 420, 0);
    Expect(field->computedStyle().cursor == jadefx::Cursor::Text, "a read-only field still uses the I-beam");

    Hover(*scene, *button, 320, 900);
    Expect(scene->hoverCursor() == jadefx::Cursor::Pointer, "hovering a button reports the hand");
    Hover(*scene, *field, 320, 900);
    Expect(scene->hoverCursor() == jadefx::Cursor::Text, "hovering a text field reports the I-beam");
    Hover(*scene, *label, 320, 900);
    Expect(scene->hoverCursor() == jadefx::Cursor::Default, "hovering a label reports the arrow");

    scene->notePointerExit();
    scene->layout(320, 900, 0);
    Expect(scene->hoverCursor() == jadefx::Cursor::Default, "leaving the window restores the arrow");
    Expect(!button->isHovered(), "leaving the window clears hover");
}

void TestStylesOverride() {
    auto box = jadefx::make<jadefx::VBox>();
    box->setStyle("cursor: move;");
    auto button = jadefx::make<jadefx::Button>("Go");
    auto label = jadefx::make<jadefx::Label>("Hi");
    auto custom = jadefx::make<jadefx::StackPane>();
    auto caption = jadefx::make<jadefx::Label>("Get started");
    custom->getChildren().add(caption);
    custom->setStyle("cursor: pointer;");
    box->getChildren().add(label);
    box->getChildren().add(button);
    box->getChildren().add(custom);
    auto scene = jadefx::make<jadefx::Scene>(box, 240, 200);
    scene->layout(240, 200, 0);

    Expect(label->computedStyle().cursor == jadefx::Cursor::Move, "cursor inherits onto a label");
    Expect(button->computedStyle().cursor == jadefx::Cursor::Pointer, "a button keeps its hand inside a move container");
    Expect(caption->computedStyle().cursor == jadefx::Cursor::Pointer, "a label inside a pointer pane inherits the hand");
    Hover(*scene, *caption, 240, 200);
    Expect(scene->hoverCursor() == jadefx::Cursor::Pointer, "hovering the caption reports the hand");

    button->setCursor(jadefx::Cursor::Crosshair);
    Expect(button->getCursor() == jadefx::Cursor::Crosshair, "setCursor stores the requested cursor");
    scene->layout(240, 200, 0);
    Expect(button->computedStyle().cursor == jadefx::Cursor::Crosshair, "setCursor replaces the button hand");

    scene->setStylesheet("button { cursor: text; } button:hover { cursor: help; }");
    scene->layout(240, 200, 0);
    Expect(button->computedStyle().cursor == jadefx::Cursor::Text, "a stylesheet replaces setCursor");
    Hover(*scene, *button, 240, 200);
    Expect(button->computedStyle().cursor == jadefx::Cursor::Help, ":hover can change the cursor");
    Expect(scene->hoverCursor() == jadefx::Cursor::Help, "the pointer reports the hover cursor");
    Expect(jadefx::cursorShape(scene->hoverCursor()) == jadefx::CursorShape::Arrow, "help uses the arrow shape");

    button->setStyle("cursor: hand;");
    scene->layout(240, 200, 0);
    Hover(*scene, *button, 240, 200);
    Expect(button->computedStyle().cursor == jadefx::Cursor::Pointer, "inline cursor: hand wins");

    button->setStyle("cursor: url(missing), crosshair;");
    scene->layout(240, 200, 0);
    Expect(button->computedStyle().cursor == jadefx::Cursor::Crosshair, "a list uses the first known keyword");

    scene->noteMove(-10, -10);
    button->setStyle("cursor: not-a-cursor;");
    scene->layout(240, 200, 0);
    Expect(button->computedStyle().cursor == jadefx::Cursor::Text, "an unknown cursor keyword is ignored");

    button->setStyle("cursor: inherit;");
    scene->layout(240, 200, 0);
    Expect(button->computedStyle().cursor == jadefx::Cursor::Move, "cursor: inherit uses the parent");

    button->setStyle("cursor: auto;");
    scene->layout(240, 200, 0);
    Expect(button->computedStyle().cursor == jadefx::Cursor::Pointer, "cursor: auto restores the button hand");

    label->setStyle("cursor: auto;");
    scene->layout(240, 200, 0);
    Expect(label->computedStyle().cursor == jadefx::Cursor::Default, "cursor: auto on a label is the arrow");

    button->setStyle("cursor: ew-resize;");
    scene->layout(240, 200, 0);
    Expect(button->computedStyle().cursor == jadefx::Cursor::EwResize, "ew-resize is its own cursor");
    Expect(jadefx::cursorShape(button->computedStyle().cursor) == jadefx::CursorShape::SizeWestEast,
           "ew-resize uses the horizontal resize shape");
    button->setStyle("cursor: col-resize;");
    scene->layout(240, 200, 0);
    Expect(button->computedStyle().cursor == jadefx::Cursor::EwResize, "col-resize shares the horizontal resize cursor");
    button->setStyle("cursor: not-allowed;");
    scene->layout(240, 200, 0);
    Expect(button->computedStyle().cursor == jadefx::Cursor::NotAllowed, "not-allowed is its own cursor");
    button->setStyle("cursor: none;");
    scene->layout(240, 200, 0);
    Expect(button->computedStyle().cursor == jadefx::Cursor::None, "none hides the cursor");
}

void TestDisabled() {
    auto button = jadefx::make<jadefx::Button>("Save");
    auto scene = jadefx::make<jadefx::Scene>(button, 200, 80);
    scene->layout(200, 80, 0);
    button->setDisable(true);
    scene->layout(200, 80, 0);
    Hover(*scene, *button, 200, 80);
    Expect(button->computedStyle().cursor == jadefx::Cursor::Default, "a disabled button drops the hand");
    Expect(scene->hoverCursor() == jadefx::Cursor::Default, "hovering a disabled button reports the arrow");

    scene->setStylesheet("button:disabled { cursor: not-allowed; }");
    scene->layout(200, 80, 0);
    Hover(*scene, *button, 200, 80);
    Expect(scene->hoverCursor() == jadefx::Cursor::NotAllowed, ":disabled can set not-allowed");
}

void TestTextAreaScrollbar() {
    auto area = jadefx::make<jadefx::StyledTextArea>();
    std::string text;
    for (int i = 0; i < 40; ++i) {
        text += "line\n";
    }
    area->setText(text);
    area->setPrefSize(200, 80);
    auto scene = jadefx::make<jadefx::Scene>(area, 200, 80);
    scene->layout(200, 80, 0);
    scene->noteMove(area->getAbsoluteX() + 12, area->getAbsoluteY() + 12);
    scene->layout(200, 80, 0);
    Expect(scene->hoverCursor() == jadefx::Cursor::Text, "the text in an area uses the I-beam");
    scene->noteMove(area->getAbsoluteX() + area->getWidth() - 3, area->getAbsoluteY() + 20);
    scene->layout(200, 80, 0);
    Expect(scene->hoverCursor() == jadefx::Cursor::Default, "the text area scrollbar keeps the arrow");
}

void TestComboMenuTabAndTree() {
    auto combo = jadefx::make<jadefx::ComboBox>();
    combo->getItems().add("Apple");
    combo->getItems().add("Berry");
    combo->setEditable(true);
    combo->setPrefSize(180, 32);

    auto save = jadefx::make<jadefx::MenuItem>("Save");
    auto menuButton = jadefx::make<jadefx::MenuButton>("File");
    menuButton->getItems().add(save);

    auto tab = jadefx::make<jadefx::Tab>("Alpha", jadefx::make<jadefx::Label>("Page"));
    auto tabs = jadefx::make<jadefx::TabPane>();
    tabs->getTabs().add(tab);
    tabs->setPrefSize(240, 120);

    auto root = jadefx::make<jadefx::TreeItem>("Root");
    root->setExpanded(true);
    for (int i = 0; i < 8; ++i) {
        root->getChildren().add(jadefx::make<jadefx::TreeItem>("Row"));
    }
    auto tree = jadefx::make<jadefx::TreeView>(root);
    tree->setPrefSize(180, 70);

    auto box = jadefx::make<jadefx::VBox>();
    box->getChildren().add(combo);
    box->getChildren().add(menuButton);
    box->getChildren().add(tabs);
    box->getChildren().add(tree);
    auto scene = jadefx::make<jadefx::Scene>(box, 360, 420);
    scene->layout(360, 420, 0);

    Expect(combo->computedStyle().cursor == jadefx::Cursor::Pointer, "a combo box uses the hand");
    Expect(menuButton->computedStyle().cursor == jadefx::Cursor::Pointer, "a menu button uses the hand");
    scene->noteMove(combo->getAbsoluteX() + 16, combo->getAbsoluteY() + combo->getHeight() * 0.5);
    scene->layout(360, 420, 0);
    Expect(scene->hoverCursor() == jadefx::Cursor::Text, "an editable combo's text uses the I-beam");
    scene->noteMove(combo->getAbsoluteX() + combo->getWidth() - 8, combo->getAbsoluteY() + combo->getHeight() * 0.5);
    scene->layout(360, 420, 0);
    Expect(scene->hoverCursor() == jadefx::Cursor::Pointer, "a combo arrow uses the hand");

    scene->noteButton(0, true, menuButton->getAbsoluteX() + 4, menuButton->getAbsoluteY() + 4);
    scene->noteButton(0, false, menuButton->getAbsoluteX() + 4, menuButton->getAbsoluteY() + 4);
    scene->layout(360, 420, 0);
    jadefx::Node* row = scene->getElementById("Save");
    Expect(row != nullptr && row->computedStyle().cursor == jadefx::Cursor::Pointer, "a menu item uses the hand");

    scene->noteButton(0, true, combo->getAbsoluteX() + combo->getWidth() - 8, combo->getAbsoluteY() + 8);
    scene->noteButton(0, false, combo->getAbsoluteX() + combo->getWidth() - 8, combo->getAbsoluteY() + 8);
    scene->layout(360, 420, 0);
    jadefx::Node* berry = scene->getElementById("Berry");
    Expect(berry != nullptr && berry->computedStyle().cursor == jadefx::Cursor::Pointer, "a combo row uses the hand");

    const std::vector<jadefx::Node*> headers = scene->getElementsByClassName("tab");
    Expect(!headers.empty() && headers[0]->computedStyle().cursor == jadefx::Cursor::Pointer, "a tab uses the hand");
    const std::vector<jadefx::Node*> closes = scene->getElementsByClassName("tab-close-button");
    Expect(!closes.empty() && closes[0]->computedStyle().cursor == jadefx::Cursor::Pointer, "a tab close button uses the hand");
    const std::vector<jadefx::Node*> cells = scene->getElementsByClassName("tree-cell");
    Expect(!cells.empty() && cells[0]->computedStyle().cursor == jadefx::Cursor::Pointer, "a tree row uses the hand");
    const std::vector<jadefx::Node*> arrows = scene->getElementsByClassName("tree-disclosure-node");
    Expect(!arrows.empty() && arrows[0]->computedStyle().cursor == jadefx::Cursor::Pointer, "a disclosure arrow uses the hand");
    bool sawBar = false;
    for (jadefx::Node* bar : scene->getElementsByClassName("scroll-bar")) {
        sawBar = true;
        Expect(bar->computedStyle().cursor == jadefx::Cursor::Default, "a tree scrollbar keeps the arrow");
    }
    Expect(sawBar, "the tree shows a scrollbar");
}

}  // namespace

int RunCursorTests() {
    TestControlDefaults();
    TestStylesOverride();
    TestDisabled();
    TestTextAreaScrollbar();
    TestComboMenuTabAndTree();
    return gFailures;
}

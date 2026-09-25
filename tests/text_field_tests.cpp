#include "jadefx/jadefx.hpp"
#include "jadefx/scene/controls/TextField.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

bool Near(double a, double b, double epsilon = 0.05) { return std::fabs(a - b) <= epsilon; }

void Press(jadefx::Scene& scene, int key, int mods = 0, bool repeat = false) {
    scene.noteKey(key, true, repeat, mods);
    scene.noteKey(key, false, false, mods);
}

struct Fixture {
    std::shared_ptr<jadefx::TextField> field;
    std::shared_ptr<jadefx::Scene> scene;
};

Fixture Open(const std::string& text = {}) {
    Fixture fixture;
    fixture.field = jadefx::make<jadefx::TextField>(text);
    fixture.scene = jadefx::make<jadefx::Scene>(fixture.field, 480, 160);
    fixture.scene->layout(480, 160, 0);
    fixture.field->requestFocus();
    return fixture;
}

void TestDefaultsAndTyping() {
    jadefx::TextField plain("hello");
    Expect(std::string(plain.getElementType()) == "textfield", "the element type is textfield");
    Expect(plain.isEditable(), "a text field starts editable");
    Expect(plain.getPrefColumnCount() == 12, "a text field prefers 12 columns");
    Expect(plain.getAlignment() == jadefx::Pos::CenterLeft, "a text field starts center-left");
    Expect(plain.getPadding().top == 6 && plain.getPadding().bottom == 6, "vertical padding is 6");
    Expect(plain.getPadding().left == 8 && plain.getPadding().right == 8, "horizontal padding is 8");
    Expect(plain.getText() == "hello", "the constructor stores its text");
    Expect(plain.getCaretPosition() == 0 && plain.getAnchor() == 0, "setText clamps a new field's caret to the start");
    Expect(plain.getLength() == 5, "length counts code points");

    plain.setText("a\r\nb");
    Expect(plain.getText() == "ab", "setText keeps the field on one line");

    Fixture open = Open();
    Expect(open.scene->noteText("h"), "typed text is consumed");
    Expect(open.scene->noteText("i"), "a second character is consumed");
    Expect(open.field->getText() == "hi", "noteText inserts characters");
    Expect(open.field->getCaretPosition() == 2 && open.field->getAnchor() == 2, "the caret follows the insert");
    Press(*open.scene, jadefx::Key::Backspace);
    Expect(open.field->getText() == "h" && open.field->getCaretPosition() == 1, "backspace removes one code point");
    open.scene->noteKey(jadefx::Key::Backspace, true, true, 0);
    Expect(open.field->getText().empty() && open.field->getCaretPosition() == 0, "a repeated backspace removes another character");

    open.field->setText("abcd");
    open.field->positionCaret(1);
    Press(*open.scene, jadefx::Key::Delete);
    Expect(open.field->getText() == "acd" && open.field->getCaretPosition() == 1, "delete removes the next code point");
    open.field->selectRange(1, 3);
    Expect(open.field->getSelectedText() == "cd", "a forward range returns its text");
    open.scene->noteText("Z");
    Expect(open.field->getText() == "aZ" && open.field->getCaretPosition() == 2, "typing replaces the selection");

    open.field->setText("abcdef");
    open.field->positionCaret(5);
    open.field->setText("xy");
    Expect(open.field->getCaretPosition() == 2 && open.field->getAnchor() == 2, "setText clamps the caret and collapses the selection");
    open.field->setText("abcdef");
    Expect(open.field->getCaretPosition() == 2, "setText does not jump the caret to the end");

    open.field->setText("abcd");
    open.field->selectAll();
    open.field->replaceSelection("x\r\ny");
    Expect(open.field->getText() == "xy" && open.field->getCaretPosition() == 2, "replaceSelection strips line breaks");
    open.field->clear();
    Expect(open.field->getText().empty() && open.field->getCaretPosition() == 0, "clear empties the field");

    open.field->setText("");
    Expect(open.scene->noteText("\u00e9"), "a non-ascii character is consumed");
    Expect(open.scene->noteText("a"), "an ascii character follows it");
    Expect(open.field->getLength() == 2 && open.field->getText() == "\u00e9a", "length is code points, not bytes");
    Press(*open.scene, jadefx::Key::Backspace);
    Expect(open.field->getText() == "\u00e9" && open.field->getLength() == 1, "backspace removes one code point, not one byte");
}

void TestActionAndClipboard() {
    Fixture open = Open("stay");
    int fires = 0;
    open.field->setOnAction([&](jadefx::ActionEvent& event) {
        ++fires;
        Expect(event.source == open.field.get(), "the action source is the text field");
    });
    const std::string before = open.field->getText();
    Expect(open.scene->noteKey(jadefx::Key::Enter, true, false, 0), "Enter is consumed");
    Expect(!open.scene->noteKey(jadefx::Key::Enter, false, false, 0), "key-up is ignored");
    Expect(fires == 1, "Enter fires the action once");
    Expect(open.field->getText() == before, "Enter does not change the text");
    open.scene->noteKey(jadefx::Key::Enter, true, true, 0);
    Expect(fires == 1, "a repeated Enter does not fire again");
    Press(*open.scene, jadefx::Key::KpEnter);
    Expect(fires == 2 && open.field->getText() == before, "KpEnter fires and does not insert a newline");
    open.field->fire();
    Expect(fires == 3, "fire() runs the action");

    Expect(!open.scene->noteKey(jadefx::Key::Up, true, false, 0), "Up is not consumed");
    Expect(!open.scene->noteKey(jadefx::Key::Down, true, false, 0), "Down is not consumed");
    Expect(!open.scene->noteKey(jadefx::Key::Tab, true, false, 0), "Tab is not consumed");
    Expect(!open.scene->noteKey(jadefx::Key::Escape, true, false, 0), "Escape is not consumed");

    open.field->setText("hello");
    open.field->selectAll();
    Expect(open.field->getAnchor() == 0 && open.field->getCaretPosition() == 5, "selectAll anchors at the start");
    Expect(open.field->getSelectedText() == "hello", "selectAll selects the whole field");
    Press(*open.scene, jadefx::Key::C, jadefx::Key::ModControl);
    Expect(open.scene->clipboardText() == "hello", "copy writes the scene clipboard");
    open.field->deselect();
    Expect(open.field->getSelectedText().empty() && open.field->getCaretPosition() == 5, "deselect keeps the caret");
    Press(*open.scene, jadefx::Key::C, jadefx::Key::ModControl);
    Expect(open.scene->clipboardText() == "hello", "copying an empty selection leaves the clipboard");
    open.field->positionCaret(open.field->getLength());
    Press(*open.scene, jadefx::Key::V, jadefx::Key::ModSuper);
    Expect(open.field->getText() == "hellohello", "paste reads the scene clipboard");

    open.field->selectRange(0, 5);
    Press(*open.scene, jadefx::Key::X, jadefx::Key::ModControl);
    Expect(open.field->getText() == "hello" && open.scene->clipboardText() == "hello", "cut removes the selection");
    open.scene->setClipboardText("a\nb");
    open.field->selectAll();
    open.field->paste();
    Expect(open.field->getText() == "ab", "paste strips line breaks");

    open.field->setText("a\u00e9b");
    open.field->selectRange(2, 1);
    Expect(open.field->getSelectedText() == "\u00e9", "a reversed range still returns the selected text");
    open.field->copy();
    Expect(open.scene->clipboardText() == "\u00e9", "copy keeps a whole code point");

    jadefx::TextField loose("cat");
    loose.selectAll();
    loose.copy();
    loose.positionCaret(3);
    loose.paste();
    Expect(loose.getText() == "catcat", "without a scene, paste uses the local clipboard");
    loose.selectRange(0, 3);
    loose.cut();
    Expect(loose.getText() == "cat", "without a scene, cut uses the local clipboard");
}

void TestNavigation() {
    Fixture open = Open("abcd");
    open.field->positionCaret(2);
    Expect(open.scene->noteKey(jadefx::Key::Left, true, false, 0), "Left is consumed");
    Expect(open.field->getCaretPosition() == 1 && open.field->getAnchor() == 1, "Left moves one code point");
    open.scene->noteKey(jadefx::Key::Left, true, true, 0);
    Expect(open.field->getCaretPosition() == 0, "a repeated Left moves again");
    Press(*open.scene, jadefx::Key::Home);
    Expect(open.field->getCaretPosition() == 0, "Home moves to the start");
    Press(*open.scene, jadefx::Key::Right);
    Expect(open.field->getCaretPosition() == 1, "Right moves one code point");
    Press(*open.scene, jadefx::Key::End);
    Expect(open.field->getCaretPosition() == 4 && open.field->getAnchor() == 4, "End moves to the end and collapses");

    open.field->positionCaret(1);
    Press(*open.scene, jadefx::Key::Right, jadefx::Key::ModShift);
    Expect(open.field->getAnchor() == 1 && open.field->getCaretPosition() == 2, "Shift+Right extends the selection");
    Expect(open.field->getSelectedText() == "b", "the extended range is the selected text");
    Press(*open.scene, jadefx::Key::Home, jadefx::Key::ModShift);
    Expect(open.field->getAnchor() == 1 && open.field->getCaretPosition() == 0, "Shift+Home keeps the anchor");
    Expect(open.field->getSelectedText() == "a", "Shift+Home selects back to the start");

    open.field->setText("hello  world");
    open.field->positionCaret(0);
    Press(*open.scene, jadefx::Key::Right, jadefx::Key::ModAlt);
    Expect(open.field->getCaretPosition() == 7, "Alt+Right skips the spaces between words");
    Press(*open.scene, jadefx::Key::Right, jadefx::Key::ModControl);
    Expect(open.field->getCaretPosition() == open.field->getLength(), "Ctrl+Right reaches the end of the last word");
    Press(*open.scene, jadefx::Key::Left, jadefx::Key::ModSuper);
    Expect(open.field->getCaretPosition() == 7, "Super+Left returns to the start of the word");
    Press(*open.scene, jadefx::Key::Left, jadefx::Key::ModAlt | jadefx::Key::ModShift);
    Expect(open.field->getCaretPosition() == 0 && open.field->getAnchor() == 7, "Shift+Alt+Left extends by a word");

    open.field->setText("ab, cd");
    open.field->positionCaret(0);
    Press(*open.scene, jadefx::Key::Right, jadefx::Key::ModControl);
    Expect(open.field->getCaretPosition() == 2, "a word stops before punctuation");
    Press(*open.scene, jadefx::Key::Right, jadefx::Key::ModControl);
    Expect(open.field->getCaretPosition() == 4, "punctuation is its own word and the following space is skipped");

    open.field->setEditable(false);
    open.field->setText("abcd");
    open.field->positionCaret(2);
    Expect(!open.scene->noteText("z"), "a read-only field does not consume text");
    Expect(open.field->getText() == "abcd", "a read-only field does not insert");
    Press(*open.scene, jadefx::Key::Backspace);
    Press(*open.scene, jadefx::Key::Delete);
    Expect(open.field->getText() == "abcd", "a read-only field does not delete");
    Press(*open.scene, jadefx::Key::Home);
    Expect(open.field->getCaretPosition() == 0, "a read-only field still moves the caret");
    open.field->selectAll();
    Press(*open.scene, jadefx::Key::C, jadefx::Key::ModControl);
    Expect(open.scene->clipboardText() == "abcd", "a read-only field can still copy");
    open.scene->setClipboardText("nope");
    Press(*open.scene, jadefx::Key::V, jadefx::Key::ModControl);
    Press(*open.scene, jadefx::Key::X, jadefx::Key::ModSuper);
    Expect(open.field->getText() == "abcd", "a read-only field does not paste or cut");
    Expect(open.scene->clipboardText() == "nope", "a refused cut does not replace the clipboard");
    Press(*open.scene, jadefx::Key::A, jadefx::Key::ModControl);
    Expect(open.field->getSelectedText() == "abcd", "a read-only field can still select all");
}

void TestPreferredColumns() {
    auto field = jadefx::make<jadefx::TextField>();
    auto scene = jadefx::make<jadefx::Scene>(field, 800, 200);
    scene->layout(800, 200, 0);
    const jadefx::Font font(field->computedStyle().fontFamily, field->computedStyle().fontSize);
    const double em = font.measureWidth("n");
    const double padX = field->computedStyle().padding.left + field->computedStyle().padding.right;
    const double padY = field->computedStyle().padding.top + field->computedStyle().padding.bottom;
    Expect(em > 1.0, "the sample glyph has a width");
    Expect(Near(field->measuredWidth(2000), 12.0 * em + padX), "preferred width is twelve n's plus padding");
    const double wide = field->getWidth();
    field->setPrefColumnCount(4);
    Expect(field->getPrefColumnCount() == 4, "the column count is stored");
    Expect(Near(field->measuredWidth(2000), 4.0 * em + padX), "fewer columns make a narrower field");
    Expect(field->measuredWidth(2000) < wide, "the measured width shrinks with the column count");
    scene->layout(800, 200, 0);
    Expect(field->getWidth() < wide, "layout uses the column count");
    Expect(Near(field->measuredHeight(400, 2000), static_cast<double>(font.lineHeight()) + padY),
           "preferred height is one line plus padding");
    field->setPrefWidth(90);
    scene->layout(800, 200, 0);
    Expect(Near(field->measuredWidth(2000), 90), "an explicit pref width replaces the column width");
    Expect(Near(field->getWidth(), 90), "layout honors an explicit pref width");
}

void TestDisabledPromptAndPointer() {
    Fixture open = Open();
    open.field->setPromptText("Search");
    Expect(open.field->getPromptText() == "Search", "prompt text is stored");
    const jadefx::Color background = open.field->computedStyle().background.color;
    Expect(background.r > 0.9f && background.g > 0.9f && background.b > 0.9f && background.a > 0.9f,
           "the field background is white");

    open.field->setText("stay");
    open.field->positionCaret(2);
    open.field->setDisable(true);
    open.scene->layout(480, 160, 0);
    Expect(!open.field->isFocused(), "disabling the field releases focus");
    open.scene->noteText("z");
    Press(*open.scene, jadefx::Key::Backspace);
    Press(*open.scene, jadefx::Key::Left);
    const double x = open.field->getAbsoluteX() + open.field->getWidth() * 0.5;
    const double y = open.field->getAbsoluteY() + open.field->getHeight() * 0.5;
    open.scene->noteButton(0, true, x, y);
    open.scene->noteButton(0, false, x, y);
    Expect(open.field->getText() == "stay", "a disabled field does not insert");
    Expect(open.field->getCaretPosition() == 2, "a disabled field ignores keys and clicks");
    Expect(!open.field->isFocused(), "a disabled field does not take focus");

    open.field->setDisable(false);
    open.scene->layout(480, 160, 0.25);
    open.field->requestFocus();
    open.field->setText("n");
    open.field->positionCaret(0);
    const jadefx::Font font(open.field->computedStyle().fontFamily, open.field->computedStyle().fontSize);
    const double glyph = font.measureWidth("n");
    const double contentLeft = open.field->getAbsoluteX() + open.field->computedStyle().padding.left +
                               open.field->computedStyle().border.left;
    const double contentRight = open.field->getAbsoluteX() + open.field->getWidth() - open.field->computedStyle().padding.right -
                                open.field->computedStyle().border.right;
    const double midY = open.field->getAbsoluteY() + open.field->getHeight() * 0.5;
    open.scene->noteButton(0, true, contentLeft + glyph * 0.25, midY);
    open.scene->noteButton(0, false, contentLeft + glyph * 0.25, midY);
    Expect(open.field->isFocused(), "a click focuses the field");
    Expect(open.field->getCaretPosition() == 0, "a click in the first quarter of a glyph stays before it");
    open.scene->noteButton(0, true, contentLeft + glyph * 0.75, midY);
    open.scene->noteButton(0, false, contentLeft + glyph * 0.75, midY);
    Expect(open.field->getCaretPosition() == 1, "a click in the last quarter of a glyph moves past it");

    open.field->setText("nn");
    open.scene->noteButton(0, true, contentLeft + 1, midY);
    open.scene->noteMove(contentRight - 1, midY);
    open.scene->noteButton(0, false, contentRight - 1, midY);
    Expect(open.field->getAnchor() == 0 && open.field->getCaretPosition() == 2, "a drag extends the caret and keeps the anchor");
    Expect(open.field->getSelectedText() == "nn", "the drag selects the characters it crossed");

    open.field->setText("n");
    open.field->setAlignment(jadefx::Pos::CenterRight);
    open.scene->noteButton(0, true, contentLeft + 1, midY);
    open.scene->noteButton(0, false, contentLeft + 1, midY);
    Expect(open.field->getCaretPosition() == 0, "a click left of right-aligned text stays at the start");
    const double textStart = contentRight - glyph;
    open.scene->noteButton(0, true, textStart + glyph * 0.75, midY);
    open.scene->noteButton(0, false, textStart + glyph * 0.75, midY);
    Expect(open.field->getCaretPosition() == 1, "a click on right-aligned text uses that origin");

    open.field->setAlignment(jadefx::Pos::Center);
    const double contentWidth = contentRight - contentLeft;
    const double centered = contentLeft + (contentWidth - glyph) * 0.5;
    open.scene->noteButton(0, true, centered + glyph * 0.25, midY);
    open.scene->noteButton(0, false, centered + glyph * 0.25, midY);
    Expect(open.field->getCaretPosition() == 0, "a click on the left of centered text stays before it");
    open.scene->noteButton(0, true, centered + glyph * 0.75, midY);
    open.scene->noteButton(0, false, centered + glyph * 0.75, midY);
    Expect(open.field->getCaretPosition() == 1, "a click on the right of centered text moves past it");
}

void TestCaretBounds() {
    auto open = Open("ab");
    open.field->positionCaret(0);
    double startX = 0;
    double startY = 0;
    double startHeight = 0;
    Expect(open.field->caretBounds(startX, startY, startHeight), "a laid-out field reports its caret");
    Expect(startHeight > 1, "the caret has a line height");
    open.field->positionCaret(2);
    double endX = 0;
    double endY = 0;
    double endHeight = 0;
    Expect(open.field->caretBounds(endX, endY, endHeight), "the caret at the end is reported");
    Expect(endX > startX + 1, "the caret moves right with the text");

    auto field = jadefx::make<jadefx::TextField>();
    field->setPrefColumnCount(2);
    auto scene = jadefx::make<jadefx::Scene>(field, 480, 160);
    scene->layout(480, 160, 0);
    field->setText("abcdefghij");
    field->positionCaret(field->getLength());
    double scrolledX = 0;
    double scrolledY = 0;
    double scrolledHeight = 0;
    Expect(field->caretBounds(scrolledX, scrolledY, scrolledHeight), "a scrolled field reports its caret");
    const double right = field->getAbsoluteX() + field->getWidth();
    Expect(scrolledX > field->getAbsoluteX() && scrolledX <= right + 1, "a scrolled caret stays inside the field");
}

void TestHorizontalScroll() {
    auto field = jadefx::make<jadefx::TextField>();
    field->setPrefColumnCount(2);
    auto scene = jadefx::make<jadefx::Scene>(field, 480, 160);
    scene->layout(480, 160, 0);
    field->requestFocus();
    field->setText("abcdefghij");
    field->positionCaret(field->getLength());
    const jadefx::Font font(field->computedStyle().fontFamily, field->computedStyle().fontSize);
    const double contentLeft = field->getAbsoluteX() + field->computedStyle().padding.left + field->computedStyle().border.left;
    const double contentRight = field->getAbsoluteX() + field->getWidth() - field->computedStyle().padding.right -
                                field->computedStyle().border.right;
    const double contentWidth = contentRight - contentLeft;
    const double midY = field->getAbsoluteY() + field->getHeight() * 0.5;
    Expect(font.measureWidth(field->getText()) > contentWidth + 1.0, "the sample is wider than the field");
    scene->noteButton(0, true, contentRight - 1, midY);
    scene->noteButton(0, false, contentRight - 1, midY);
    Expect(field->getCaretPosition() == field->getLength(), "scroll keeps the caret at the end under the right edge");
    scene->noteButton(0, true, contentLeft + 1, midY);
    scene->noteButton(0, false, contentLeft + 1, midY);
    Expect(field->getCaretPosition() > 0 && field->getCaretPosition() < field->getLength(),
           "the left edge shows a later character once the line has scrolled");
}

}  // namespace

int RunTextFieldTests() {
    TestDefaultsAndTyping();
    TestActionAndClipboard();
    TestNavigation();
    TestPreferredColumns();
    TestDisabledPromptAndPointer();
    TestCaretBounds();
    TestHorizontalScroll();
    return gFailures;
}

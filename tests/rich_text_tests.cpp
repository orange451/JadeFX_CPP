#include "jadefx/jadefx.hpp"

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

void Key(jadefx::Scene& scene, int key, int mods = 0) {
    scene.noteKey(key, true, false, mods);
    scene.noteKey(key, false, false, mods);
}

std::shared_ptr<jadefx::StyledTextArea> Editor(const std::string& text, double width, double height) {
    auto area = jadefx::make<jadefx::StyledTextArea>();
    area->setPrefSize(width, height);
    area->setText(text);
    area->forgetHistory();
    return area;
}

void TestDocument() {
    jadefx::EditableStyledDocument document("some\nthing");
    Expect(document.length() == 10, "newline counts as one code point");
    Expect(document.paragraphCount() == 2, "two paragraphs");
    Expect(document.offset(0, 4) == 4, "end of the first paragraph");
    Expect(document.offset(0, 5) == 5, "column past the paragraph is the next one");
    Expect(document.offset(1, -1) == 4, "a negative column walks into the previous paragraph");
    Expect(document.offset(1, 0) == 5, "start of the second paragraph");
    Expect(document.text(4, 5) == "\n", "the gap between paragraphs is a newline");
    Expect(document.subDocument(2, 7).text() == "me\nth", "a range can cross a paragraph");

    document.replaceText(4, 4, "X", {}, {});
    Expect(document.text() == "someX\nthing", "insert at the end of a paragraph stays on that paragraph");
    document.replaceText(5, 6, "", {}, {});
    Expect(document.text() == "someXthing", "deleting the newline joins paragraphs");

    jadefx::EditableStyledDocument unicode("a\xF0\x9F\x98\x80" "b");
    Expect(unicode.length() == 3, "a code point is one caret position");
    unicode.replaceText(1, 2, "e\u0301", {}, {});
    Expect(unicode.text() == "ae\u0301" "b", "replacing one code point keeps the neighbors");
}

void TestStylesAndUndo(jadefx::Scene& scene, jadefx::StyledTextArea& area) {
    area.setText("hello");
    area.forgetHistory();
    jadefx::TextStyle bold;
    bold.bold = true;
    area.setStyle(0, 5, bold);
    Expect(area.getStyleSpans(0, 5).spans().size() == 1 && area.getStyleSpans(0, 5).spans().front().style.bold,
           "a style covers the range");
    area.undo();
    Expect(!area.getText().empty() && !area.getStyleSpans(0, area.length()).spans().empty(), "undo restores the text");
    Expect(!area.getStyleSpans(0, 5).spans().front().style.bold, "undo removes the bold");

    area.setText("");
    area.forgetHistory();
    area.requestFocus();
    scene.noteText("h");
    scene.noteText("i");
    Expect(area.getText() == "hi", "typed characters are inserted");
    Expect(area.canUndo(), "typing can be undone");
    area.undo();
    Expect(area.getText().empty(), "consecutive typing is one undo");

    area.setText("hello world");
    area.moveTo(area.length());
    Key(scene, jadefx::Key::Left, jadefx::Key::ModControl);
    Expect(area.caretPosition() == 6, "ctrl+left stops at the start of the word");
    Key(scene, jadefx::Key::Right, jadefx::Key::ModControl);
    Expect(area.caretPosition() == area.length(), "ctrl+right stops at the end of the word");

    area.setEditable(false);
    const std::string frozen = area.getText();
    scene.noteText("z");
    Expect(area.getText() == frozen, "a read-only area ignores typing");
    area.setEditable(true);
}

void TestWrapFoldAndClipboard(jadefx::Scene& scene, jadefx::StyledTextArea& area) {
    area.setWrapText(true);
    area.setText("abcdefghijklmnopqrstuvwxyz");
    area.setPrefSize(80, 160);
    scene.layout(80, 160, 0);
    Expect(area.paragraphCount() == 1, "wrapping does not split the paragraph");
    Expect(area.visualLineCount() > 1, "a narrow area wraps the paragraph");
    area.moveTo(0);
    Key(scene, jadefx::Key::Down);
    Expect(area.currentParagraph() == 0 && area.caretPosition() > 0, "down moves to the next visual line");

    area.setWrapText(false);
    area.setText("one\ntwo\nthree");
    area.foldParagraphs(0, 2);
    Expect(area.isFolded(1) && area.isFolded(2) && !area.isFolded(0), "a fold hides the paragraphs under the header");
    Expect(area.getText() == "one\ntwo\nthree", "folding keeps the text");
    Expect(area.visualLineCount() == 1, "a fold draws as one line");
    area.moveTo(3);
    Key(scene, jadefx::Key::Right);
    Expect(area.caretPosition() == 3, "the caret does not walk into a fold");
    area.unfoldParagraphs(1);
    Expect(!area.isFolded(1), "unfold shows the paragraph again");

    area.setText("alpha");
    area.selectAll();
    area.copy();
    area.moveTo(area.length());
    area.paste();
    Expect(area.getText() == "alphaalpha", "copy and paste use the scene clipboard");

    jadefx::TextStyle color;
    color.hasFill = true;
    color.fill = jadefx::Color::parse("#ff0000");
    area.setText("red");
    area.setStyle(0, 3, color);
    area.selectAll();
    area.copy();
    area.moveTo(area.length());
    area.paste();
    Expect(area.getText() == "redred", "styled paste keeps the characters");
    Expect(area.getStyleSpans(3, 6).spans().front().style.hasFill, "styled paste keeps the color");

    area.setText("ab");
    area.moveTo(0);
    area.addCaret(2);
    scene.noteText("Q");
    Expect(area.getText() == "QabQ", "each caret receives the typed character");
}

jadefx::TextStyle StyleAt(const jadefx::StyledTextArea& area, int start, int end) {
    return area.getStyleSpans(start, end).spans().front().style;
}

void TestCharacterStyleShortcuts() {
    auto notes = jadefx::make<jadefx::InlineCssTextArea>();
    notes->setPrefSize(240, 160);
    auto scene = jadefx::make<jadefx::Scene>(notes, 240, 160);
    scene->layout(240, 160, 0);
    notes->requestFocus();

    notes->setText("abcdef");
    notes->setStyle(0, 3, "color: #111111;");
    notes->setStyle(3, 6, "color: #222222;");
    notes->forgetHistory();
    notes->selectAll();
    Key(*scene, jadefx::Key::B, jadefx::Key::ModControl);
    Expect(StyleAt(*notes, 0, 3).bold && StyleAt(*notes, 3, 6).bold, "bold covers every span in the selection");
    Expect(StyleAt(*notes, 0, 3).inlineCss.find("#111111") != std::string::npos, "bold keeps the first color");
    Expect(StyleAt(*notes, 3, 6).inlineCss.find("#222222") != std::string::npos, "bold keeps the second color");
    notes->undo();
    Expect(!StyleAt(*notes, 0, 3).bold && !StyleAt(*notes, 3, 6).bold, "one undo removes bold from every span");

    notes->setText("hello notes");
    notes->setStyle(0, 5, "color: #8250df;");
    notes->setStyle(6, 11, "color: #0a7d33; text-decoration: line-through;");
    notes->forgetHistory();
    notes->selectRange(0, 5);
    Key(*scene, jadefx::Key::B, jadefx::Key::ModControl);
    const jadefx::TextStyle bold = StyleAt(*notes, 0, 5);
    Expect(bold.bold && bold.hasFill, "notes bold keeps the color");
    Expect(bold.inlineCss.find("font-weight") != std::string::npos, "notes bold is stored as CSS");
    Expect(!StyleAt(*notes, 6, 11).bold, "bold stops at the selection");
    notes->undo();
    Expect(!StyleAt(*notes, 0, 5).bold, "undo removes the bold");
    Expect(StyleAt(*notes, 0, 5).inlineCss.find("font-weight") == std::string::npos, "undo restores the CSS");

    notes->selectRange(6, 11);
    Key(*scene, jadefx::Key::U, jadefx::Key::ModControl);
    const jadefx::TextStyle lined = StyleAt(*notes, 6, 11);
    Expect(lined.underline && lined.strikethrough && lined.hasFill, "underline keeps the other decorations");
    Expect(lined.inlineCss.find("underline") != std::string::npos, "underline is stored as CSS");
    Key(*scene, jadefx::Key::U, jadefx::Key::ModControl);
    const jadefx::TextStyle restored = StyleAt(*notes, 6, 11);
    Expect(!restored.underline && restored.strikethrough, "underline toggles off and line-through stays");

    notes->moveTo(notes->length());
    Key(*scene, jadefx::Key::B, jadefx::Key::ModControl);
    scene->noteKey(jadefx::Key::Unknown, false, false, 0);
    scene->noteText("!");
    Expect(notes->getText() == "hello notes!", "a caret style applies to the next character");
    const jadefx::TextStyle typed = StyleAt(*notes, notes->length() - 1, notes->length());
    Expect(typed.bold && typed.hasFill && typed.strikethrough, "the typed character keeps the caret CSS");

    notes->setText("frozen");
    notes->forgetHistory();
    notes->selectAll();
    notes->setEditable(false);
    Key(*scene, jadefx::Key::B, jadefx::Key::ModControl);
    Expect(!StyleAt(*notes, 0, 6).bold, "a read-only notes area ignores bold");
    notes->setEditable(true);

    auto plain = jadefx::make<jadefx::StyledTextArea>();
    plain->setPrefSize(240, 160);
    auto plainScene = jadefx::make<jadefx::Scene>(plain, 240, 160);
    plainScene->layout(240, 160, 0);
    plain->setText("hello");
    plain->selectAll();
    plain->requestFocus();
    Key(*plainScene, jadefx::Key::B, jadefx::Key::ModControl);
    Key(*plainScene, jadefx::Key::U, jadefx::Key::ModControl);
    Expect(!StyleAt(*plain, 0, 5).bold && !StyleAt(*plain, 0, 5).underline, "the base editor has no style shortcuts");

    auto code = jadefx::make<jadefx::CodeArea>();
    code->setPrefSize(240, 160);
    auto codeScene = jadefx::make<jadefx::Scene>(code, 240, 160);
    codeScene->layout(240, 160, 0);
    code->setText("hello");
    code->selectAll();
    code->requestFocus();
    Key(*codeScene, jadefx::Key::B, jadefx::Key::ModControl);
    Key(*codeScene, jadefx::Key::U, jadefx::Key::ModControl);
    Expect(!StyleAt(*code, 0, 5).bold && !StyleAt(*code, 0, 5).underline, "a code area ignores bold and underline");

    auto source = jadefx::make<jadefx::StyledTextArea>();
    source->setText("red");
    jadefx::TextStyle color;
    color.hasFill = true;
    color.fill = jadefx::Color::parse("#ff0000");
    color.bold = true;
    source->setStyle(0, 3, color);
    source->selectAll();
    source->copy();
    code->setText("");
    code->paste();
    Expect(code->getText() == "red", "a code area pastes the characters");
    Expect(!StyleAt(*code, 0, 3).hasFill && !StyleAt(*code, 0, 3).bold, "a code area drops pasted styles");
}

}  // namespace

int RunRichTextTests() {
    gFailures = 0;
    TestDocument();

    auto area = Editor("", 240, 160);
    auto scene = jadefx::make<jadefx::Scene>(area, 240, 160);
    scene->layout(240, 160, 0);
    area->requestFocus();
    TestStylesAndUndo(*scene, *area);
    TestWrapFoldAndClipboard(*scene, *area);
    TestCharacterStyleShortcuts();
    return gFailures;
}

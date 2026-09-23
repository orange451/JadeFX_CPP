#include "jadefx/jadefx.hpp"

#include <cctype>
#include <string>

namespace {

constexpr const char* kStylesheet = R"CSS(
scene {
    background-color: #e8eaed;
    font-family: "Open Sans";
    font-size: 15px;
    color: #202124;
}
.editor {
    background-color: white;
    width: calc(100% - 32px);
    height: calc(100% - 32px);
    border-radius: 8px;
    border-width: 1px;
    border-color: #dadce0;
    padding: 8px;
    box-shadow: 8px 16px 32px 0px rgba(0, 0, 0, 0.18);
}
.editor:focus {
    border-color: #1a73e8;
}
.hint {
    color: #5f6368;
    font-size: 13px;
    padding: 4px 8px 0 8px;
}
tab {
    background-color: #e8eaed;
    border-radius: 8px 8px 0 0;
    color: #3c4043;
}
tab:selected {
    background-color: white;
    color: #1a73e8;
}
tab:hover {
    color: #174ea6;
}
)CSS";

constexpr const char* kSource =
    "// JadeFX rich text\n"
    "// Type, select, and use the shortcuts in the hint.\n"
    "int fibonacci(int n) {\n"
    "    if (n < 2) {\n"
    "        return n;\n"
    "    }\n"
    "    return fibonacci(n - 1) + fibonacci(n - 2);\n"
    "}\n"
    "\n"
    "int main() {\n"
    "    const char* message = \"hello\";\n"
    "    int value = 42;\n"
    "    return fibonacci(value);\n"
    "}\n";

bool Keyword(const std::string& word) {
    static const char* kWords[] = {"int", "void", "return", "const", "if", "else", "for", "while", "class", "auto",
                                   "true", "false", "char"};
    for (const char* candidate : kWords) {
        if (word == candidate) {
            return true;
        }
    }
    return false;
}

jadefx::StyleSpans Highlight(const std::string& text) {
    jadefx::StyleSpansBuilder builder;
    auto add = [&](const char* name, int length) {
        jadefx::TextStyle style;
        if (name != nullptr) {
            style.styleClass = name;
        }
        builder.add(style, length);
    };
    std::size_t index = 0;
    while (index < text.size()) {
        if (text[index] == '/' && index + 1 < text.size() && text[index + 1] == '/') {
            int length = 0;
            while (index < text.size() && text[index] != '\n') {
                ++index;
                ++length;
            }
            add("comment", length);
            continue;
        }
        if (text[index] == '"') {
            int length = 1;
            ++index;
            while (index < text.size() && text[index] != '"' && text[index] != '\n') {
                ++index;
                ++length;
            }
            if (index < text.size() && text[index] == '"') {
                ++index;
                ++length;
            }
            add("string", length);
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
            int length = 0;
            while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
                ++index;
                ++length;
            }
            add("number", length);
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(text[index])) != 0 || text[index] == '_') {
            const std::size_t start = index;
            while (index < text.size() &&
                   (std::isalnum(static_cast<unsigned char>(text[index])) != 0 || text[index] == '_')) {
                ++index;
            }
            const std::string word = text.substr(start, index - start);
            add(Keyword(word) ? "keyword" : nullptr, static_cast<int>(word.size()));
            continue;
        }
        add(nullptr, 1);
        ++index;
    }
    return builder.create();
}

void DefineCodeStyles(jadefx::StyleClassedTextArea& area) {
    jadefx::TextStyle comment;
    comment.hasFill = true;
    comment.fill = jadefx::Color::parse("#6a737d");
    area.defineStyleClass("comment", comment);

    jadefx::TextStyle keyword;
    keyword.hasFill = true;
    keyword.fill = jadefx::Color::parse("#8250df");
    keyword.bold = true;
    area.defineStyleClass("keyword", keyword);

    jadefx::TextStyle stringStyle;
    stringStyle.hasFill = true;
    stringStyle.fill = jadefx::Color::parse("#0a7d33");
    area.defineStyleClass("string", stringStyle);

    jadefx::TextStyle number;
    number.hasFill = true;
    number.fill = jadefx::Color::parse("#0550ae");
    area.defineStyleClass("number", number);
}

class RichTextApp : public jadefx::Application {
public:
    void start(jadefx::Stage& stage, int, char**) override {
        auto code = jadefx::make<jadefx::CodeArea>();
        code->getClassList().add("editor");
        DefineCodeStyles(*code);
        code->setText(kSource);
        code->setStyleSpans(0, Highlight(code->getText()));
        code->foldParagraphs(2, 7);
        code->moveTo(0);
        jadefx::CodeArea* editor = code.get();
        code->setOnPlainTextChange([editor](const jadefx::PlainTextChange&) {
            editor->suspendUndo();
            editor->setStyleSpans(0, Highlight(editor->getText()));
            editor->resumeUndo();
        });

        auto notes = jadefx::make<jadefx::InlineCssTextArea>();
        notes->getClassList().add("editor");
        notes->setWrapText(true);
        const std::string body =
            "Rich text\n"
            "Select a word and press Cmd/Ctrl+B for bold, or Cmd/Ctrl+U for underline.\n"
            "Colors, sizes, and underlines share a paragraph. Undo puts them back.";
        notes->setText(body);
        notes->setStyle(0, 9, "color: #174ea6; font-size: 28px; font-weight: bold;");
        const std::size_t boldAt = body.find("bold");
        notes->setStyle(static_cast<int>(boldAt), static_cast<int>(boldAt + 4), "font-weight: bold; color: #8250df;");
        const std::size_t lineAt = body.find("underline");
        notes->setStyle(static_cast<int>(lineAt), static_cast<int>(lineAt + 9),
                        "text-decoration: underline; color: #0a7d33;");

        auto tabs = jadefx::make<jadefx::TabPane>();
        tabs->setTabClosingPolicy(jadefx::TabPane::TabClosingPolicy::Unavailable);
        tabs->getTabs().add(jadefx::make<jadefx::Tab>("Code", code));
        tabs->getTabs().add(jadefx::make<jadefx::Tab>("Notes", notes));

        auto hint = jadefx::make<jadefx::Label>(
            "Arrows, words, page, undo, clipboard. Alt-click adds a caret. Click a fold arrow, or Cmd/Ctrl+Alt+[ to fold.");
        hint->getClassList().add("hint");

        auto root = jadefx::make<jadefx::BorderPane>();
        root->setTop(hint);
        root->setCenter(tabs);

        auto scene = jadefx::make<jadefx::Scene>(root, 880, 560);
        scene->setStylesheet(kStylesheet);
        stage.setTitle("Rich text");
        stage.setScene(scene);
        code->requestFocus();
    }
};

}  // namespace

int main(int argc, char** argv) {
    return jadefx::Application::launch(std::make_unique<RichTextApp>(), argc, argv);
}

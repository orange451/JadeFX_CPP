#pragma once

#include "jadefx/scene/text/StyledDocument.hpp"

#include <functional>
#include <string_view>

namespace jadefx {

// One mutation. A paragraph-style change leaves the text documents empty and sets paragraphIndex.
struct DocumentChange {
    int position = 0;
    StyledDocument removed;
    StyledDocument inserted;
    int paragraphIndex = -1;
    ParagraphStyle oldParagraph;
    ParagraphStyle newParagraph;

    bool isParagraphStyle() const { return paragraphIndex >= 0; }
    bool changesText() const { return removed.text() != inserted.text(); }
};

// The mutable document behind a rich text area. Offsets are Unicode code points.
// Listeners run after the document has already changed.
class EditableStyledDocument {
public:
    EditableStyledDocument();
    explicit EditableStyledDocument(std::string text, const TextStyle& style = {},
                                    const ParagraphStyle& paragraphStyle = {});

    int length() const { return document_.length(); }
    int paragraphCount() const { return document_.paragraphCount(); }
    std::string text() const { return document_.text(); }
    std::string text(int start, int end) const { return document_.text(start, end); }
    const Paragraph& paragraph(int index) const { return document_.paragraph(index); }
    TextPos position(int offset) const { return document_.position(offset); }
    int offset(int paragraph, int column) const { return document_.offset(paragraph, column); }
    StyledDocument snapshot() const { return document_; }
    StyledDocument subDocument(int start, int end) const { return document_.sub(start, end); }
    StyleSpans styleSpans(int start, int end) const { return document_.styleSpans(start, end); }
    TextStyle styleAt(int offset) const { return document_.styleAt(offset); }

    DocumentChange replace(int start, int end, const StyledDocument& replacement);
    DocumentChange replaceText(int start, int end, std::string_view text, const TextStyle& style,
                               const ParagraphStyle& paragraphStyle);
    DocumentChange setStyle(int start, int end, const TextStyle& style);
    DocumentChange setStyleSpans(int start, const StyleSpans& spans);
    DocumentChange setParagraphStyle(int paragraph, const ParagraphStyle& style);

    void setOnPlainTextChange(std::function<void(const PlainTextChange&)> handler) { onPlain_ = std::move(handler); }
    void setOnRichTextChange(std::function<void(const DocumentChange&)> handler) { onRich_ = std::move(handler); }

private:
    void install(std::vector<Paragraph> paragraphs);
    void notify(const DocumentChange& change);

    StyledDocument document_;
    int revisionClock_ = 0;
    std::function<void(const PlainTextChange&)> onPlain_;
    std::function<void(const DocumentChange&)> onRich_;
};

}  // namespace jadefx

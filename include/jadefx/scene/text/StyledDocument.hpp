#pragma once

#include "jadefx/scene/text/StyleSpans.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace jadefx {

// A caret is between code points. column is a code-point index in that paragraph.
// The newline that separates paragraphs is not stored in the paragraph text.
// absolute position 0 is before the first code point. A document "some\nthing"
// has length 10, and position 4 is the end of the first paragraph while position 5
// is the start of the second. getAbsolutePosition(0, length + 1) lands on the next paragraph.
struct TextPos {
    int paragraph = 0;
    int column = 0;
};

struct IndexRange {
    int start = 0;
    int end = 0;

    int length() const { return end > start ? end - start : 0; }
    bool empty() const { return start == end; }

    static IndexRange of(int a, int b) {
        if (a > b) {
            return {b, a};
        }
        return {a, b};
    }
};

struct PlainTextChange {
    int position = 0;
    std::string removed;
    std::string inserted;
};

// One paragraph. The text has no newline. Style span lengths sum to the code-point length.
// An empty paragraph keeps a single zero-length span so it still has an insertion style.
class Paragraph {
public:
    Paragraph();

    int length() const { return static_cast<int>(text_.size()); }
    const std::u32string& content() const { return text_; }
    std::string text() const;
    const std::vector<StyleSpan>& spans() const { return spans_; }
    const ParagraphStyle& paragraphStyle() const { return style_; }
    int revision() const { return revision_; }

private:
    friend class StyledDocument;
    friend class EditableStyledDocument;

    std::u32string text_;
    std::vector<StyleSpan> spans_;
    ParagraphStyle style_;
    int revision_ = 0;
};

// Immutable rich text. Paragraphs are joined by '\n' in text().
// An empty document is one empty paragraph, and its length is 0.
class StyledDocument {
public:
    StyledDocument();

    static StyledDocument fromPlain(std::string_view text, const TextStyle& style = {},
                                    const ParagraphStyle& paragraphStyle = {});

    int length() const;
    int paragraphCount() const { return static_cast<int>(paragraphs_.size()); }
    const Paragraph& paragraph(int index) const;

    std::string text() const;
    std::string text(int start, int end) const;

    TextPos position(int offset) const;
    int offset(int paragraph, int column) const;

    StyledDocument sub(int start, int end) const;
    StyleSpans styleSpans(int start, int end) const;
    TextStyle styleAt(int offset) const;

    static StyledDocument concat(const StyledDocument& left, const StyledDocument& right);

    bool operator==(const StyledDocument& other) const;
    bool operator!=(const StyledDocument& other) const { return !(*this == other); }

private:
    friend class EditableStyledDocument;

    explicit StyledDocument(std::vector<Paragraph> paragraphs);

    static void overlay(Paragraph& paragraph, int start, int end, const TextStyle& style);
    static TextStyle styleAtColumn(const Paragraph& paragraph, int column);
    static void normalize(Paragraph& paragraph);
    static std::vector<StyleSpan> sliceSpans(const Paragraph& paragraph, int start, int end);
    static Paragraph slice(const Paragraph& source, int start, int end);
    static void append(Paragraph& dest, const Paragraph& source);

    std::vector<Paragraph> paragraphs_;
};

}  // namespace jadefx

#pragma once

#include "jadefx/scene/controls/Controls.hpp"
#include "jadefx/scene/controls/ScrollTrack.hpp"
#include "jadefx/scene/text/EditableStyledDocument.hpp"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace jadefx {

namespace detail {
struct MeasuredLine;
}

// Where a pointer landed in the text. insertionIndex is the caret gap nearest the pointer,
// and column is that gap's column in paragraph. characterIndex is the code point whose glyph
// is under the pointer, or -1 past the end of a line. leading is true on the left half of it.
// onLine is false above the first line and below the last. There no glyph is under the pointer,
// and insertionIndex is still the nearest gap on that line, so a click there places the caret.
struct CharacterHit {
    bool valid = false;
    int insertionIndex = 0;
    int characterIndex = -1;
    int paragraph = 0;
    int column = 0;
    bool leading = true;
    bool onLine = false;
};

// A rectangle in window points. valid is false when the caret is outside the viewport.
struct TextBounds {
    bool valid = false;
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
};

// How a text mark is drawn. Error is the strongest gutter color.
enum class TextMarkSeverity { Error, Warning, Information, Hint };

// One underline range in code points, the same offsets as the caret.
struct TextMark {
    int start = 0;
    int end = 0;
    TextMarkSeverity severity = TextMarkSeverity::Error;

    bool operator==(const TextMark& other) const {
        return start == other.start && end == other.end && severity == other.severity;
    }
};

// Which part of the vertical scroll bar's width a scroll mark covers.
enum class ScrollMarkLane { Full, Left, Right };

// A band on the vertical scroll bar at the height of a code-point range, as in VS Code's
// overview ruler, so marks anywhere in the text show at once.
struct ScrollMark {
    int start = 0;
    int end = 0;
    Color color;
    ScrollMarkLane lane = ScrollMarkLane::Full;

    bool operator==(const ScrollMark& other) const {
        return start == other.start && end == other.end && lane == other.lane && color.r == other.color.r &&
               color.g == other.color.g && color.b == other.color.b && color.a == other.color.a;
    }
};

// A click on a link: its href and the run of text it covers.
struct LinkEvent {
    std::string href;
    IndexRange range;
};

// When links draw their underline. Hover is the JavaFX Hyperlink look.
enum class LinkUnderline { Hover, Always, Never };

// Virtualized rich text editor in the shape of RichTextFX's StyledTextArea.
// Text is a list of paragraphs. A newline starts a paragraph and counts as one code point.
// Only the paragraphs inside the viewport are drawn. Styles cover code-point ranges.
//
// The area shows a caret while it is focused, scrolls with the wheel, and edits from the
// keyboard: arrows, word jumps, home and end, page up and down, enter, tab, undo, and clipboard.
// Alt-click adds a caret.
// Text whose style has an href is a link. It takes --link-color unless it sets its
// own fill, and a click on it that does not drag reports it through setOnLinkClicked.
// In an editable area a link is followed with Cmd/Ctrl held, as in code editors,
// so a plain click still places the caret in it.
// Italic is stored but drawn with the regular face, because the bundled font has no italic.
class StyledTextArea : public Controls {
public:
    enum class CaretVisibility { Auto, On, Off };

    StyledTextArea();

    const char* getElementType() const override { return "styledtextarea"; }

    int length() const { return content_.length(); }
    int paragraphCount() const { return content_.paragraphCount(); }
    std::string getText() const { return content_.text(); }
    std::string getText(int start, int end) const { return content_.text(start, end); }
    std::string getText(int paragraph) const;
    void setText(std::string text);
    void clear();
    void appendText(std::string text);
    void insertText(int index, std::string text);
    void deleteText(int start, int end);
    void replaceText(int start, int end, std::string text);
    void replace(int start, int end, const StyledDocument& replacement);

    const Paragraph& getParagraph(int index) const { return content_.paragraph(index); }
    ParagraphStyle getParagraphStyle(int index) const { return content_.paragraph(index).paragraphStyle(); }
    void setParagraphStyle(int index, ParagraphStyle style);
    StyledDocument subDocument(int start, int end) const { return content_.subDocument(start, end); }
    StyledDocument getDocument() const { return content_.snapshot(); }
    const EditableStyledDocument& getContent() const { return content_; }

    TextPos position(int offset) const { return content_.position(offset); }
    int absolutePosition(int paragraph, int column) const { return content_.offset(paragraph, column); }

    int caretPosition() const;
    int anchor() const;
    int caretColumn() const;
    int currentParagraph() const;
    IndexRange selection() const;
    std::string selectedText() const;
    // Every caret, primary last. Each range is ordered start then end.
    std::vector<IndexRange> selections() const;
    void selectRange(int anchorPos, int caret);
    void selectRange(int anchorParagraph, int anchorColumn, int caretParagraph, int caretColumn);
    void moveTo(int offset);
    void moveTo(int paragraph, int column);
    void displaceCaret(int offset);
    void selectAll();
    void deselect();
    void addCaret(int offset);
    void clearSecondaryCarets();

    // Node's setStyle(css) styles the area itself; these style ranges of its text.
    using Controls::setStyle;
    void setStyle(int start, int end, const TextStyle& style);
    // undoable is false for highlighters that restyle after every text change.
    void setStyleSpans(int start, const StyleSpans& spans, bool undoable = false);
    StyleSpans getStyleSpans(int start, int end) const { return content_.styleSpans(start, end); }

    // Makes the range a link to href, keeping its other styling. An empty href removes links.
    void setLink(int start, int end, const std::string& href);
    // The href of the code point at index, or empty.
    std::string linkAt(int index) const;
    // The run of text around index that links to the same href, across line breaks.
    // Empty when index is not in a link.
    IndexRange linkRange(int index) const;
    // The href under the pointer while it can be followed, or empty. Every run with
    // this href is drawn hovered.
    const std::string& hoveredLink() const { return hoveredLink_; }
    void setLinkUnderline(LinkUnderline underline) { linkUnderline_ = underline; }
    LinkUnderline getLinkUnderline() const { return linkUnderline_; }
    void setOnLinkClicked(std::function<void(const LinkEvent&)> handler) { onLinkClicked_ = std::move(handler); }

    void undo();
    void redo();
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    void forgetHistory();
    void suspendUndo();
    void resumeUndo();

    void copy();
    void cut();
    void paste();

    void setEditable(bool editable) { editable_ = editable; }
    bool isEditable() const { return editable_; }
    void setWrapText(bool wrap);
    bool isWrapText() const { return wrap_; }
    void setShowCaret(CaretVisibility visibility) { showCaret_ = visibility; }
    CaretVisibility getShowCaret() const { return showCaret_; }
    void setFollowCaret(bool follow) { followCaret_ = follow; }
    bool isFollowCaret() const { return followCaret_; }
    void setTabSize(int spaces);
    int getTabSize() const { return tabSize_; }
    void setInsertSpacesForTab(bool spaces) { insertSpaces_ = spaces; }
    bool isInsertSpacesForTab() const { return insertSpaces_; }
    void setAutoIndent(bool enabled) { autoIndent_ = enabled; }
    bool isAutoIndent() const { return autoIndent_; }
    void setUseInitialStyleForInsertion(bool use) { useInitialStyle_ = use; }
    bool isUseInitialStyleForInsertion() const { return useInitialStyle_; }
    void setInitialTextStyle(TextStyle style) { initialStyle_ = std::move(style); }
    const TextStyle& getInitialTextStyle() const { return initialStyle_; }
    void setInitialParagraphStyle(ParagraphStyle style) { initialParagraph_ = std::move(style); }

    void setShowLineNumbers(bool show);
    bool isShowLineNumbers() const { return lineNumbers_; }
    void setHighlightCurrentParagraph(bool highlight) { highlightLine_ = highlight; }
    bool isHighlightCurrentParagraph() const { return highlightLine_; }

    // Collapses paragraphs (start, end] under the start paragraph. end must be greater than start.
    void foldParagraphs(int startParagraph, int endParagraph);
    void unfoldParagraphs(int paragraph);
    bool isFolded(int paragraph) const;

    double getScrollX() const { return scrollX_; }
    double getScrollY() const { return scrollY_; }
    void scrollTo(double x, double y);
    void showPosition(int offset);

    CharacterHit hit(double x, double y) const;
    TextBounds caretBounds() const;
    int visualLineCount() const;

    // A wavy underline under a code-point range, drawn with the text so it scrolls.
    // end <= start marks the caret gap at start. The script editor uses these for analysis.
    void setTextMarks(std::vector<TextMark> marks);
    const std::vector<TextMark>& textMarks() const { return textMarks_; }

    // Bands on the vertical scroll bar, drawn over its thumb, a later mark over an
    // earlier one. Each is at least a few points tall. While there are any, the bar
    // keeps its lane even when the text fits, and then each band sits beside its
    // line. A click on a band scrolls its text to the middle of the view. Like text
    // marks, they do not follow edits; set them again after the text changes.
    void setScrollMarks(std::vector<ScrollMark> marks);
    const std::vector<ScrollMark>& scrollMarks() const { return scrollMarks_; }

    // A node that sits in the text before the code point at offset, as an inline
    // object in RichTextFX: it takes its preferred width in the line, the caret and
    // wrapping step over it, and it takes its own clicks. It is not part of the text,
    // so an edit does not move it; set the nodes again after the text changes. The
    // script editor puts a color swatch before each Color3 this way.
    struct InlineNode {
        int offset = 0;
        std::shared_ptr<Node> node;
    };
    void setInlineNodes(std::vector<InlineNode> nodes);
    const std::vector<InlineNode>& getInlineNodes() const { return inlineNodes_; }
    // One paragraph's inline boxes as (column, width) pairs, sorted by column.
    using InlineBoxes = std::vector<std::pair<int, float>>;

    void setOnPlainTextChange(std::function<void(const PlainTextChange&)> handler) { onPlain_ = std::move(handler); }
    void setOnRichTextChange(std::function<void(const DocumentChange&)> handler) { onRich_ = std::move(handler); }
    void setOnMouseOverText(std::function<void(int index)> handler) { onHover_ = std::move(handler); }
    void setMouseOverTextDelay(int milliseconds) { hoverDelayMs_ = milliseconds; }

    void handleMousePressed(const MouseEvent& event) override;
    void handleMouseReleased(const MouseEvent& event) override;
    void handleMouseDragged(const MouseEvent& event) override;
    void handleMouseMoved(const MouseEvent& event) override;
    void handleHoverChanged() override;
    void handleScroll(ScrollEvent& event) override;
    void handleKey(KeyEvent& event) override;
    void handleText(TextEvent& event) override;
    Cursor cursorAt(double x, double y) const override;

protected:
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;
    void layoutChildren() override;
    void renderContent(UiRenderer& renderer, float opacity) override;
    virtual TextStyle resolveStyle(const TextStyle& style) const;
    TextStyle styleForInsertion(int offset) const;
    void setTypingStyle(TextStyle style);
    // Replaces each span's style in the range with map of it, as one undoable edit.
    void restyle(int start, int end, const std::function<TextStyle(const TextStyle&)>& map);
    void transact(bool coalesce, const std::function<void()>& body);
    // Styled paste keeps colors and weight when this returns true.
    virtual bool pasteKeepsStyle() const { return true; }

private:
    struct Selection {
        int anchor = 0;
        int caret = 0;
        int start() const { return anchor < caret ? anchor : caret; }
        int end() const { return anchor > caret ? anchor : caret; }
    };

    struct UndoEntry {
        std::vector<DocumentChange> changes;
        std::vector<Selection> before;
        std::vector<Selection> after;
        bool coalesce = false;
    };

    struct Fold {
        int start = 0;
        int end = 0;
    };

    struct LineLayout {
        int start = 0;
        int end = 0;
        float y = 0.f;
        float height = 0.f;
        float width = 0.f;
        float ascent = 0.f;
    };

    struct ParagraphLayout {
        int revision = -1;
        // The inline boxes the lines were wrapped with.
        InlineBoxes boxes;
        float wrapWidth = -1.f;
        bool wrapped = false;
        float fontSize = 0.f;
        std::string fontFamily;
        int tabSize = 0;
        float height = 0.f;
        float width = 0.f;
        std::vector<LineLayout> lines;
    };

    struct ViewLayout {
        float gutter = 0.f;
        float bar = 8.f;
        bool verticalBar = false;
        bool horizontalBar = false;
        float textX = 0.f;
        float textY = 0.f;
        float textW = 0.f;
        float textH = 0.f;
        float contentWidth = 0.f;
        float contentHeight = 0.f;
        std::vector<float> tops;
        std::vector<ParagraphLayout> paragraphs;
        float fontSize = 0.f;
        std::string fontFamily;
    };

    enum class Drag { None, Text, Word, Paragraph, VerticalBar, HorizontalBar };

    // A scroll mark placed on the vertical bar. top and bottom are points down the
    // track. textTop and textBottom are the lines it covers, in content points.
    struct ScrollBand {
        float top = 0.f;
        float bottom = 0.f;
        float textTop = 0.f;
        float textBottom = 0.f;
        ScrollMarkLane lane = ScrollMarkLane::Full;
        Color color;
    };

    Font areaFont() const;
    ParagraphStyle paragraphStyleForInsertion(int offset) const;
    void commit(UndoEntry entry);
    DocumentChange editReplace(int start, int end, const StyledDocument& replacement);
    void flushPending();
    void clampSelections();
    void collapseCarets(int offset);
    void applyEditToCarets(const std::string& text, bool typing);
    void deleteRanges(bool forward, bool word);
    void breakParagraphs();
    void indentLines(bool outdent);
    void retargetFolds(int startParagraph, int endParagraph, int delta);
    bool isHidden(int paragraph) const;
    bool isFoldHeader(int paragraph) const;
    int foldEnd(int paragraph) const;
    int step(int offset, int direction) const;
    int wordBoundary(int offset, int direction) const;
    void moveCarets(int direction, bool select, bool word);
    void moveVertical(int lines, bool select);
    void moveToDocument(bool end, bool select);
    void moveLineEdge(bool end, bool select);
    void page(bool down, bool select);
    void ensureCaretVisible();
    void markDirty();
    void rebuild() const;
    // The bands for the scroll marks, placed again when the marks are set or the layout moves them.
    const std::vector<ScrollBand>& scrollBands() const;
    void renderScrollMarks(UiRenderer& renderer, float absoluteX, float absoluteY, float opacity) const;
    // The scroll offset that centers the band under a press on the vertical bar, or -1 off every band.
    double scrollBandOffset(float localY) const;
    int primaryIndex() const;
    // A line of a paragraph measured with its styles and inline boxes.
    detail::MeasuredLine measureLine(int paragraph, int start, int end) const;
    // Sorts the inline nodes into boxes by paragraph, with their widths.
    void collectInlineBoxes() const;
    const InlineBoxes& inlineBoxesOf(int paragraph) const;
    void placeInlineNodes();
    Selection& primary();
    const Selection& primary() const;
    // The link under a window point that a click would follow, or empty.
    std::string followableLinkAt(double x, double y) const;

    EditableStyledDocument content_;
    std::vector<Selection> selections_;
    std::vector<UndoEntry> undo_;
    std::vector<UndoEntry> redo_;
    std::vector<Fold> folds_;
    std::vector<PlainTextChange> pendingPlain_;
    std::vector<DocumentChange> pendingRich_;
    UndoEntry* open_ = nullptr;
    int undoSuspend_ = 0;
    bool mergeArmed_ = false;

    bool editable_ = true;
    bool wrap_ = true;
    bool followCaret_ = true;
    bool lineNumbers_ = false;
    bool highlightLine_ = false;
    bool insertSpaces_ = false;
    bool autoIndent_ = false;
    bool useInitialStyle_ = false;
    int tabSize_ = 4;
    CaretVisibility showCaret_ = CaretVisibility::Auto;
    TextStyle initialStyle_;
    ParagraphStyle initialParagraph_;
    std::optional<TextStyle> typingStyle_;

    mutable double scrollX_ = 0;
    mutable double scrollY_ = 0;
    double preferredX_ = -1;
    bool caretDirty_ = false;
    bool flushing_ = false;
    mutable bool layoutDirty_ = true;
    double caretMovedAt_ = 0;
    mutable ViewLayout view_;
    mutable ScrollTrack verticalScroll_;
    mutable ScrollTrack horizontalScroll_;

    Drag drag_ = Drag::None;
    int anchorWord_ = 0;
    int clickCount_ = 0;
    float scrollGrab_ = 0.f;

    int hoverIndex_ = -1;
    int hoverDelayMs_ = 500;
    double hoverSince_ = 0;
    bool hoverFired_ = false;
    std::function<void(const PlainTextChange&)> onPlain_;
    std::function<void(const DocumentChange&)> onRich_;
    std::function<void(int)> onHover_;
    std::vector<TextMark> textMarks_;
    std::vector<ScrollMark> scrollMarks_;
    mutable std::vector<ScrollBand> scrollBands_;
    // What the bands were placed for. Setting the marks, even to the same ones, places them again.
    mutable bool scrollBandsDirty_ = true;
    mutable float scrollBandsContent_ = -1.f;
    mutable float scrollBandsTrack_ = -1.f;
    mutable int scrollBandsParagraphs_ = -1;
    std::vector<InlineNode> inlineNodes_;
    // Only the paragraphs that have boxes, so a long text without any costs nothing.
    mutable std::map<int, InlineBoxes> inlineBoxes_;

    LinkUnderline linkUnderline_ = LinkUnderline::Hover;
    std::string hoveredLink_;
    // The link a press landed on. A release on it without a drag follows it.
    std::string pressedLink_;
    std::function<void(const LinkEvent&)> onLinkClicked_;
};

}  // namespace jadefx

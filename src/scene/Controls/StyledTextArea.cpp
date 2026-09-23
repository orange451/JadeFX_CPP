#include "jadefx/scene/Controls/StyledTextArea.hpp"

#include "jadefx/scene/Scene.hpp"
#include "gl/UiRenderer.hpp"
#include "scene/text/Unicode.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <utility>

namespace jadefx {
namespace {

struct LocalClipboard {
    std::string plain;
    StyledDocument document;
    bool styled = false;
};

LocalClipboard& Clipboard() {
    static LocalClipboard clipboard;
    return clipboard;
}

double Now() {
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

bool IsSpace(char32_t codepoint) {
    return codepoint == U' ' || codepoint == U'\t' || codepoint == U'\n' || codepoint == U'\r' || codepoint == 0x00A0;
}

bool IsWord(char32_t codepoint) {
    if (codepoint == U'_') {
        return true;
    }
    if (codepoint < 128) {
        return std::isalnum(static_cast<unsigned char>(codepoint)) != 0;
    }
    return !IsSpace(codepoint);
}

bool CodePointAt(const EditableStyledDocument& document, int offset, char32_t& codepoint) {
    if (offset < 0 || offset >= document.length()) {
        return false;
    }
    const TextPos pos = document.position(offset);
    const std::u32string& text = document.paragraph(pos.paragraph).content();
    if (pos.column >= 0 && pos.column < static_cast<int>(text.size())) {
        codepoint = text[static_cast<std::size_t>(pos.column)];
        return true;
    }
    codepoint = U'\n';
    return true;
}

TextStyle SpanStyle(const Paragraph& paragraph, int column) {
    if (paragraph.spans().empty()) {
        return {};
    }
    if (paragraph.length() == 0) {
        return paragraph.spans().front().style;
    }
    if (column < 0) {
        column = 0;
    }
    if (column >= paragraph.length()) {
        column = paragraph.length() - 1;
    }
    int cursor = 0;
    for (const StyleSpan& span : paragraph.spans()) {
        if (span.length <= 0) {
            continue;
        }
        if (column < cursor + span.length) {
            return span.style;
        }
        cursor += span.length;
    }
    return paragraph.spans().back().style;
}

void Fill(UiRenderer& renderer, float x, float y, float width, float height, const Color& color) {
    if (width <= 0.f || height <= 0.f || color.a <= 0.f) {
        return;
    }
    const float radius[4] = {};
    const float at = 0.f;
    renderer.fillRounded(x, y, width, height, radius, &color, &at, 1, 0.f);
}

struct MeasuredLine {
    struct Piece {
        int begin = 0;
        int end = 0;
        float x = 0.f;
        TextStyle style;
        float fontSize = 0.f;
        bool tab = false;
    };

    std::vector<float> caret;
    std::vector<Piece> pieces;
    float width = 0.f;
    float height = 0.f;
    float ascent = 0.f;
};

Font FontFor(const std::string& family, float baseSize, const TextStyle& style) {
    const float size = style.fontSize > 0.f ? style.fontSize : baseSize;
    return Font(family, size > 0.f ? size : 1.f);
}

MeasuredLine MeasureRange(const std::u32string& text, int start, int end, const std::string& family, float baseSize,
                          int tabSize, const std::function<TextStyle(int)>& styleAt) {
    MeasuredLine measured;
    const Font base(family, baseSize > 0.f ? baseSize : 1.f);
    measured.height = std::max(1.f, base.lineHeight());
    measured.ascent = base.ascent();
    measured.caret.push_back(0.f);
    if (end < start) {
        end = start;
    }
    if (start < 0) {
        start = 0;
    }
    if (end > static_cast<int>(text.size())) {
        end = static_cast<int>(text.size());
    }
    float pen = 0.f;
    int index = start;
    while (index < end) {
        const TextStyle style = styleAt(index);
        const Font font = FontFor(family, baseSize, style);
        measured.height = std::max(measured.height, font.lineHeight());
        measured.ascent = std::max(measured.ascent, font.ascent());
        const char32_t codepoint = text[static_cast<std::size_t>(index)];
        if (codepoint == U'\t') {
            const float space = std::max(1.f, font.measureWidth(" "));
            const float tab = space * static_cast<float>(std::max(1, tabSize));
            const float next = std::floor(pen / tab + 0.0001f) * tab + tab;
            measured.pieces.push_back(MeasuredLine::Piece{index, index + 1, pen, style, font.size(), true});
            pen = next;
            measured.caret.push_back(pen);
            ++index;
            continue;
        }
        int next = index + 1;
        while (next < end && text[static_cast<std::size_t>(next)] != U'\t' && styleAt(next) == style) {
            ++next;
        }
        const std::u32string chunk = text.substr(static_cast<std::size_t>(index), static_cast<std::size_t>(next - index));
        const ShapedText shaped = font.shape(Utf8(chunk));
        measured.pieces.push_back(MeasuredLine::Piece{index, next, pen, style, font.size(), false});
        if (static_cast<int>(shaped.glyphs.size()) == next - index) {
            for (int glyph = 0; glyph < next - index; ++glyph) {
                const float right =
                    pen + (glyph + 1 == next - index ? shaped.width : shaped.glyphs[static_cast<std::size_t>(glyph + 1)].x);
                measured.caret.push_back(right);
            }
            pen += shaped.width;
        } else {
            for (int column = index; column < next; ++column) {
                pen += font.measureWidth(Utf8(text[static_cast<std::size_t>(column)]));
                measured.caret.push_back(pen);
            }
        }
        index = next;
    }
    measured.width = pen;
    const int columns = end - start;
    if (static_cast<int>(measured.caret.size()) < columns + 1) {
        measured.caret.resize(static_cast<std::size_t>(columns + 1), pen);
    }
    return measured;
}

float AdvanceOf(char32_t codepoint, float pen, const Font& font, int tabSize,
                std::unordered_map<char32_t, float>& cache) {
    if (codepoint == U'\t') {
        float space = 0.f;
        const auto found = cache.find(U' ');
        if (found == cache.end()) {
            space = std::max(1.f, font.measureWidth(" "));
            cache.emplace(U' ', space);
        } else {
            space = found->second;
        }
        const float tab = space * static_cast<float>(std::max(1, tabSize));
        const float next = std::floor(pen / tab + 0.0001f) * tab + tab;
        return std::max(0.f, next - pen);
    }
    const auto found = cache.find(codepoint);
    if (found != cache.end()) {
        return found->second;
    }
    const float width = font.measureWidth(Utf8(codepoint));
    cache.emplace(codepoint, width);
    return width;
}

}  // namespace

StyledTextArea::StyledTextArea() {
    setDefaultCursor(Cursor::Text);
    selections_.push_back(Selection{});
    content_.setOnPlainTextChange([this](const PlainTextChange& change) { pendingPlain_.push_back(change); });
    content_.setOnRichTextChange([this](const DocumentChange& change) { pendingRich_.push_back(change); });
    caretMovedAt_ = Now();
}

std::string StyledTextArea::getText(int paragraph) const {
    if (paragraph < 0 || paragraph >= content_.paragraphCount()) {
        return {};
    }
    return content_.paragraph(paragraph).text();
}

void StyledTextArea::setText(std::string text) {
    transact(false, [&] {
        const DocumentChange change =
            editReplace(0, content_.length(), StyledDocument::fromPlain(text, initialStyle_, initialParagraph_));
        const int end = change.position + change.inserted.length();
        selections_ = {Selection{end, end}};
    });
}

void StyledTextArea::clear() { setText({}); }

void StyledTextArea::appendText(std::string text) { insertText(content_.length(), std::move(text)); }

void StyledTextArea::insertText(int index, std::string text) { replaceText(index, index, std::move(text)); }

void StyledTextArea::deleteText(int start, int end) { replaceText(start, end, {}); }

void StyledTextArea::replaceText(int start, int end, std::string text) {
    if (!editable_) {
        return;
    }
    const TextStyle style = styleForInsertion(std::min(start, end));
    const ParagraphStyle paragraphStyle = paragraphStyleForInsertion(std::min(start, end));
    transact(false, [&] {
        const DocumentChange change =
            editReplace(start, end, StyledDocument::fromPlain(text, style, paragraphStyle));
        const int caret = change.position + change.inserted.length();
        selections_ = {Selection{caret, caret}};
    });
}

void StyledTextArea::replace(int start, int end, const StyledDocument& replacement) {
    if (!editable_) {
        return;
    }
    transact(false, [&] {
        const DocumentChange change = editReplace(start, end, replacement);
        const int caret = change.position + change.inserted.length();
        selections_ = {Selection{caret, caret}};
    });
}

void StyledTextArea::setParagraphStyle(int index, ParagraphStyle style) {
    transact(false, [&] {
        const DocumentChange change = content_.setParagraphStyle(index, style);
        markDirty();
        if (change.isParagraphStyle() && !(change.oldParagraph == change.newParagraph) && open_ != nullptr) {
            open_->changes.push_back(change);
        }
    });
}

int StyledTextArea::primaryIndex() const {
    if (selections_.empty()) {
        return 0;
    }
    return static_cast<int>(selections_.size()) - 1;
}

StyledTextArea::Selection& StyledTextArea::primary() {
    if (selections_.empty()) {
        selections_.push_back(Selection{});
    }
    return selections_[static_cast<std::size_t>(primaryIndex())];
}

const StyledTextArea::Selection& StyledTextArea::primary() const {
    static const Selection kEmpty;
    if (selections_.empty()) {
        return kEmpty;
    }
    return selections_[static_cast<std::size_t>(primaryIndex())];
}

int StyledTextArea::caretPosition() const { return primary().caret; }

int StyledTextArea::anchor() const { return primary().anchor; }

int StyledTextArea::caretColumn() const { return content_.position(primary().caret).column; }

int StyledTextArea::currentParagraph() const { return content_.position(primary().caret).paragraph; }

IndexRange StyledTextArea::selection() const { return {primary().start(), primary().end()}; }

std::string StyledTextArea::selectedText() const { return content_.text(primary().start(), primary().end()); }

std::vector<IndexRange> StyledTextArea::selections() const {
    std::vector<IndexRange> ranges;
    ranges.reserve(selections_.size());
    for (const Selection& selection : selections_) {
        ranges.push_back({selection.start(), selection.end()});
    }
    return ranges;
}

void StyledTextArea::selectRange(int anchorPos, int caret) {
    if (anchorPos > content_.length()) {
        anchorPos = content_.length();
    }
    if (caret > content_.length()) {
        caret = content_.length();
    }
    if (anchorPos < 0) {
        anchorPos = 0;
    }
    if (caret < 0) {
        caret = 0;
    }
    selections_ = {Selection{anchorPos, caret}};
    typingStyle_.reset();
    mergeArmed_ = false;
    preferredX_ = -1;
    caretDirty_ = true;
    caretMovedAt_ = Now();
    ensureCaretVisible();
}

void StyledTextArea::selectRange(int anchorParagraph, int anchorColumn, int caretParagraph, int caretColumn) {
    selectRange(content_.offset(anchorParagraph, anchorColumn), content_.offset(caretParagraph, caretColumn));
}

void StyledTextArea::moveTo(int offset) { selectRange(offset, offset); }

void StyledTextArea::moveTo(int paragraph, int column) { moveTo(content_.offset(paragraph, column)); }

void StyledTextArea::displaceCaret(int offset) {
    if (offset < 0) {
        offset = 0;
    }
    if (offset > content_.length()) {
        offset = content_.length();
    }
    primary().caret = offset;
    caretDirty_ = true;
    caretMovedAt_ = Now();
    ensureCaretVisible();
}

void StyledTextArea::selectAll() { selectRange(0, content_.length()); }

void StyledTextArea::deselect() { moveTo(caretPosition()); }

void StyledTextArea::addCaret(int offset) {
    if (offset < 0) {
        offset = 0;
    }
    if (offset > content_.length()) {
        offset = content_.length();
    }
    for (std::size_t i = 0; i < selections_.size(); ++i) {
        if (selections_[i].caret == offset && selections_[i].anchor == offset) {
            if (selections_.size() > 1) {
                selections_.erase(selections_.begin() + static_cast<std::ptrdiff_t>(i));
            }
            return;
        }
    }
    selections_.push_back(Selection{offset, offset});
    caretDirty_ = true;
    caretMovedAt_ = Now();
}

void StyledTextArea::clearSecondaryCarets() {
    if (selections_.empty()) {
        selections_.push_back(Selection{});
        return;
    }
    const Selection kept = primary();
    selections_ = {kept};
}

void StyledTextArea::setStyle(int start, int end, const TextStyle& style) {
    StyleSpansBuilder builder;
    if (end < start) {
        std::swap(start, end);
    }
    builder.add(style, end - start);
    setStyleSpans(start, builder.create(), true);
}

void StyledTextArea::setStyleSpans(int start, const StyleSpans& spans, bool undoable) {
    const DocumentChange change = content_.setStyleSpans(start, spans);
    markDirty();
    if (!undoable || undoSuspend_ > 0) {
        return;
    }
    if (change.removed == change.inserted) {
        return;
    }
    if (open_ != nullptr) {
        open_->changes.push_back(change);
        return;
    }
    transact(false, [&] {
        if (open_ != nullptr) {
            open_->changes.push_back(change);
        }
    });
}

void StyledTextArea::forgetHistory() {
    undo_.clear();
    redo_.clear();
    mergeArmed_ = false;
}

void StyledTextArea::suspendUndo() { ++undoSuspend_; }

void StyledTextArea::resumeUndo() {
    if (undoSuspend_ > 0) {
        --undoSuspend_;
    }
}

void StyledTextArea::setWrapText(bool wrap) {
    if (wrap_ == wrap) {
        return;
    }
    wrap_ = wrap;
    markDirty();
}

void StyledTextArea::setTabSize(int spaces) {
    tabSize_ = std::max(1, spaces);
    markDirty();
}

void StyledTextArea::setShowLineNumbers(bool show) {
    lineNumbers_ = show;
    markDirty();
}

bool StyledTextArea::isFolded(int paragraph) const { return isHidden(paragraph); }

void StyledTextArea::foldParagraphs(int startParagraph, int endParagraph) {
    if (endParagraph < startParagraph) {
        std::swap(startParagraph, endParagraph);
    }
    if (startParagraph < 0) {
        startParagraph = 0;
    }
    if (endParagraph >= content_.paragraphCount()) {
        endParagraph = content_.paragraphCount() - 1;
    }
    if (endParagraph <= startParagraph) {
        return;
    }
    std::vector<Fold> kept;
    for (const Fold& fold : folds_) {
        if (fold.end < startParagraph || fold.start > endParagraph) {
            kept.push_back(fold);
        }
    }
    kept.push_back(Fold{startParagraph, endParagraph});
    folds_ = std::move(kept);
    for (Selection& selection : selections_) {
        const TextPos pos = content_.position(selection.caret);
        if (isHidden(pos.paragraph)) {
            const int header = foldEnd(pos.paragraph) >= 0 ? startParagraph : pos.paragraph;
            const int column = content_.paragraph(header).length();
            selection.caret = selection.anchor = content_.offset(header, column);
        }
    }
    markDirty();
    caretDirty_ = true;
}

void StyledTextArea::unfoldParagraphs(int paragraph) {
    std::vector<Fold> kept;
    for (const Fold& fold : folds_) {
        if (paragraph < fold.start || paragraph > fold.end) {
            kept.push_back(fold);
        }
    }
    if (kept.size() == folds_.size()) {
        return;
    }
    folds_ = std::move(kept);
    markDirty();
}

void StyledTextArea::scrollTo(double x, double y) {
    scrollX_ = x;
    scrollY_ = y;
    rebuild();
}

void StyledTextArea::showPosition(int offset) {
    const int saved = primary().caret;
    primary().caret = std::max(0, std::min(content_.length(), offset));
    caretDirty_ = true;
    ensureCaretVisible();
    primary().caret = saved;
}

Font StyledTextArea::areaFont() const {
    const ComputedStyle& style = computedStyle();
    const float size = style.fontSize > 0.f ? style.fontSize : 16.f;
    const std::string& family = style.fontFamily.empty() ? std::string("Open Sans") : style.fontFamily;
    return Font(family, size);
}

TextStyle StyledTextArea::resolveStyle(const TextStyle& style) const { return style; }

TextStyle StyledTextArea::styleForInsertion(int offset) const {
    if (typingStyle_) {
        return *typingStyle_;
    }
    if (useInitialStyle_) {
        return initialStyle_;
    }
    if (offset < 0) {
        offset = 0;
    }
    if (offset > content_.length()) {
        offset = content_.length();
    }
    return content_.styleAt(offset);
}

ParagraphStyle StyledTextArea::paragraphStyleForInsertion(int offset) const {
    if (content_.paragraphCount() == 0) {
        return initialParagraph_;
    }
    return content_.paragraph(content_.position(std::max(0, offset)).paragraph).paragraphStyle();
}

void StyledTextArea::markDirty() { layoutDirty_ = true; }

void StyledTextArea::flushPending() {
    if (flushing_) {
        return;
    }
    flushing_ = true;
    for (int guard = 0; guard < 8 && (!pendingPlain_.empty() || !pendingRich_.empty()); ++guard) {
        const std::vector<PlainTextChange> plain = std::move(pendingPlain_);
        const std::vector<DocumentChange> rich = std::move(pendingRich_);
        pendingPlain_.clear();
        pendingRich_.clear();
        for (const PlainTextChange& change : plain) {
            if (onPlain_) {
                onPlain_(change);
            }
        }
        for (const DocumentChange& change : rich) {
            if (onRich_) {
                onRich_(change);
            }
        }
    }
    flushing_ = false;
}

void StyledTextArea::clampSelections() {
    const int length = content_.length();
    if (selections_.empty()) {
        selections_.push_back(Selection{});
    }
    for (Selection& selection : selections_) {
        selection.anchor = std::max(0, std::min(length, selection.anchor));
        selection.caret = std::max(0, std::min(length, selection.caret));
    }
}

DocumentChange StyledTextArea::editReplace(int start, int end, const StyledDocument& replacement) {
    if (start > end) {
        std::swap(start, end);
    }
    const int startParagraph = content_.position(start).paragraph;
    const int endParagraph = content_.position(end).paragraph;
    const int before = content_.paragraphCount();
    DocumentChange change = content_.replace(start, end, replacement);
    retargetFolds(startParagraph, endParagraph, content_.paragraphCount() - before);
    markDirty();
    if (open_ != nullptr && !(change.removed == change.inserted)) {
        open_->changes.push_back(change);
    }
    return change;
}

void StyledTextArea::retargetFolds(int startParagraph, int endParagraph, int delta) {
    std::vector<Fold> next;
    for (Fold fold : folds_) {
        if (fold.end < startParagraph) {
            next.push_back(fold);
            continue;
        }
        if (fold.start > endParagraph) {
            fold.start += delta;
            fold.end += delta;
            if (fold.end > fold.start && fold.start >= 0 && fold.end < content_.paragraphCount()) {
                next.push_back(fold);
            }
            continue;
        }
    }
    folds_ = std::move(next);
}

bool StyledTextArea::isHidden(int paragraph) const {
    for (const Fold& fold : folds_) {
        if (paragraph > fold.start && paragraph <= fold.end) {
            return true;
        }
    }
    return false;
}

bool StyledTextArea::isFoldHeader(int paragraph) const {
    for (const Fold& fold : folds_) {
        if (paragraph == fold.start) {
            return true;
        }
    }
    return false;
}

int StyledTextArea::foldEnd(int paragraph) const {
    for (const Fold& fold : folds_) {
        if (paragraph >= fold.start && paragraph <= fold.end) {
            return fold.end;
        }
    }
    return paragraph;
}

int StyledTextArea::step(int offset, int direction) const {
    if (direction < 0) {
        if (offset <= 0) {
            return 0;
        }
        const int previous = offset - 1;
        const int paragraph = content_.position(previous).paragraph;
        if (isHidden(paragraph)) {
            for (const Fold& fold : folds_) {
                if (paragraph >= fold.start && paragraph <= fold.end) {
                    return content_.offset(fold.start, content_.paragraph(fold.start).length());
                }
            }
        }
        return previous;
    }
    if (offset >= content_.length()) {
        return content_.length();
    }
    const int next = offset + 1;
    const int paragraph = content_.position(next).paragraph;
    if (isHidden(paragraph)) {
        for (const Fold& fold : folds_) {
            if (paragraph >= fold.start && paragraph <= fold.end) {
                if (fold.end + 1 < content_.paragraphCount()) {
                    return content_.offset(fold.end + 1, 0);
                }
                return content_.offset(fold.start, content_.paragraph(fold.start).length());
            }
        }
    }
    return next;
}

int StyledTextArea::wordBoundary(int offset, int direction) const {
    const int length = content_.length();
    char32_t codepoint = 0;
    if (direction > 0) {
        if (offset >= length) {
            return length;
        }
        if (CodePointAt(content_, offset, codepoint) && IsWord(codepoint)) {
            while (offset < length && CodePointAt(content_, offset, codepoint) && IsWord(codepoint)) {
                ++offset;
            }
            return offset;
        }
        while (offset < length && CodePointAt(content_, offset, codepoint) && !IsWord(codepoint)) {
            ++offset;
        }
        while (offset < length && CodePointAt(content_, offset, codepoint) && IsWord(codepoint)) {
            ++offset;
        }
        return offset;
    }
    if (offset <= 0) {
        return 0;
    }
    int index = offset - 1;
    while (index > 0 && CodePointAt(content_, index, codepoint) && !IsWord(codepoint)) {
        --index;
    }
    while (index > 0 && CodePointAt(content_, index - 1, codepoint) && IsWord(codepoint)) {
        --index;
    }
    return index;
}

void StyledTextArea::commit(UndoEntry entry) {
    entry.changes.erase(std::remove_if(entry.changes.begin(), entry.changes.end(),
                                       [](const DocumentChange& change) {
                                           if (change.isParagraphStyle()) {
                                               return change.oldParagraph == change.newParagraph;
                                           }
                                           return change.removed == change.inserted;
                                       }),
                        entry.changes.end());
    if (entry.changes.empty() || undoSuspend_ > 0) {
        return;
    }
    auto mergeable = [](const UndoEntry& previous, const UndoEntry& next) {
        if (!previous.coalesce || !next.coalesce || previous.changes.size() != 1 || next.changes.size() != 1) {
            return false;
        }
        const DocumentChange& first = previous.changes.front();
        const DocumentChange& second = next.changes.front();
        if (first.isParagraphStyle() || second.isParagraphStyle()) {
            return false;
        }
        const bool firstInsert =
            first.removed.length() == 0 && first.inserted.paragraphCount() == 1 && first.inserted.length() == 1;
        const bool secondInsert =
            second.removed.length() == 0 && second.inserted.paragraphCount() == 1 && second.inserted.length() == 1;
        if (firstInsert && secondInsert && second.position == first.position + first.inserted.length()) {
            return true;
        }
        const bool firstDelete = first.inserted.length() == 0 && first.removed.length() == 1;
        const bool secondDelete = second.inserted.length() == 0 && second.removed.length() == 1;
        return firstDelete && secondDelete && second.position + second.removed.length() == first.position;
    };
    redo_.clear();
    if (entry.coalesce && mergeArmed_ && !undo_.empty() && mergeable(undo_.back(), entry)) {
        DocumentChange& first = undo_.back().changes.front();
        const DocumentChange& second = entry.changes.front();
        if (second.removed.length() == 0) {
            first.inserted = StyledDocument::concat(first.inserted, second.inserted);
        } else {
            first.position = second.position;
            first.removed = StyledDocument::concat(second.removed, first.removed);
        }
        undo_.back().after = entry.after;
        mergeArmed_ = true;
        return;
    }
    mergeArmed_ = entry.coalesce;
    undo_.push_back(std::move(entry));
    if (undo_.size() > 200) {
        undo_.erase(undo_.begin());
    }
}

void StyledTextArea::transact(bool coalesce, const std::function<void()>& body) {
    if (open_ != nullptr) {
        body();
        return;
    }
    if (undoSuspend_ > 0) {
        body();
        flushPending();
        caretDirty_ = true;
        return;
    }
    UndoEntry entry;
    entry.before = selections_;
    entry.coalesce = coalesce;
    open_ = &entry;
    body();
    flushPending();
    open_ = nullptr;
    entry.after = selections_;
    commit(std::move(entry));
    caretDirty_ = true;
    caretMovedAt_ = Now();
}

void StyledTextArea::applyEditToCarets(const std::string& text, bool typing) {
    if (!editable_) {
        return;
    }
    clampSelections();
    std::vector<Selection> ordered = selections_;
    std::sort(ordered.begin(), ordered.end(), [](const Selection& a, const Selection& b) { return a.start() > b.start(); });
    transact(typing, [&] {
        std::vector<Selection> placed;
        for (const Selection& selection : ordered) {
            const int from = selection.start();
            const int to = selection.end();
            const DocumentChange change =
                editReplace(from, to, StyledDocument::fromPlain(text, styleForInsertion(from), paragraphStyleForInsertion(from)));
            const int delta = change.inserted.length() - (to - from);
            for (Selection& done : placed) {
                done.anchor += delta;
                done.caret += delta;
            }
            const int caret = from + change.inserted.length();
            placed.push_back(Selection{caret, caret});
        }
        if (!placed.empty()) {
            selections_ = std::move(placed);
        }
        clampSelections();
    });
    preferredX_ = -1;
}

void StyledTextArea::deleteRanges(bool forward, bool word) {
    if (!editable_) {
        return;
    }
    const bool single =
        selections_.size() == 1 && selections_.front().start() == selections_.front().end() && !word;
    std::vector<Selection> ordered = selections_;
    std::sort(ordered.begin(), ordered.end(), [](const Selection& a, const Selection& b) { return a.start() > b.start(); });
    transact(single, [&] {
        std::vector<Selection> placed;
        for (Selection selection : ordered) {
            int from = selection.start();
            int to = selection.end();
            if (from == to) {
                const int origin = selection.caret;
                if (forward) {
                    to = word ? wordBoundary(origin, 1) : step(origin, 1);
                    from = origin;
                } else {
                    from = word ? wordBoundary(origin, -1) : step(origin, -1);
                    to = origin;
                    if (!forward && insertSpaces_ && from == origin - 1) {
                        const TextPos pos = content_.position(origin);
                        const std::u32string& line = content_.paragraph(pos.paragraph).content();
                        int spaces = 0;
                        int column = pos.column;
                        while (spaces < tabSize_ && column - spaces - 1 >= 0 &&
                               line[static_cast<std::size_t>(column - spaces - 1)] == U' ') {
                            ++spaces;
                        }
                        if (spaces == tabSize_ || (spaces > 0 && column - spaces == 0)) {
                            from = content_.offset(pos.paragraph, column - spaces);
                        }
                    }
                }
            }
            if (from > to) {
                std::swap(from, to);
            }
            const DocumentChange change = editReplace(from, to, StyledDocument::fromPlain(""));
            const int delta = change.inserted.length() - (to - from);
            for (Selection& done : placed) {
                if (done.caret >= to) {
                    done.caret += delta;
                    done.anchor += delta;
                }
            }
            placed.push_back(Selection{from, from});
        }
        selections_ = std::move(placed);
        clampSelections();
    });
    preferredX_ = -1;
    typingStyle_.reset();
}

void StyledTextArea::breakParagraphs() {
    if (!editable_) {
        return;
    }
    std::vector<Selection> ordered = selections_;
    std::sort(ordered.begin(), ordered.end(), [](const Selection& a, const Selection& b) { return a.start() > b.start(); });
    transact(false, [&] {
        std::vector<Selection> placed;
        for (const Selection& selection : ordered) {
            const int from = selection.start();
            const int to = selection.end();
            std::string indent;
            if (autoIndent_ && from == to) {
                const TextPos pos = content_.position(from);
                const std::u32string& line = content_.paragraph(pos.paragraph).content();
                std::size_t count = 0;
                while (count < line.size() && (line[count] == U' ' || line[count] == U'\t')) {
                    ++count;
                }
                indent = Utf8(line.substr(0, count));
            }
            const std::string inserted = std::string("\n") + indent;
            const DocumentChange change = editReplace(
                from, to, StyledDocument::fromPlain(inserted, styleForInsertion(from), paragraphStyleForInsertion(from)));
            const int delta = change.inserted.length() - (to - from);
            for (Selection& done : placed) {
                done.anchor += delta;
                done.caret += delta;
            }
            const int caret = from + change.inserted.length();
            placed.push_back(Selection{caret, caret});
        }
        selections_ = std::move(placed);
        clampSelections();
    });
    preferredX_ = -1;
    typingStyle_.reset();
}

void StyledTextArea::indentLines(bool outdent) {
    if (!editable_) {
        return;
    }
    std::vector<int> paragraphs;
    for (const Selection& selection : selections_) {
        const int from = content_.position(selection.start()).paragraph;
        int to = from;
        if (selection.end() > selection.start()) {
            to = content_.position(selection.end() - 1).paragraph;
        }
        for (int paragraph = from; paragraph <= to; ++paragraph) {
            paragraphs.push_back(paragraph);
        }
    }
    std::sort(paragraphs.begin(), paragraphs.end());
    paragraphs.erase(std::unique(paragraphs.begin(), paragraphs.end()), paragraphs.end());
    std::vector<Selection> marks = selections_;
    transact(false, [&] {
        for (auto it = paragraphs.rbegin(); it != paragraphs.rend(); ++it) {
            const int paragraph = *it;
            const int at = content_.offset(paragraph, 0);
            const std::u32string& line = content_.paragraph(paragraph).content();
            int remove = 0;
            std::string inserted;
            if (outdent) {
                if (!line.empty() && line[0] == U'\t') {
                    remove = 1;
                } else {
                    while (remove < tabSize_ && remove < static_cast<int>(line.size()) &&
                           line[static_cast<std::size_t>(remove)] == U' ') {
                        ++remove;
                    }
                }
            } else if (insertSpaces_) {
                inserted.assign(static_cast<std::size_t>(tabSize_), ' ');
            } else {
                inserted = "\t";
            }
            if (remove == 0 && inserted.empty()) {
                continue;
            }
            const DocumentChange change =
                editReplace(at, at + remove, StyledDocument::fromPlain(inserted, styleForInsertion(at),
                                                                       content_.paragraph(paragraph).paragraphStyle()));
            const int delta = change.inserted.length() - remove;
            for (Selection& mark : marks) {
                if (mark.caret >= at + remove) {
                    mark.caret += delta;
                } else if (mark.caret > at) {
                    mark.caret = at;
                }
                if (mark.anchor >= at + remove) {
                    mark.anchor += delta;
                } else if (mark.anchor > at) {
                    mark.anchor = at;
                }
            }
        }
        selections_ = marks;
        clampSelections();
    });
}

void StyledTextArea::toggleStyle(bool underline) {
    if (!editable_) {
        return;
    }
    bool enable = false;
    for (const Selection& selection : selections_) {
        if (selection.start() == selection.end()) {
            const TextStyle current = resolveStyle(styleForInsertion(selection.caret));
            if (underline ? !current.underline : !current.bold) {
                enable = true;
            }
            continue;
        }
        const StyleSpans spans = content_.styleSpans(selection.start(), selection.end());
        for (const StyleSpan& span : spans.spans()) {
            const TextStyle resolved = resolveStyle(span.style);
            if (underline ? !resolved.underline : !resolved.bold) {
                enable = true;
            }
        }
    }
    const bool anyRange = std::any_of(selections_.begin(), selections_.end(),
                                      [](const Selection& selection) { return selection.start() != selection.end(); });
    if (!anyRange) {
        TextStyle style = styleForInsertion(primary().caret);
        if (underline) {
            style.underline = enable;
        } else {
            style.bold = enable;
        }
        typingStyle_ = style;
        return;
    }
    std::vector<Selection> ordered = selections_;
    std::sort(ordered.begin(), ordered.end(), [](const Selection& a, const Selection& b) { return a.start() > b.start(); });
    transact(false, [&] {
        for (const Selection& selection : ordered) {
            if (selection.start() == selection.end()) {
                continue;
            }
            const StyleSpans spans = content_.styleSpans(selection.start(), selection.end());
            StyleSpansBuilder builder;
            for (const StyleSpan& span : spans.spans()) {
                TextStyle style = span.style;
                if (underline) {
                    style.underline = enable;
                } else {
                    style.bold = enable;
                }
                builder.add(style, span.length);
            }
            setStyleSpans(selection.start(), builder.create(), true);
        }
    });
}

void StyledTextArea::undo() {
    if (undo_.empty()) {
        return;
    }
    UndoEntry entry = std::move(undo_.back());
    undo_.pop_back();
    ++undoSuspend_;
    for (auto it = entry.changes.rbegin(); it != entry.changes.rend(); ++it) {
        if (it->isParagraphStyle()) {
            content_.setParagraphStyle(it->paragraphIndex, it->oldParagraph);
            markDirty();
        } else {
            editReplace(it->position, it->position + it->inserted.length(), it->removed);
        }
    }
    selections_ = entry.before;
    clampSelections();
    redo_.push_back(std::move(entry));
    --undoSuspend_;
    mergeArmed_ = false;
    typingStyle_.reset();
    preferredX_ = -1;
    caretDirty_ = true;
    caretMovedAt_ = Now();
    flushPending();
    ensureCaretVisible();
}

void StyledTextArea::redo() {
    if (redo_.empty()) {
        return;
    }
    UndoEntry entry = std::move(redo_.back());
    redo_.pop_back();
    ++undoSuspend_;
    for (const DocumentChange& change : entry.changes) {
        if (change.isParagraphStyle()) {
            content_.setParagraphStyle(change.paragraphIndex, change.newParagraph);
            markDirty();
        } else {
            editReplace(change.position, change.position + change.removed.length(), change.inserted);
        }
    }
    selections_ = entry.after;
    clampSelections();
    undo_.push_back(std::move(entry));
    --undoSuspend_;
    mergeArmed_ = false;
    typingStyle_.reset();
    preferredX_ = -1;
    caretDirty_ = true;
    caretMovedAt_ = Now();
    flushPending();
    ensureCaretVisible();
}

void StyledTextArea::copy() {
    std::vector<Selection> ordered = selections_;
    std::sort(ordered.begin(), ordered.end(), [](const Selection& a, const Selection& b) { return a.start() < b.start(); });
    std::string plain;
    StyledDocument rich;
    bool any = false;
    for (const Selection& selection : ordered) {
        if (selection.start() == selection.end()) {
            continue;
        }
        const StyledDocument piece = content_.subDocument(selection.start(), selection.end());
        if (any) {
            plain.push_back('\n');
            rich = StyledDocument::concat(rich, StyledDocument::concat(StyledDocument::fromPlain("\n"), piece));
        } else {
            rich = piece;
        }
        plain += piece.text();
        any = true;
    }
    if (!any) {
        return;
    }
    Clipboard().plain = plain;
    Clipboard().document = std::move(rich);
    Clipboard().styled = true;
    if (Scene* scene = getScene()) {
        scene->setClipboardText(plain);
    }
}

void StyledTextArea::cut() {
    copy();
    if (editable_) {
        deleteRanges(true, false);
    }
}

void StyledTextArea::paste() {
    if (!editable_) {
        return;
    }
    std::string plain = Clipboard().plain;
    if (Scene* scene = getScene()) {
        const std::string live = scene->clipboardText();
        if (!live.empty()) {
            plain = live;
        }
    }
    if (plain.empty()) {
        return;
    }
    const bool styled = Clipboard().styled && plain == Clipboard().plain;
    std::vector<Selection> ordered = selections_;
    std::sort(ordered.begin(), ordered.end(), [](const Selection& a, const Selection& b) { return a.start() > b.start(); });
    transact(false, [&] {
        std::vector<Selection> placed;
        for (const Selection& selection : ordered) {
            const int from = selection.start();
            const int to = selection.end();
            DocumentChange change;
            if (styled) {
                change = editReplace(from, to, Clipboard().document);
            } else {
                change = editReplace(from, to, StyledDocument::fromPlain(plain, styleForInsertion(from),
                                                                         paragraphStyleForInsertion(from)));
            }
            const int delta = change.inserted.length() - (to - from);
            for (Selection& done : placed) {
                done.anchor += delta;
                done.caret += delta;
            }
            const int caret = from + change.inserted.length();
            placed.push_back(Selection{caret, caret});
        }
        selections_ = std::move(placed);
        clampSelections();
    });
    typingStyle_.reset();
    preferredX_ = -1;
}

void StyledTextArea::collapseCarets(int offset) { selections_ = {Selection{offset, offset}}; }

void StyledTextArea::moveCarets(int direction, bool select, bool word) {
    rebuild();
    for (Selection& selection : selections_) {
        if (!select && selection.start() != selection.end() && !word) {
            const int edge = direction < 0 ? selection.start() : selection.end();
            selection.anchor = edge;
            selection.caret = edge;
            continue;
        }
        const int next = word ? wordBoundary(selection.caret, direction) : step(selection.caret, direction);
        if (!select) {
            selection.anchor = next;
        }
        selection.caret = next;
    }
    clampSelections();
    mergeArmed_ = false;
    typingStyle_.reset();
    preferredX_ = -1;
    caretDirty_ = true;
    caretMovedAt_ = Now();
    ensureCaretVisible();
}

void StyledTextArea::moveLineEdge(bool end, bool select) {
    rebuild();
    for (Selection& selection : selections_) {
        const TextPos pos = content_.position(selection.caret);
        const ParagraphLayout& layout =
            pos.paragraph < static_cast<int>(view_.paragraphs.size()) ? view_.paragraphs[static_cast<std::size_t>(pos.paragraph)]
                                                                       : ParagraphLayout{};
        int column = end ? content_.paragraph(pos.paragraph).length() : 0;
        for (std::size_t i = 0; i < layout.lines.size(); ++i) {
            const LineLayout& line = layout.lines[i];
            const bool last = i + 1 == layout.lines.size();
            const bool inside = pos.column >= line.start && (pos.column < line.end || (last && pos.column <= line.end));
            if (!inside) {
                continue;
            }
            if (end) {
                column = line.end;
                break;
            }
            int nonSpace = line.start;
            const std::u32string& text = content_.paragraph(pos.paragraph).content();
            while (nonSpace < line.end && nonSpace < static_cast<int>(text.size()) &&
                   (text[static_cast<std::size_t>(nonSpace)] == U' ' || text[static_cast<std::size_t>(nonSpace)] == U'\t')) {
                ++nonSpace;
            }
            column = pos.column == nonSpace ? line.start : nonSpace;
            break;
        }
        const int offset = content_.offset(pos.paragraph, column);
        if (!select) {
            selection.anchor = offset;
        }
        selection.caret = offset;
    }
    mergeArmed_ = false;
    typingStyle_.reset();
    preferredX_ = -1;
    caretDirty_ = true;
    caretMovedAt_ = Now();
    ensureCaretVisible();
}

void StyledTextArea::moveToDocument(bool end, bool select) {
    const int offset = end ? content_.length() : 0;
    for (Selection& selection : selections_) {
        if (!select) {
            selection.anchor = offset;
        }
        selection.caret = offset;
    }
    mergeArmed_ = false;
    typingStyle_.reset();
    preferredX_ = -1;
    caretDirty_ = true;
    caretMovedAt_ = Now();
    ensureCaretVisible();
}

void StyledTextArea::moveVertical(int lines, bool select) {
    rebuild();
    struct Visual {
        int paragraph = 0;
        int line = 0;
        float top = 0.f;
    };
    std::vector<Visual> visual;
    for (int paragraph = 0; paragraph < content_.paragraphCount() && paragraph < static_cast<int>(view_.tops.size());
         ++paragraph) {
        if (isHidden(paragraph)) {
            continue;
        }
        const ParagraphLayout& layout = view_.paragraphs[static_cast<std::size_t>(paragraph)];
        for (int line = 0; line < static_cast<int>(layout.lines.size()); ++line) {
            visual.push_back(Visual{paragraph, line, view_.tops[static_cast<std::size_t>(paragraph)] + layout.lines[static_cast<std::size_t>(line)].y});
        }
    }
    if (visual.empty()) {
        return;
    }
    const Font font = areaFont();
    for (Selection& selection : selections_) {
        const TextPos pos = content_.position(selection.caret);
        int found = 0;
        for (int i = 0; i < static_cast<int>(visual.size()); ++i) {
            const Visual& line = visual[static_cast<std::size_t>(i)];
            if (line.paragraph != pos.paragraph) {
                continue;
            }
            const LineLayout& layout = view_.paragraphs[static_cast<std::size_t>(line.paragraph)].lines[static_cast<std::size_t>(line.line)];
            const bool last = line.line + 1 == static_cast<int>(view_.paragraphs[static_cast<std::size_t>(line.paragraph)].lines.size());
            if (pos.column >= layout.start && (pos.column < layout.end || (last && pos.column <= layout.end))) {
                found = i;
            }
        }
        if (preferredX_ < 0) {
            const Visual& line = visual[static_cast<std::size_t>(found)];
            const LineLayout& layout = view_.paragraphs[static_cast<std::size_t>(line.paragraph)].lines[static_cast<std::size_t>(line.line)];
            const MeasuredLine measured =
                MeasureRange(content_.paragraph(line.paragraph).content(), layout.start, layout.end, font.family(), font.size(),
                             tabSize_, [&](int column) { return resolveStyle(SpanStyle(content_.paragraph(line.paragraph), column)); });
            const int local = std::max(0, std::min(static_cast<int>(measured.caret.size()) - 1, pos.column - layout.start));
            preferredX_ = measured.caret[static_cast<std::size_t>(local)];
        }
        int target = found + lines;
        if (target < 0) {
            target = 0;
        }
        if (target >= static_cast<int>(visual.size())) {
            target = static_cast<int>(visual.size()) - 1;
        }
        const Visual& line = visual[static_cast<std::size_t>(target)];
        const LineLayout& layout = view_.paragraphs[static_cast<std::size_t>(line.paragraph)].lines[static_cast<std::size_t>(line.line)];
        const MeasuredLine measured =
            MeasureRange(content_.paragraph(line.paragraph).content(), layout.start, layout.end, font.family(), font.size(), tabSize_,
                         [&](int column) { return resolveStyle(SpanStyle(content_.paragraph(line.paragraph), column)); });
        int best = 0;
        float bestDistance = 1.0e9f;
        for (int i = 0; i < static_cast<int>(measured.caret.size()); ++i) {
            const float distance = std::fabs(measured.caret[static_cast<std::size_t>(i)] - static_cast<float>(preferredX_));
            if (distance < bestDistance) {
                bestDistance = distance;
                best = i;
            }
        }
        const int offset = content_.offset(line.paragraph, layout.start + best);
        if (!select) {
            selection.anchor = offset;
        }
        selection.caret = offset;
    }
    mergeArmed_ = false;
    typingStyle_.reset();
    caretDirty_ = true;
    caretMovedAt_ = Now();
    ensureCaretVisible();
}

void StyledTextArea::page(bool down, bool select) {
    rebuild();
    const float line = std::max(1.f, areaFont().lineHeight());
    int count = static_cast<int>(view_.textH / line);
    if (count < 1) {
        count = 1;
    }
    moveVertical(down ? count : -count, select);
}

void StyledTextArea::ensureCaretVisible() {
    if (!followCaret_) {
        return;
    }
    rebuild();
    if (view_.paragraphs.empty() || view_.textH <= 0.f) {
        return;
    }
    int offset = primary().caret;
    TextPos pos = content_.position(offset);
    if (isHidden(pos.paragraph)) {
        for (const Fold& fold : folds_) {
            if (pos.paragraph > fold.start && pos.paragraph <= fold.end) {
                offset = content_.offset(fold.start, content_.paragraph(fold.start).length());
                pos = content_.position(offset);
                break;
            }
        }
    }
    if (pos.paragraph < 0 || pos.paragraph >= static_cast<int>(view_.paragraphs.size())) {
        return;
    }
    const ParagraphLayout& layout = view_.paragraphs[static_cast<std::size_t>(pos.paragraph)];
    if (layout.lines.empty()) {
        return;
    }
    const LineLayout* line = &layout.lines.front();
    for (std::size_t i = 0; i < layout.lines.size(); ++i) {
        const LineLayout& candidate = layout.lines[i];
        const bool last = i + 1 == layout.lines.size();
        if (pos.column >= candidate.start && (pos.column < candidate.end || (last && pos.column <= candidate.end))) {
            line = &candidate;
        }
    }
    const float top = view_.tops[static_cast<std::size_t>(pos.paragraph)] + line->y;
    if (top < scrollY_) {
        scrollY_ = top;
    }
    if (top + line->height > scrollY_ + view_.textH) {
        scrollY_ = top + line->height - view_.textH;
    }
    const Font font = areaFont();
    const MeasuredLine measured =
        MeasureRange(content_.paragraph(pos.paragraph).content(), line->start, line->end, font.family(), font.size(), tabSize_,
                     [&](int column) { return resolveStyle(SpanStyle(content_.paragraph(pos.paragraph), column)); });
    const int local = std::max(0, std::min(static_cast<int>(measured.caret.size()) - 1, pos.column - line->start));
    const float x = measured.caret[static_cast<std::size_t>(local)];
    if (!wrap_) {
        if (x < scrollX_) {
            scrollX_ = x;
        }
        if (x + 2.f > scrollX_ + view_.textW) {
            scrollX_ = x + 2.f - view_.textW;
        }
    }
    const float maxY = std::max(0.f, view_.contentHeight - view_.textH);
    const float maxX = wrap_ ? 0.f : std::max(0.f, view_.contentWidth - view_.textW);
    scrollY_ = std::max(0.0, std::min(static_cast<double>(maxY), scrollY_));
    scrollX_ = std::max(0.0, std::min(static_cast<double>(maxX), scrollX_));
}

void StyledTextArea::rebuild() const {
    const Font font = areaFont();
    const float contentW = static_cast<float>(contentWidth());
    const float contentH = static_cast<float>(contentHeight());
    const bool fontChanged = view_.fontSize != font.size() || view_.fontFamily != font.family();
    if (!layoutDirty_ && !fontChanged && std::fabs(view_.textW) >= 0.f && contentW > 0.f) {
        // Still rebuild when the viewport width changed. textW is compared below.
    }
    float gutter = 0.f;
    if (lineNumbers_ || !folds_.empty()) {
        int digits = 1;
        int count = std::max(1, content_.paragraphCount());
        while (count >= 10) {
            ++digits;
            count /= 10;
        }
        const float digit = std::max(1.f, font.measureWidth("0"));
        gutter = 16.f + (lineNumbers_ ? digit * static_cast<float>(digits) + 10.f : 0.f);
    }
    const float bar = ScrollBar::kThickness;
    bool vertical = view_.verticalBar;
    bool horizontal = view_.horizontalBar;
    std::vector<ParagraphLayout> layouts = view_.paragraphs;
    if (static_cast<int>(layouts.size()) != content_.paragraphCount()) {
        layouts.assign(static_cast<std::size_t>(std::max(0, content_.paragraphCount())), ParagraphLayout{});
    }
    auto buildParagraph = [&](int index, float wrapWidth) {
        ParagraphLayout& layout = layouts[static_cast<std::size_t>(index)];
        const Paragraph& paragraph = content_.paragraph(index);
        const bool same = layout.revision == paragraph.revision() && std::fabs(layout.wrapWidth - wrapWidth) < 0.5f &&
                          layout.wrapped == wrap_ && std::fabs(layout.fontSize - font.size()) < 0.1f &&
                          layout.fontFamily == font.family() && layout.tabSize == tabSize_;
        if (same) {
            return;
        }
        layout = ParagraphLayout{};
        layout.revision = paragraph.revision();
        layout.wrapWidth = wrapWidth;
        layout.wrapped = wrap_;
        layout.fontSize = font.size();
        layout.fontFamily = font.family();
        layout.tabSize = tabSize_;
        const std::u32string& text = paragraph.content();
        const int length = static_cast<int>(text.size());
        const float limit = wrap_ ? std::max(8.f, wrapWidth) : 1.0e9f;
        std::vector<std::pair<int, int>> ranges;
        if (length == 0) {
            ranges.emplace_back(0, 0);
        } else {
            std::unordered_map<char32_t, float> cache;
            int lineStart = 0;
            int index = 0;
            float pen = 0.f;
            int breakAt = -1;
            while (index < length) {
                const TextStyle style = resolveStyle(SpanStyle(paragraph, index));
                const Font face = FontFor(font.family(), font.size(), style);
                const float advance = AdvanceOf(text[static_cast<std::size_t>(index)], pen, face, tabSize_, cache);
                if (wrap_ && index > lineStart && pen + advance > limit) {
                    const int cut = breakAt > lineStart ? breakAt : index;
                    ranges.emplace_back(lineStart, cut);
                    index = cut;
                    lineStart = cut;
                    pen = 0.f;
                    breakAt = -1;
                    cache.clear();
                    continue;
                }
                pen += advance;
                ++index;
                const char32_t previous = text[static_cast<std::size_t>(index - 1)];
                if (previous == U' ' || previous == U'\t') {
                    breakAt = index;
                }
            }
            ranges.emplace_back(lineStart, length);
        }
        float y = 0.f;
        for (const std::pair<int, int>& range : ranges) {
            const MeasuredLine measured =
                MeasureRange(text, range.first, range.second, font.family(), font.size(), tabSize_,
                             [&](int column) { return resolveStyle(SpanStyle(paragraph, column)); });
            LineLayout line;
            line.start = range.first;
            line.end = range.second;
            line.y = y;
            line.height = std::max(1.f, measured.height);
            line.width = measured.width;
            line.ascent = measured.ascent;
            y += line.height;
            layout.width = std::max(layout.width, line.width);
            layout.lines.push_back(line);
        }
        layout.height = std::max(y, font.lineHeight());
    };

    float textW = 8.f;
    float textH = 8.f;
    for (int pass = 0; pass < 3; ++pass) {
        textW = std::max(8.f, contentW - gutter - (vertical ? bar : 0.f));
        textH = std::max(8.f, contentH - (horizontal ? bar : 0.f));
        for (int i = 0; i < content_.paragraphCount(); ++i) {
            buildParagraph(i, textW);
        }
        float contentHeight = 0.f;
        float contentWidth = 0.f;
        for (int i = 0; i < content_.paragraphCount(); ++i) {
            if (!isHidden(i)) {
                contentHeight += layouts[static_cast<std::size_t>(i)].height;
                contentWidth = std::max(contentWidth, layouts[static_cast<std::size_t>(i)].width);
            }
        }
        const bool needVertical = contentHeight > textH + 0.5f;
        const bool needHorizontal = !wrap_ && contentWidth > textW + 0.5f;
        if (needVertical == vertical && needHorizontal == horizontal) {
            break;
        }
        vertical = needVertical;
        horizontal = needHorizontal;
    }

    view_.gutter = gutter;
    view_.bar = bar;
    view_.verticalBar = vertical;
    view_.horizontalBar = horizontal;
    view_.textX = static_cast<float>(contentLeft()) + gutter;
    view_.textY = static_cast<float>(contentTop());
    view_.textW = textW;
    view_.textH = textH;
    view_.fontSize = font.size();
    view_.fontFamily = font.family();
    view_.paragraphs = std::move(layouts);
    view_.tops.assign(static_cast<std::size_t>(content_.paragraphCount() + 1), 0.f);
    float y = 0.f;
    float contentWidth = 0.f;
    for (int i = 0; i < content_.paragraphCount(); ++i) {
        view_.tops[static_cast<std::size_t>(i)] = y;
        if (!isHidden(i)) {
            y += view_.paragraphs[static_cast<std::size_t>(i)].height;
            contentWidth = std::max(contentWidth, view_.paragraphs[static_cast<std::size_t>(i)].width);
        }
    }
    view_.tops[static_cast<std::size_t>(content_.paragraphCount())] = y;
    view_.contentHeight = y;
    view_.contentWidth = contentWidth;
    const float maxY = std::max(0.f, view_.contentHeight - view_.textH);
    const float maxX = wrap_ ? 0.f : std::max(0.f, view_.contentWidth - view_.textW);
    scrollY_ = std::max(0.0, std::min(static_cast<double>(maxY), scrollY_));
    scrollX_ = std::max(0.0, std::min(static_cast<double>(maxX), scrollX_));
    if (view_.verticalBar && view_.contentHeight > 0.f) {
        verticalScroll_ = ScrollBar::vertical(static_cast<float>(contentLeft()) + contentW - bar,
                                               static_cast<float>(contentTop()), view_.textH, view_.contentHeight,
                                               view_.textH, scrollY_);
    } else {
        verticalScroll_ = {};
    }
    if (view_.horizontalBar && view_.contentWidth > 0.f) {
        horizontalScroll_ = ScrollBar::horizontal(view_.textX, static_cast<float>(contentTop()) + contentH - bar,
                                                   view_.textW, view_.contentWidth, view_.textW, scrollX_);
    } else {
        horizontalScroll_ = {};
    }
    layoutDirty_ = false;
}

CharacterHit StyledTextArea::hit(double x, double y) const {
    rebuild();
    CharacterHit result;
    if (view_.paragraphs.empty()) {
        return result;
    }
    const float localX = static_cast<float>(x - getAbsoluteX());
    const float localY = static_cast<float>(y - getAbsoluteY());
    float contentY = localY - view_.textY + static_cast<float>(scrollY_);
    float contentX = localX - view_.textX + static_cast<float>(scrollX_);
    if (contentY < 0.f) {
        contentY = 0.f;
    }
    int paragraph = 0;
    if (view_.tops.size() >= 2) {
        int low = 0;
        int high = static_cast<int>(view_.tops.size()) - 2;
        while (low < high) {
            const int mid = (low + high + 1) / 2;
            if (view_.tops[static_cast<std::size_t>(mid)] <= contentY) {
                low = mid;
            } else {
                high = mid - 1;
            }
        }
        paragraph = low;
    }
    while (paragraph + 1 < content_.paragraphCount() && isHidden(paragraph)) {
        ++paragraph;
    }
    if (paragraph < 0 || paragraph >= static_cast<int>(view_.paragraphs.size())) {
        return result;
    }
    const ParagraphLayout& layout = view_.paragraphs[static_cast<std::size_t>(paragraph)];
    if (layout.lines.empty()) {
        return result;
    }
    const float local = contentY - view_.tops[static_cast<std::size_t>(paragraph)];
    int lineIndex = 0;
    for (int i = 0; i < static_cast<int>(layout.lines.size()); ++i) {
        const LineLayout& line = layout.lines[static_cast<std::size_t>(i)];
        if (local < line.y + line.height || i + 1 == static_cast<int>(layout.lines.size())) {
            lineIndex = i;
            break;
        }
    }
    const LineLayout& line = layout.lines[static_cast<std::size_t>(lineIndex)];
    const Font font = areaFont();
    const MeasuredLine measured =
        MeasureRange(content_.paragraph(paragraph).content(), line.start, line.end, font.family(), font.size(), tabSize_,
                     [&](int column) { return resolveStyle(SpanStyle(content_.paragraph(paragraph), column)); });
    int best = 0;
    float bestDistance = 1.0e9f;
    for (int i = 0; i < static_cast<int>(measured.caret.size()); ++i) {
        const float distance = std::fabs(measured.caret[static_cast<std::size_t>(i)] - contentX);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    bool leading = true;
    if (best + 1 < static_cast<int>(measured.caret.size())) {
        const float left = measured.caret[static_cast<std::size_t>(best)];
        const float right = measured.caret[static_cast<std::size_t>(best + 1)];
        if (contentX > (left + right) * 0.5f && best < line.end - line.start) {
            // The nearest caret is still `best`; leading says which side of a glyph we hit.
        }
        if (best < static_cast<int>(measured.caret.size()) - 1) {
            const float mid = (measured.caret[static_cast<std::size_t>(best)] +
                               (best > 0 ? measured.caret[static_cast<std::size_t>(best - 1)] : 0.f)) *
                              0.5f;
            (void)mid;
        }
    }
    if (best > 0) {
        const float previous = measured.caret[static_cast<std::size_t>(best - 1)];
        const float here = measured.caret[static_cast<std::size_t>(best)];
        leading = contentX < (previous + here) * 0.5f ? false : true;
    }
    const int column = line.start + best;
    result.valid = true;
    result.paragraph = paragraph;
    result.column = column;
    result.insertionIndex = content_.offset(paragraph, column);
    result.leading = leading;
    if (column > line.start) {
        result.characterIndex = content_.offset(paragraph, column - (leading ? 0 : 1));
        if (leading && column < line.end) {
            result.characterIndex = content_.offset(paragraph, column);
        }
        if (!leading) {
            result.characterIndex = content_.offset(paragraph, column - 1);
        }
    } else if (column < line.end) {
        result.characterIndex = content_.offset(paragraph, column);
    } else {
        result.characterIndex = -1;
    }
    return result;
}

TextBounds StyledTextArea::caretBounds() const {
    rebuild();
    TextBounds bounds;
    const TextPos pos = content_.position(primary().caret);
    if (pos.paragraph < 0 || pos.paragraph >= static_cast<int>(view_.paragraphs.size())) {
        return bounds;
    }
    const ParagraphLayout& layout = view_.paragraphs[static_cast<std::size_t>(pos.paragraph)];
    if (layout.lines.empty() || pos.paragraph >= static_cast<int>(view_.tops.size())) {
        return bounds;
    }
    const LineLayout* line = &layout.lines.front();
    for (std::size_t i = 0; i < layout.lines.size(); ++i) {
        const LineLayout& candidate = layout.lines[i];
        const bool last = i + 1 == layout.lines.size();
        if (pos.column >= candidate.start && (pos.column < candidate.end || (last && pos.column <= candidate.end))) {
            line = &candidate;
        }
    }
    const Font font = areaFont();
    const MeasuredLine measured =
        MeasureRange(content_.paragraph(pos.paragraph).content(), line->start, line->end, font.family(), font.size(), tabSize_,
                     [&](int column) { return resolveStyle(SpanStyle(content_.paragraph(pos.paragraph), column)); });
    const int local = std::max(0, std::min(static_cast<int>(measured.caret.size()) - 1, pos.column - line->start));
    const float x = measured.caret[static_cast<std::size_t>(local)];
    const float top = view_.tops[static_cast<std::size_t>(pos.paragraph)] + line->y;
    bounds.x = getAbsoluteX() + view_.textX + x - scrollX_;
    bounds.y = getAbsoluteY() + view_.textY + top - scrollY_;
    bounds.width = 1;
    bounds.height = line->height;
    bounds.valid = bounds.y + bounds.height > getAbsoluteY() + view_.textY &&
                   bounds.y < getAbsoluteY() + view_.textY + view_.textH;
    return bounds;
}

int StyledTextArea::visualLineCount() const {
    rebuild();
    int count = 0;
    for (int i = 0; i < content_.paragraphCount() && i < static_cast<int>(view_.paragraphs.size()); ++i) {
        if (!isHidden(i)) {
            count += static_cast<int>(view_.paragraphs[static_cast<std::size_t>(i)].lines.size());
        }
    }
    return count;
}

double StyledTextArea::preferredContentWidth(double) const {
    const Font font = areaFont();
    return std::max(160.0, static_cast<double>(font.measureWidth("0000000000000000000000000000000000000000")));
}

double StyledTextArea::preferredContentHeight(double) const { return std::max(1.f, areaFont().lineHeight()) * 10.0; }

void StyledTextArea::layoutChildren() {
    rebuild();
    if (caretDirty_) {
        ensureCaretVisible();
        caretDirty_ = false;
    }
    if (onHover_ && hoverIndex_ >= 0 && !hoverFired_ && hoverDelayMs_ >= 0) {
        if ((Now() - hoverSince_) * 1000.0 >= hoverDelayMs_) {
            hoverFired_ = true;
            onHover_(hoverIndex_);
        }
    }
}

Cursor StyledTextArea::cursorAt(double x, double y) const {
    const Cursor base = Node::cursorAt(x, y);
    if (base != Cursor::Text) {
        return base;
    }
    const float localX = static_cast<float>(x - getAbsoluteX());
    const float localY = static_cast<float>(y - getAbsoluteY());
    if (verticalScroll_.part(localX, localY) != ScrollBar::Part::None ||
        horizontalScroll_.part(localX, localY) != ScrollBar::Part::None) {
        return Cursor::Default;
    }
    return base;
}

void StyledTextArea::handleMousePressed(const MouseEvent& event) {
    rebuild();
    const float localX = static_cast<float>(event.x - getAbsoluteX());
    const float localY = static_cast<float>(event.y - getAbsoluteY());
    const float left = static_cast<float>(contentLeft());
    const float top = static_cast<float>(contentTop());
    const float boxH = static_cast<float>(contentHeight());
    const bool shift = getScene() != nullptr && (getScene()->modifierMask() & Key::ModShift) != 0;
    const bool alt = getScene() != nullptr && (getScene()->modifierMask() & Key::ModAlt) != 0;
    const ScrollBar::Part verticalPart = verticalScroll_.part(localX, localY);
    if (verticalPart != ScrollBar::Part::None) {
        drag_ = Drag::VerticalBar;
        if (verticalPart == ScrollBar::Part::Thumb) {
            scrollGrab_ = localY - verticalScroll_.thumb;
        } else {
            scrollY_ = verticalScroll_.offsetFromPage(scrollY_, verticalPart == ScrollBar::Part::After);
            rebuild();
            scrollGrab_ = verticalScroll_.thumbLength * 0.5f;
        }
        return;
    }
    const ScrollBar::Part horizontalPart = horizontalScroll_.part(localX, localY);
    if (horizontalPart != ScrollBar::Part::None) {
        drag_ = Drag::HorizontalBar;
        if (horizontalPart == ScrollBar::Part::Thumb) {
            scrollGrab_ = localX - horizontalScroll_.thumb;
        } else {
            scrollX_ = horizontalScroll_.offsetFromPage(scrollX_, horizontalPart == ScrollBar::Part::After);
            rebuild();
            scrollGrab_ = horizontalScroll_.thumbLength * 0.5f;
        }
        return;
    }
    if (localX >= left && localX < left + view_.gutter && localY >= top && localY < top + boxH) {
        const CharacterHit where = hit(event.x, event.y);
        if (where.valid) {
            if (localX < left + 14.f && isFoldHeader(where.paragraph)) {
                unfoldParagraphs(where.paragraph);
            } else if (localX < left + 14.f && where.paragraph + 1 < content_.paragraphCount() && !isHidden(where.paragraph)) {
                foldParagraphs(where.paragraph, where.paragraph + 1);
            } else {
                const int start = content_.offset(where.paragraph, 0);
                const int end = where.paragraph + 1 < content_.paragraphCount() ? content_.offset(where.paragraph + 1, 0)
                                                                                : content_.length();
                selectRange(start, end);
            }
        }
        drag_ = Drag::None;
        return;
    }
    const double now = Now();
    const double dx = event.x - lastPressX_;
    const double dy = event.y - lastPressY_;
    if (now - lastPressSeconds_ < 0.4 && dx * dx + dy * dy < 16.0) {
        clickCount_ = std::min(3, clickCount_ + 1);
    } else {
        clickCount_ = 1;
    }
    lastPressSeconds_ = now;
    lastPressX_ = event.x;
    lastPressY_ = event.y;
    const CharacterHit where = hit(event.x, event.y);
    if (!where.valid) {
        return;
    }
    mergeArmed_ = false;
    typingStyle_.reset();
    preferredX_ = -1;
    caretMovedAt_ = Now();
    if (alt && clickCount_ == 1) {
        addCaret(where.insertionIndex);
        drag_ = Drag::Text;
        return;
    }
    if (clickCount_ >= 3) {
        const int start = content_.offset(where.paragraph, 0);
        const int end = where.paragraph + 1 < content_.paragraphCount() ? content_.offset(where.paragraph + 1, 0)
                                                                        : content_.length();
        selectRange(start, end);
        anchorWord_ = where.paragraph;
        drag_ = Drag::Paragraph;
        return;
    }
    if (clickCount_ == 2) {
        int index = where.characterIndex >= 0 ? where.characterIndex : std::max(0, where.insertionIndex - 1);
        char32_t codepoint = 0;
        int start = index;
        int end = index;
        if (CodePointAt(content_, index, codepoint) && IsWord(codepoint)) {
            while (start > 0 && CodePointAt(content_, start - 1, codepoint) && IsWord(codepoint)) {
                --start;
            }
            end = index;
            while (end < content_.length() && CodePointAt(content_, end, codepoint) && IsWord(codepoint)) {
                ++end;
            }
        } else {
            while (start > 0 && CodePointAt(content_, start - 1, codepoint) && !IsWord(codepoint) && codepoint != U'\n') {
                --start;
            }
            end = index;
            while (end < content_.length() && CodePointAt(content_, end, codepoint) && !IsWord(codepoint) && codepoint != U'\n') {
                ++end;
            }
            if (end == start) {
                end = std::min(content_.length(), start + 1);
            }
        }
        selectRange(start, end);
        anchorWord_ = start;
        drag_ = Drag::Word;
        return;
    }
    if (shift) {
        primary().caret = where.insertionIndex;
        caretDirty_ = true;
        ensureCaretVisible();
    } else {
        selectRange(where.insertionIndex, where.insertionIndex);
    }
    drag_ = Drag::Text;
}

void StyledTextArea::handleMouseDragged(const MouseEvent& event) {
    if (drag_ == Drag::None) {
        return;
    }
    rebuild();
    const float localX = static_cast<float>(event.x - getAbsoluteX());
    const float localY = static_cast<float>(event.y - getAbsoluteY());
    if (drag_ == Drag::VerticalBar) {
        scrollY_ = verticalScroll_.offsetFromDrag(localX, localY, scrollGrab_);
        rebuild();
        return;
    }
    if (drag_ == Drag::HorizontalBar) {
        scrollX_ = horizontalScroll_.offsetFromDrag(localX, localY, scrollGrab_);
        rebuild();
        return;
    }
    const CharacterHit where = hit(event.x, event.y);
    if (!where.valid) {
        return;
    }
    if (drag_ == Drag::Word) {
        int index = where.characterIndex >= 0 ? where.characterIndex : where.insertionIndex;
        char32_t codepoint = 0;
        int start = index;
        int end = index;
        if (CodePointAt(content_, index, codepoint) && IsWord(codepoint)) {
            while (start > 0 && CodePointAt(content_, start - 1, codepoint) && IsWord(codepoint)) {
                --start;
            }
            while (end < content_.length() && CodePointAt(content_, end, codepoint) && IsWord(codepoint)) {
                ++end;
            }
        }
        const int anchor = anchorWord_;
        selectRange(std::min(anchor, start), std::max(primary().end(), end));
        return;
    }
    if (drag_ == Drag::Paragraph) {
        const int start = content_.offset(std::min(anchorWord_, where.paragraph), 0);
        const int last = std::max(anchorWord_, where.paragraph);
        const int end = last + 1 < content_.paragraphCount() ? content_.offset(last + 1, 0) : content_.length();
        selectRange(start, end);
        return;
    }
    primary().caret = where.insertionIndex;
    caretDirty_ = true;
    caretMovedAt_ = Now();
    ensureCaretVisible();
}

void StyledTextArea::handleMouseReleased(const MouseEvent&) { drag_ = Drag::None; }

void StyledTextArea::handleMouseMoved(const MouseEvent& event) {
    const CharacterHit where = hit(event.x, event.y);
    const int index = where.valid ? where.insertionIndex : -1;
    if (index != hoverIndex_) {
        hoverIndex_ = index;
        hoverSince_ = Now();
        hoverFired_ = false;
        if (hoverDelayMs_ <= 0 && onHover_ && index >= 0) {
            hoverFired_ = true;
            onHover_(index);
        }
    }
}

void StyledTextArea::handleScroll(ScrollEvent& event) {
    rebuild();
    const float line = std::max(8.f, areaFont().lineHeight());
    const bool shift = getScene() != nullptr && (getScene()->modifierMask() & Key::ModShift) != 0;
    if (shift || std::fabs(event.deltaX) > std::fabs(event.deltaY)) {
        if (!wrap_ && view_.contentWidth > view_.textW) {
            scrollX_ -= (event.deltaX + (shift ? event.deltaY : 0.0)) * 32.0;
            event.consume();
        }
    } else if (view_.contentHeight > view_.textH) {
        scrollY_ -= event.deltaY * static_cast<double>(line) * 2.0;
        event.consume();
    }
    rebuild();
}

void StyledTextArea::handleText(TextEvent& event) {
    if (!editable_ || event.text.empty()) {
        return;
    }
    for (unsigned char unit : event.text) {
        if (unit < 32) {
            return;
        }
    }
    const std::u32string characters = Utf32(event.text);
    const bool typing = characters.size() == 1;
    applyEditToCarets(event.text, typing);
    event.consume();
}

void StyledTextArea::handleKey(KeyEvent& event) {
    if (!event.pressed && !event.repeat) {
        return;
    }
    const bool select = event.shift;
    if (event.key == Key::Escape) {
        if (selections_.size() > 1) {
            clearSecondaryCarets();
            event.consume();
        } else if (primary().start() != primary().end()) {
            deselect();
            event.consume();
        }
        return;
    }
    if (event.shortcut() && event.key == Key::A) {
        selectAll();
        event.consume();
        return;
    }
    if (event.shortcut() && event.key == Key::C) {
        copy();
        event.consume();
        return;
    }
    if (event.shortcut() && event.key == Key::X) {
        cut();
        event.consume();
        return;
    }
    if (event.shortcut() && event.key == Key::V) {
        paste();
        event.consume();
        return;
    }
    if (event.shortcut() && event.key == Key::Z && !event.shift) {
        undo();
        event.consume();
        return;
    }
    if (event.shortcut() && (event.key == Key::Y || (event.key == Key::Z && event.shift))) {
        redo();
        event.consume();
        return;
    }
    if (event.shortcut() && event.key == Key::B) {
        toggleStyle(false);
        event.consume();
        return;
    }
    if (event.shortcut() && event.key == Key::U) {
        toggleStyle(true);
        event.consume();
        return;
    }
    if (event.shortcut() && event.alt && event.key == Key::LeftBracket) {
        const int from = content_.position(selection().start).paragraph;
        int to = content_.position(std::max(selection().start, selection().end - 1)).paragraph;
        if (to <= from && from + 1 < content_.paragraphCount()) {
            to = from + 1;
        }
        foldParagraphs(from, to);
        event.consume();
        return;
    }
    if (event.shortcut() && event.alt && event.key == Key::RightBracket) {
        unfoldParagraphs(currentParagraph());
        event.consume();
        return;
    }
    if (event.key == Key::Left || event.key == Key::Right) {
        if (event.meta) {
            moveLineEdge(event.key == Key::Right, select);
        } else {
            moveCarets(event.key == Key::Left ? -1 : 1, select, event.control || event.alt);
        }
        event.consume();
        return;
    }
    if (event.key == Key::Up || event.key == Key::Down) {
        if (event.meta) {
            moveToDocument(event.key == Key::Down, select);
        } else {
            moveVertical(event.key == Key::Down ? 1 : -1, select);
        }
        event.consume();
        return;
    }
    if (event.key == Key::Home) {
        if (event.shortcut() || event.meta) {
            moveToDocument(false, select);
        } else {
            moveLineEdge(false, select);
        }
        event.consume();
        return;
    }
    if (event.key == Key::End) {
        if (event.shortcut() || event.meta) {
            moveToDocument(true, select);
        } else {
            moveLineEdge(true, select);
        }
        event.consume();
        return;
    }
    if (event.key == Key::PageUp || event.key == Key::PageDown) {
        page(event.key == Key::PageDown, select);
        event.consume();
        return;
    }
    if (event.key == Key::Backspace) {
        deleteRanges(false, event.control || event.alt);
        event.consume();
        return;
    }
    if (event.key == Key::Delete) {
        deleteRanges(true, event.control || event.alt);
        event.consume();
        return;
    }
    if (event.key == Key::Enter || event.key == Key::KpEnter) {
        breakParagraphs();
        event.consume();
        return;
    }
    if (event.key == Key::Tab) {
        bool multiple = selections_.size() > 1;
        for (const Selection& selection : selections_) {
            if (selection.start() == selection.end()) {
                continue;
            }
            const int from = content_.position(selection.start()).paragraph;
            const int to = content_.position(selection.end() - 1).paragraph;
            if (from != to) {
                multiple = true;
            }
        }
        if (event.shift || multiple) {
            indentLines(event.shift);
        } else if (editable_) {
            applyEditToCarets(insertSpaces_ ? std::string(static_cast<std::size_t>(tabSize_), ' ') : std::string("\t"), false);
        }
        event.consume();
    }
}

void StyledTextArea::renderContent(UiRenderer& renderer, float opacity) {
    rebuild();
    const float absX = static_cast<float>(getAbsoluteX());
    const float absY = static_cast<float>(getAbsoluteY());
    const float boxX = absX + static_cast<float>(contentLeft());
    const float boxY = absY + static_cast<float>(contentTop());
    const float boxW = static_cast<float>(contentWidth());
    const float boxH = static_cast<float>(contentHeight());
    renderer.pushClip(absX, absY, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
    if (!computedStyle().background.visible) {
        Fill(renderer, boxX, boxY, boxW, boxH, Color::rgba(1.f, 1.f, 1.f, opacity));
    }
    if (view_.gutter > 0.f) {
        Fill(renderer, boxX, boxY, view_.gutter, std::max(0.f, boxH - (view_.horizontalBar ? view_.bar : 0.f)),
             Color::rgba(0.f, 0.f, 0.f, 0.035f * opacity));
    }
    const Font font = areaFont();
    const int caretParagraph = content_.position(primary().caret).paragraph;
    if (highlightLine_ && caretParagraph >= 0 && caretParagraph < static_cast<int>(view_.tops.size()) - 1 &&
        !isHidden(caretParagraph)) {
        const float y = absY + view_.textY + view_.tops[static_cast<std::size_t>(caretParagraph)] - static_cast<float>(scrollY_);
        const float height = view_.tops[static_cast<std::size_t>(caretParagraph + 1)] - view_.tops[static_cast<std::size_t>(caretParagraph)];
        Fill(renderer, boxX, y, boxW - (view_.verticalBar ? view_.bar : 0.f), height, Color::rgba(0.f, 0.f, 0.f, 0.045f * opacity));
    }

    renderer.pushClip(absX + view_.textX, absY + view_.textY, view_.textW, view_.textH);
    const float viewTop = static_cast<float>(scrollY_);
    const float viewBottom = viewTop + view_.textH;
    for (int paragraph = 0; paragraph < content_.paragraphCount(); ++paragraph) {
        if (isHidden(paragraph) || paragraph >= static_cast<int>(view_.paragraphs.size())) {
            continue;
        }
        const float top = view_.tops[static_cast<std::size_t>(paragraph)];
        const float bottom = view_.tops[static_cast<std::size_t>(paragraph + 1)];
        if (bottom < viewTop || top > viewBottom) {
            continue;
        }
        const Paragraph& model = content_.paragraph(paragraph);
        const ParagraphLayout& layout = view_.paragraphs[static_cast<std::size_t>(paragraph)];
        if (model.paragraphStyle().hasBackground) {
            Color color = model.paragraphStyle().background;
            color.a *= opacity;
            Fill(renderer, absX + view_.textX, absY + view_.textY + top - viewTop, view_.textW, bottom - top, color);
        }
        for (const LineLayout& line : layout.lines) {
            const float lineY = absY + view_.textY + top + line.y - viewTop;
            const MeasuredLine measured =
                MeasureRange(model.content(), line.start, line.end, font.family(), font.size(), tabSize_,
                             [&](int column) { return resolveStyle(SpanStyle(model, column)); });
            const int lineStart = content_.offset(paragraph, line.start);
            const int lineEnd = content_.offset(paragraph, line.end);
            for (const Selection& selection : selections_) {
                const int from = selection.start();
                const int to = selection.end();
                if (from == to || to <= lineStart || from >= lineEnd) {
                    const int paragraphEnd = content_.offset(paragraph, model.length());
                    if (from < to && line.end == model.length() && from <= paragraphEnd && to > paragraphEnd &&
                        from >= lineStart) {
                        const int local = std::max(0, std::min(static_cast<int>(measured.caret.size()) - 1, from - lineStart));
                        const float x = measured.caret[static_cast<std::size_t>(local)];
                        Fill(renderer, absX + view_.textX + x - static_cast<float>(scrollX_), lineY,
                             std::max(0.f, view_.textW - x + static_cast<float>(scrollX_)), line.height,
                             Color::rgba(0.10f, 0.45f, 0.91f, 0.28f * opacity));
                    }
                    continue;
                }
                const int selFrom = std::max(from, lineStart);
                const int selTo = std::min(to, lineEnd);
                const int a = std::max(0, std::min(static_cast<int>(measured.caret.size()) - 1, selFrom - lineStart));
                const int b = std::max(0, std::min(static_cast<int>(measured.caret.size()) - 1, selTo - lineStart));
                Fill(renderer, absX + view_.textX + measured.caret[static_cast<std::size_t>(a)] - static_cast<float>(scrollX_),
                     lineY, std::max(0.f, measured.caret[static_cast<std::size_t>(b)] - measured.caret[static_cast<std::size_t>(a)]),
                     line.height, Color::rgba(0.10f, 0.45f, 0.91f, 0.28f * opacity));
            }
            for (const MeasuredLine::Piece& piece : measured.pieces) {
                if (piece.tab || piece.end <= piece.begin) {
                    continue;
                }
                const std::u32string chunk = model.content().substr(static_cast<std::size_t>(piece.begin),
                                                                    static_cast<std::size_t>(piece.end - piece.begin));
                const std::string utf8 = Utf8(chunk);
                Color color = piece.style.hasFill ? piece.style.fill : computedStyle().color;
                color.a *= opacity;
                if (piece.style.hasBackground) {
                    Color back = piece.style.background;
                    back.a *= opacity;
                    const float width = piece.begin - line.start >= 0 &&
                                                piece.end - line.start < static_cast<int>(measured.caret.size())
                                            ? measured.caret[static_cast<std::size_t>(piece.end - line.start)] - piece.x
                                            : piece.style.fontSize;
                    Fill(renderer, absX + view_.textX + piece.x - static_cast<float>(scrollX_), lineY, std::max(0.f, width),
                         line.height, back);
                }
                const float drawX = absX + view_.textX + piece.x - static_cast<float>(scrollX_);
                renderer.text(drawX, lineY, utf8, font.family(), piece.fontSize, color, computedStyle().subpixel);
                if (piece.style.bold) {
                    renderer.text(drawX + 0.6f, lineY, utf8, font.family(), piece.fontSize, color, computedStyle().subpixel);
                }
                if (piece.style.underline) {
                    const int localEnd = piece.end - line.start;
                    const int localBegin = piece.begin - line.start;
                    if (localEnd >= 0 && localEnd < static_cast<int>(measured.caret.size()) && localBegin >= 0) {
                        Fill(renderer, drawX, lineY + measured.ascent + 1.f,
                             std::max(0.f, measured.caret[static_cast<std::size_t>(localEnd)] -
                                               measured.caret[static_cast<std::size_t>(localBegin)]),
                             1.f, color);
                    }
                }
                if (piece.style.strikethrough) {
                    const int localEnd = piece.end - line.start;
                    const int localBegin = piece.begin - line.start;
                    if (localEnd >= 0 && localEnd < static_cast<int>(measured.caret.size()) && localBegin >= 0) {
                        Fill(renderer, drawX, lineY + measured.ascent * 0.62f,
                             std::max(0.f, measured.caret[static_cast<std::size_t>(localEnd)] -
                                               measured.caret[static_cast<std::size_t>(localBegin)]),
                             1.f, color);
                    }
                }
            }
        }
    }
    bool caretOn = showCaret_ == CaretVisibility::On || (showCaret_ == CaretVisibility::Auto && isFocused());
    if (caretOn) {
        const double phase = Now() - caretMovedAt_;
        caretOn = std::fmod(phase, 1.06) < 0.53;
    }
    if (caretOn) {
        for (const Selection& selection : selections_) {
            const TextPos pos = content_.position(selection.caret);
            if (isHidden(pos.paragraph) || pos.paragraph >= static_cast<int>(view_.paragraphs.size())) {
                continue;
            }
            const ParagraphLayout& layout = view_.paragraphs[static_cast<std::size_t>(pos.paragraph)];
            const LineLayout* line = layout.lines.empty() ? nullptr : &layout.lines.front();
            for (std::size_t i = 0; i < layout.lines.size(); ++i) {
                const LineLayout& candidate = layout.lines[i];
                const bool last = i + 1 == layout.lines.size();
                if (pos.column >= candidate.start && (pos.column < candidate.end || (last && pos.column <= candidate.end))) {
                    line = &candidate;
                }
            }
            if (line == nullptr) {
                continue;
            }
            const MeasuredLine measured = MeasureRange(
                content_.paragraph(pos.paragraph).content(), line->start, line->end, font.family(), font.size(), tabSize_,
                [&](int column) { return resolveStyle(SpanStyle(content_.paragraph(pos.paragraph), column)); });
            const int local = std::max(0, std::min(static_cast<int>(measured.caret.size()) - 1, pos.column - line->start));
            const float x = absX + view_.textX + measured.caret[static_cast<std::size_t>(local)] - static_cast<float>(scrollX_);
            const float y = absY + view_.textY + view_.tops[static_cast<std::size_t>(pos.paragraph)] + line->y - viewTop;
            Color color = computedStyle().color;
            color.a *= opacity;
            Fill(renderer, x, y, 1.f, line->height, color);
        }
    }
    renderer.popClip();

    if (view_.gutter > 0.f) {
        renderer.pushClip(boxX, boxY, view_.gutter, std::max(0.f, boxH - (view_.horizontalBar ? view_.bar : 0.f)));
        for (int paragraph = 0; paragraph < content_.paragraphCount(); ++paragraph) {
            if (isHidden(paragraph)) {
                continue;
            }
            const float top = view_.tops[static_cast<std::size_t>(paragraph)];
            const float bottom = view_.tops[static_cast<std::size_t>(paragraph + 1)];
            if (bottom < viewTop || top > viewBottom) {
                continue;
            }
            const float y = absY + view_.textY + top - viewTop;
            if (isFoldHeader(paragraph)) {
                Color marker = computedStyle().color;
                marker.a *= 0.7f * opacity;
                renderer.text(boxX + 2.f, y, ">", font.family(), font.size(), marker, computedStyle().subpixel);
            }
            if (lineNumbers_) {
                const std::string number = std::to_string(paragraph + 1);
                const float width = font.measureWidth(number);
                Color color = computedStyle().color;
                color.a *= (paragraph == caretParagraph ? 0.9f : 0.45f) * opacity;
                renderer.text(boxX + view_.gutter - 8.f - width, y, number, font.family(), font.size(), color,
                              computedStyle().subpixel);
            }
        }
        renderer.popClip();
    }

    verticalScroll_.draw(renderer, absX, absY, opacity);
    horizontalScroll_.draw(renderer, absX, absY, opacity);
    renderer.popClip();
}

}  // namespace jadefx

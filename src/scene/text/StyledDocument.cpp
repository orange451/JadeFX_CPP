#include "jadefx/scene/text/EditableStyledDocument.hpp"

#include "scene/text/Unicode.hpp"

#include <algorithm>
#include <utility>

namespace jadefx {
namespace {

void Coalesce(std::vector<StyleSpan>& spans) {
    std::vector<StyleSpan> merged;
    for (const StyleSpan& span : spans) {
        if (span.length <= 0) {
            continue;
        }
        if (!merged.empty() && merged.back().style == span.style) {
            merged.back().length += span.length;
        } else {
            merged.push_back(span);
        }
    }
    spans = std::move(merged);
}

class SpanReader {
public:
    explicit SpanReader(const StyleSpans& spans) : spans_(spans.spans()) {}

    int remaining() const {
        int total = 0;
        for (std::size_t i = index_; i < spans_.size(); ++i) {
            total += std::max(0, spans_[i].length);
            if (i == index_) {
                total -= used_;
            }
        }
        return std::max(0, total);
    }

    int spanRemaining() const {
        if (index_ >= spans_.size()) {
            return 0;
        }
        return std::max(0, spans_[index_].length - used_);
    }

    TextStyle style() const {
        if (index_ >= spans_.size()) {
            return {};
        }
        return spans_[index_].style;
    }

    void consume(int count) {
        while (count > 0 && index_ < spans_.size()) {
            const int room = std::max(0, spans_[index_].length - used_);
            if (count < room) {
                used_ += count;
                return;
            }
            count -= room;
            ++index_;
            used_ = 0;
        }
    }

private:
    const std::vector<StyleSpan>& spans_;
    std::size_t index_ = 0;
    int used_ = 0;
};

}  // namespace

Paragraph::Paragraph() { spans_.push_back(StyleSpan{0, {}}); }

std::string Paragraph::text() const { return Utf8(text_); }

StyledDocument::StyledDocument() { paragraphs_.push_back(Paragraph{}); }

StyledDocument::StyledDocument(std::vector<Paragraph> paragraphs) : paragraphs_(std::move(paragraphs)) {
    if (paragraphs_.empty()) {
        paragraphs_.push_back(Paragraph{});
    }
    for (Paragraph& paragraph : paragraphs_) {
        normalize(paragraph);
    }
}

TextStyle StyledDocument::styleAtColumn(const Paragraph& paragraph, int column) {
    if (paragraph.spans_.empty()) {
        return {};
    }
    if (paragraph.text_.empty()) {
        return paragraph.spans_.front().style;
    }
    if (column < 0) {
        column = 0;
    }
    if (column >= paragraph.length()) {
        column = paragraph.length() - 1;
    }
    int cursor = 0;
    for (const StyleSpan& span : paragraph.spans_) {
        if (span.length <= 0) {
            continue;
        }
        if (column < cursor + span.length) {
            return span.style;
        }
        cursor += span.length;
    }
    return paragraph.spans_.back().style;
}

void StyledDocument::normalize(Paragraph& paragraph) {
    if (paragraph.text_.empty()) {
        const TextStyle style = paragraph.spans_.empty() ? TextStyle{} : paragraph.spans_.front().style;
        paragraph.spans_.clear();
        paragraph.spans_.push_back(StyleSpan{0, style});
        return;
    }
    Coalesce(paragraph.spans_);
    int covered = 0;
    for (const StyleSpan& span : paragraph.spans_) {
        covered += span.length;
    }
    if (covered != paragraph.length()) {
        const TextStyle style = paragraph.spans_.empty() ? TextStyle{} : paragraph.spans_.back().style;
        paragraph.spans_.clear();
        paragraph.spans_.push_back(StyleSpan{paragraph.length(), style});
    }
}

std::vector<StyleSpan> StyledDocument::sliceSpans(const Paragraph& paragraph, int start, int end) {
    std::vector<StyleSpan> out;
    if (end <= start) {
        return out;
    }
    int cursor = 0;
    for (const StyleSpan& span : paragraph.spans_) {
        const int spanEnd = cursor + std::max(0, span.length);
        const int from = std::max(start, cursor);
        const int to = std::min(end, spanEnd);
        if (to > from) {
            out.push_back(StyleSpan{to - from, span.style});
        }
        cursor = spanEnd;
    }
    return out;
}

Paragraph StyledDocument::slice(const Paragraph& source, int start, int end) {
    Paragraph out;
    out.style_ = source.style_;
    const int length = source.length();
    if (start < 0) {
        start = 0;
    }
    if (end > length) {
        end = length;
    }
    if (start > end) {
        start = end;
    }
    if (start == end) {
        out.spans_.push_back(StyleSpan{0, styleAtColumn(source, start)});
        return out;
    }
    out.text_ = source.text_.substr(static_cast<std::size_t>(start), static_cast<std::size_t>(end - start));
    out.spans_ = sliceSpans(source, start, end);
    normalize(out);
    return out;
}

void StyledDocument::append(Paragraph& dest, const Paragraph& source) {
    if (source.text_.empty()) {
        return;
    }
    if (dest.text_.empty()) {
        const ParagraphStyle kept = dest.style_;
        dest.text_ = source.text_;
        dest.spans_ = source.spans_;
        dest.style_ = kept.hasBackground ? kept : source.style_;
        normalize(dest);
        return;
    }
    for (const StyleSpan& span : source.spans_) {
        if (span.length <= 0) {
            continue;
        }
        if (!dest.spans_.empty() && dest.spans_.back().style == span.style) {
            dest.spans_.back().length += span.length;
        } else {
            dest.spans_.push_back(span);
        }
    }
    dest.text_ += source.text_;
    normalize(dest);
}

void StyledDocument::overlay(Paragraph& paragraph, int start, int end, const TextStyle& style) {
    if (start < 0) {
        start = 0;
    }
    if (end > paragraph.length()) {
        end = paragraph.length();
    }
    if (start >= end) {
        return;
    }
    std::vector<StyleSpan> next = sliceSpans(paragraph, 0, start);
    next.push_back(StyleSpan{end - start, style});
    const std::vector<StyleSpan> right = sliceSpans(paragraph, end, paragraph.length());
    next.insert(next.end(), right.begin(), right.end());
    paragraph.spans_ = std::move(next);
    normalize(paragraph);
}

StyledDocument StyledDocument::fromPlain(std::string_view text, const TextStyle& style,
                                         const ParagraphStyle& paragraphStyle) {
    std::vector<Paragraph> paragraphs;
    Paragraph current;
    current.style_ = paragraphStyle;
    current.spans_.clear();
    const std::u32string decoded = Utf32(text);
    auto flush = [&]() {
        if (current.text_.empty()) {
            current.spans_.clear();
            current.spans_.push_back(StyleSpan{0, style});
        } else {
            current.spans_.clear();
            current.spans_.push_back(StyleSpan{current.length(), style});
        }
        paragraphs.push_back(current);
        current = Paragraph{};
        current.style_ = paragraphStyle;
        current.text_.clear();
        current.spans_.clear();
    };
    for (std::size_t i = 0; i < decoded.size(); ++i) {
        const char32_t codepoint = decoded[i];
        if (codepoint == U'\r') {
            flush();
            if (i + 1 < decoded.size() && decoded[i + 1] == U'\n') {
                ++i;
            }
            continue;
        }
        if (codepoint == U'\n') {
            flush();
            continue;
        }
        current.text_.push_back(codepoint);
    }
    flush();
    return StyledDocument(std::move(paragraphs));
}

int StyledDocument::length() const {
    if (paragraphs_.empty()) {
        return 0;
    }
    int total = 0;
    for (std::size_t i = 0; i < paragraphs_.size(); ++i) {
        total += paragraphs_[i].length();
        if (i + 1 < paragraphs_.size()) {
            ++total;
        }
    }
    return total;
}

const Paragraph& StyledDocument::paragraph(int index) const {
    static const Paragraph kEmpty;
    if (paragraphs_.empty()) {
        return kEmpty;
    }
    if (index < 0) {
        index = 0;
    }
    if (index >= paragraphCount()) {
        index = paragraphCount() - 1;
    }
    return paragraphs_[static_cast<std::size_t>(index)];
}

std::string StyledDocument::text() const {
    std::string out;
    for (std::size_t i = 0; i < paragraphs_.size(); ++i) {
        if (i > 0) {
            out.push_back('\n');
        }
        out += paragraphs_[i].text();
    }
    return out;
}

std::string StyledDocument::text(int start, int end) const { return sub(start, end).text(); }

TextPos StyledDocument::position(int offset) const {
    if (paragraphs_.empty()) {
        return {};
    }
    if (offset < 0) {
        offset = 0;
    }
    int cursor = 0;
    for (int i = 0; i < paragraphCount(); ++i) {
        const int paragraphLength = paragraphs_[static_cast<std::size_t>(i)].length();
        if (offset <= cursor + paragraphLength) {
            return {i, offset - cursor};
        }
        cursor += paragraphLength;
        if (i + 1 < paragraphCount()) {
            ++cursor;
        }
    }
    const int last = paragraphCount() - 1;
    return {last, paragraphs_[static_cast<std::size_t>(last)].length()};
}

int StyledDocument::offset(int paragraphIndex, int column) const {
    if (paragraphs_.empty()) {
        return 0;
    }
    int index = paragraphIndex;
    if (index < 0) {
        index = 0;
    }
    if (index >= paragraphCount()) {
        index = paragraphCount() - 1;
    }
    int pos = 0;
    for (int i = 0; i < index; ++i) {
        pos += paragraphs_[static_cast<std::size_t>(i)].length() + 1;
    }
    pos += column;
    if (pos < 0) {
        pos = 0;
    }
    const int total = length();
    if (pos > total) {
        pos = total;
    }
    return pos;
}

StyledDocument StyledDocument::sub(int start, int end) const {
    if (start > end) {
        std::swap(start, end);
    }
    if (start < 0) {
        start = 0;
    }
    const int total = length();
    if (end > total) {
        end = total;
    }
    if (start > total) {
        start = total;
    }
    const TextPos from = position(start);
    const TextPos to = position(end);
    if (from.paragraph == to.paragraph) {
        std::vector<Paragraph> one;
        one.push_back(slice(paragraphs_[static_cast<std::size_t>(from.paragraph)], from.column, to.column));
        return StyledDocument(std::move(one));
    }
    std::vector<Paragraph> parts;
    const Paragraph& first = paragraphs_[static_cast<std::size_t>(from.paragraph)];
    parts.push_back(slice(first, from.column, first.length()));
    for (int i = from.paragraph + 1; i < to.paragraph; ++i) {
        parts.push_back(paragraphs_[static_cast<std::size_t>(i)]);
    }
    if (to.column == 0) {
        Paragraph empty;
        empty.style_ = paragraphs_[static_cast<std::size_t>(to.paragraph)].style_;
        empty.spans_.clear();
        empty.spans_.push_back(StyleSpan{0, styleAtColumn(paragraphs_[static_cast<std::size_t>(to.paragraph)], 0)});
        parts.push_back(std::move(empty));
    } else {
        parts.push_back(slice(paragraphs_[static_cast<std::size_t>(to.paragraph)], 0, to.column));
    }
    return StyledDocument(std::move(parts));
}

StyleSpans StyledDocument::styleSpans(int start, int end) const {
    if (start > end) {
        std::swap(start, end);
    }
    if (start < 0) {
        start = 0;
    }
    if (end > length()) {
        end = length();
    }
    StyleSpansBuilder builder;
    int pos = start;
    while (pos < end) {
        const TextPos at = position(pos);
        const Paragraph& paragraph = paragraphs_[static_cast<std::size_t>(at.paragraph)];
        const int room = paragraph.length() - at.column;
        if (room > 0) {
            const int take = std::min(room, end - pos);
            int cursor = 0;
            int local = at.column;
            const int localEnd = at.column + take;
            for (const StyleSpan& span : paragraph.spans_) {
                if (span.length <= 0) {
                    continue;
                }
                const int spanEnd = cursor + span.length;
                const int from = std::max(local, cursor);
                const int to = std::min(localEnd, spanEnd);
                if (to > from) {
                    builder.add(span.style, to - from);
                }
                cursor = spanEnd;
            }
            pos += take;
            continue;
        }
        if (at.paragraph + 1 < paragraphCount() && pos < end) {
            builder.add(styleAtColumn(paragraph, paragraph.length()), 1);
            ++pos;
            continue;
        }
        break;
    }
    return builder.create();
}

TextStyle StyledDocument::styleAt(int offset) const {
    if (offset < 0) {
        offset = 0;
    }
    if (offset > length()) {
        offset = length();
    }
    const TextPos at = position(offset);
    const Paragraph& paragraph = this->paragraph(at.paragraph);
    if (paragraph.length() == 0) {
        return styleAtColumn(paragraph, 0);
    }
    int column = at.column;
    if (column >= paragraph.length()) {
        column = paragraph.length() - 1;
    } else if (column > 0) {
        --column;
    }
    return styleAtColumn(paragraph, column);
}

StyledDocument StyledDocument::concat(const StyledDocument& left, const StyledDocument& right) {
    if (left.paragraphCount() == 0) {
        return right;
    }
    if (right.paragraphCount() == 0) {
        return left;
    }
    std::vector<Paragraph> paragraphs = left.paragraphs_;
    Paragraph& tail = paragraphs.back();
    const bool tailWasEmpty = tail.text_.empty();
    const ParagraphStyle tailStyle = tail.style_;
    append(tail, right.paragraphs_.front());
    if (tailWasEmpty) {
        tail.style_ = right.paragraphs_.front().style_;
    } else {
        tail.style_ = tailStyle;
    }
    for (std::size_t i = 1; i < right.paragraphs_.size(); ++i) {
        paragraphs.push_back(right.paragraphs_[i]);
    }
    return StyledDocument(std::move(paragraphs));
}

bool StyledDocument::operator==(const StyledDocument& other) const {
    if (paragraphs_.size() != other.paragraphs_.size()) {
        return false;
    }
    for (std::size_t i = 0; i < paragraphs_.size(); ++i) {
        const Paragraph& a = paragraphs_[i];
        const Paragraph& b = other.paragraphs_[i];
        if (a.text_ != b.text_ || !(a.style_ == b.style_) || a.spans_.size() != b.spans_.size()) {
            return false;
        }
        for (std::size_t s = 0; s < a.spans_.size(); ++s) {
            if (a.spans_[s].length != b.spans_[s].length || !(a.spans_[s].style == b.spans_[s].style)) {
                return false;
            }
        }
    }
    return true;
}

EditableStyledDocument::EditableStyledDocument() { install(std::vector<Paragraph>{Paragraph{}}); }

EditableStyledDocument::EditableStyledDocument(std::string text, const TextStyle& style,
                                               const ParagraphStyle& paragraphStyle) {
    install(StyledDocument::fromPlain(text, style, paragraphStyle).paragraphs_);
}

void EditableStyledDocument::install(std::vector<Paragraph> paragraphs) {
    if (paragraphs.empty()) {
        paragraphs.push_back(Paragraph{});
    }
    for (Paragraph& paragraph : paragraphs) {
        StyledDocument::normalize(paragraph);
        paragraph.revision_ = ++revisionClock_;
    }
    document_ = StyledDocument(std::move(paragraphs));
    for (Paragraph& paragraph : document_.paragraphs_) {
        if (paragraph.revision_ == 0) {
            paragraph.revision_ = ++revisionClock_;
        }
    }
}

void EditableStyledDocument::notify(const DocumentChange& change) {
    if (onRich_) {
        onRich_(change);
    }
    if (change.isParagraphStyle() || !change.changesText() || !onPlain_) {
        return;
    }
    PlainTextChange plain;
    plain.position = change.position;
    plain.removed = change.removed.text();
    plain.inserted = change.inserted.text();
    onPlain_(plain);
}

DocumentChange EditableStyledDocument::replace(int start, int end, const StyledDocument& replacement) {
    if (start > end) {
        std::swap(start, end);
    }
    if (start < 0) {
        start = 0;
    }
    if (end > document_.length()) {
        end = document_.length();
    }
    if (start > document_.length()) {
        start = document_.length();
    }
    DocumentChange change;
    change.position = start;
    change.removed = document_.sub(start, end);
    change.inserted = replacement;
    const StyledDocument before = document_.sub(0, start);
    const StyledDocument after = document_.sub(end, document_.length());
    const StyledDocument next = StyledDocument::concat(StyledDocument::concat(before, replacement), after);
    install(next.paragraphs_);
    if (!(change.removed == change.inserted)) {
        notify(change);
    }
    return change;
}

DocumentChange EditableStyledDocument::replaceText(int start, int end, std::string_view text, const TextStyle& style,
                                                   const ParagraphStyle& paragraphStyle) {
    return replace(start, end, StyledDocument::fromPlain(text, style, paragraphStyle));
}

DocumentChange EditableStyledDocument::setStyle(int start, int end, const TextStyle& style) {
    StyleSpansBuilder builder;
    if (end < start) {
        std::swap(start, end);
    }
    builder.add(style, end - start);
    return setStyleSpans(start, builder.create());
}

DocumentChange EditableStyledDocument::setStyleSpans(int start, const StyleSpans& spans) {
    if (start < 0) {
        start = 0;
    }
    if (start > document_.length()) {
        start = document_.length();
    }
    const int applyEnd = std::min(document_.length(), start + spans.length());
    DocumentChange change;
    change.position = start;
    change.removed = document_.sub(start, applyEnd);
    SpanReader reader(spans);
    int pos = start;
    while (pos < applyEnd && reader.remaining() > 0) {
        const TextPos at = document_.position(pos);
        Paragraph& paragraph = document_.paragraphs_[static_cast<std::size_t>(at.paragraph)];
        const int room = paragraph.length() - at.column;
        if (room > 0) {
            if (reader.spanRemaining() <= 0) {
                break;
            }
            int left = std::min(room, applyEnd - pos);
            int column = at.column;
            while (left > 0 && reader.spanRemaining() > 0) {
                const int piece = std::min(left, reader.spanRemaining());
                StyledDocument::overlay(paragraph, column, column + piece, reader.style());
                reader.consume(piece);
                column += piece;
                left -= piece;
                pos += piece;
            }
            paragraph.revision_ = ++revisionClock_;
            continue;
        }
        if (at.paragraph + 1 < document_.paragraphCount() && pos < applyEnd) {
            reader.consume(std::min(1, reader.remaining()));
            ++pos;
            continue;
        }
        break;
    }
    change.inserted = document_.sub(start, applyEnd);
    if (!(change.removed == change.inserted)) {
        notify(change);
    }
    return change;
}

DocumentChange EditableStyledDocument::setParagraphStyle(int index, const ParagraphStyle& style) {
    DocumentChange change;
    if (document_.paragraphCount() == 0) {
        return change;
    }
    if (index < 0) {
        index = 0;
    }
    if (index >= document_.paragraphCount()) {
        index = document_.paragraphCount() - 1;
    }
    Paragraph& paragraph = document_.paragraphs_[static_cast<std::size_t>(index)];
    change.paragraphIndex = index;
    change.oldParagraph = paragraph.style_;
    change.newParagraph = style;
    if (paragraph.style_ == style) {
        return change;
    }
    paragraph.style_ = style;
    paragraph.revision_ = ++revisionClock_;
    notify(change);
    return change;
}

}  // namespace jadefx

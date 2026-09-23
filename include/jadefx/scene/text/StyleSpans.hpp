#pragma once

#include "jadefx/scene/text/TextStyle.hpp"

#include <vector>

namespace jadefx {

// A contiguous run of code points that share one style. Lengths are Unicode code points.
struct StyleSpan {
    int length = 0;
    TextStyle style;
};

// Styles covering a flat range of the document. A newline between paragraphs counts as one code point.
class StyleSpans {
public:
    StyleSpans() = default;
    explicit StyleSpans(std::vector<StyleSpan> spans) : spans_(std::move(spans)) {}

    const std::vector<StyleSpan>& spans() const { return spans_; }
    bool empty() const { return spans_.empty(); }

    int length() const {
        int total = 0;
        for (const StyleSpan& span : spans_) {
            if (span.length > 0) {
                total += span.length;
            }
        }
        return total;
    }

private:
    std::vector<StyleSpan> spans_;
};

class StyleSpansBuilder {
public:
    void add(const TextStyle& style, int length) {
        if (length <= 0) {
            return;
        }
        if (!spans_.empty() && spans_.back().style == style) {
            spans_.back().length += length;
            return;
        }
        spans_.push_back(StyleSpan{length, style});
    }

    StyleSpans create() const { return StyleSpans(spans_); }

private:
    std::vector<StyleSpan> spans_;
};

}  // namespace jadefx

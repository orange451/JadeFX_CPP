#include "jadefx/scene/Controls/Labeled.hpp"

#include "gl/UiRenderer.hpp"

#include <cstddef>

namespace jadefx {
namespace {

// U+2026 HORIZONTAL ELLIPSIS, the mark Labeled uses when a line does not fit.
constexpr char kEllipsis[] = "\u2026";

double Align(double space, double child, int mode) {
    const double extra = space - child;
    if (mode == 1) {
        return extra * 0.5;
    }
    if (mode == 2) {
        return extra;
    }
    return 0;
}

std::size_t Utf8Unit(const std::string& text, std::size_t index) {
    if (index >= text.size()) {
        return 0;
    }
    const auto lead = static_cast<unsigned char>(text[index]);
    std::size_t need = 1;
    if ((lead & 0x80) == 0) {
        need = 1;
    } else if ((lead & 0xE0) == 0xC0 && lead >= 0xC2) {
        need = 2;
    } else if ((lead & 0xF0) == 0xE0) {
        need = 3;
    } else if ((lead & 0xF8) == 0xF0) {
        need = 4;
    } else {
        return 1;
    }
    if (index + need > text.size()) {
        return text.size() - index;
    }
    return need;
}

// Longest prefix of text, plus an ellipsis, that fits in maxWidth.
// The mark alone is used when no character fits. Nothing is drawn when the mark itself does not fit.
std::string FitLine(const Font& font, const std::string& text, double maxWidth) {
    if (text.empty() || maxWidth <= 0.0) {
        return {};
    }
    if (static_cast<double>(font.measureWidth(text)) <= maxWidth) {
        return text;
    }
    const std::string mark(kEllipsis);
    if (static_cast<double>(font.measureWidth(mark)) > maxWidth) {
        return {};
    }
    std::string fitted = mark;
    std::string prefix;
    std::size_t index = 0;
    while (index < text.size()) {
        const std::size_t bytes = Utf8Unit(text, index);
        if (bytes == 0) {
            break;
        }
        prefix.append(text, index, bytes);
        index += bytes;
        const std::string candidate = prefix + mark;
        if (static_cast<double>(font.measureWidth(candidate)) > maxWidth) {
            break;
        }
        fitted = candidate;
    }
    return fitted;
}

}  // namespace

Labeled::Labeled(std::string text) : text_(std::move(text)) {}

void Labeled::setText(std::string text) { text_ = std::move(text); }

std::string Labeled::displayedText() const {
    const Font face(computedStyle().fontFamily, computedStyle().fontSize);
    return FitLine(face, text_, contentWidth());
}

void Labeled::setTextFill(const Color& color) { setTextFillInternal(color, true); }

void Labeled::setFont(const Font& font) { setFontInternal(font, true); }

double Labeled::preferredContentWidth(double) const {
    const Font face(computedStyle().fontFamily, computedStyle().fontSize);
    return face.measureWidth(text_);
}

double Labeled::preferredContentHeight(double) const {
    if (text_.empty()) {
        return 0;
    }
    const Font face(computedStyle().fontFamily, computedStyle().fontSize);
    return face.shape(text_).height;
}

void Labeled::renderContent(UiRenderer& renderer, float opacity) {
    const std::string shown = displayedText();
    if (shown.empty()) {
        return;
    }
    const Font face(computedStyle().fontFamily, computedStyle().fontSize);
    const ShapedText shaped = face.shape(shown);
    const Pos align = usingAlignment();
    const int horizontal = hpos(align) == HPos::Center ? 1 : (hpos(align) == HPos::Right ? 2 : 0);
    const int vertical = vpos(align) == VPos::Center ? 1 : (vpos(align) == VPos::Bottom ? 2 : 0);
    const double x = contentLeft() + Align(contentWidth(), shaped.width, horizontal);
    const double y = contentTop() + Align(contentHeight(), shaped.height, vertical);
    Color color = computedStyle().color;
    color.a *= opacity;
    renderer.text(static_cast<float>(getAbsoluteX() + x), static_cast<float>(getAbsoluteY() + y), shown,
                  computedStyle().fontFamily, computedStyle().fontSize, color, computedStyle().subpixel);
}

}  // namespace jadefx

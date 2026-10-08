#include "jadefx/scene/controls/Labeled.hpp"

#include "gl/UiRenderer.hpp"

#include <algorithm>
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

struct Labeled::Block {
    double graphicX = 0;
    double graphicY = 0;
    double graphicWidth = 0;
    double graphicHeight = 0;
    double textX = 0;
    double textY = 0;
    double textWidth = 0;
    double textHeight = 0;
    float fontSize = 0.f;
};

Labeled::Labeled(std::string text) : text_(std::move(text)) {}

void Labeled::setText(std::string text) {
    if (text_ == text) {
        return;
    }
    text_ = std::move(text);
    markLayoutDirty();
}

std::string Labeled::displayedText() const {
    if (!showsText()) {
        return {};
    }
    const Block block = arrange();
    return FitLine(Font(computedStyle().fontFamily, block.fontSize), text_, block.textWidth);
}

void Labeled::setTextFill(const Color& color) { setTextFillInternal(color, true); }

void Labeled::setFont(const Font& font) { setFontInternal(font, true); }

float Labeled::displayedFontSize() const { return arrange().fontSize; }

float Labeled::scaledFontSize(double width, double height) const {
    const ComputedStyle& style = computedStyle();
    const float base = style.fontSize > 0.f ? style.fontSize : 16.f;
    ScaledFit& fit = scaledFit_;
    if (fit.text == text_ && fit.family == style.fontFamily && fit.base == base && fit.width == width &&
        fit.height == height) {
        return fit.size;
    }
    float size = base;
    const ShapedText measured = Font(style.fontFamily, base).shape(text_);
    if (width > 0.0 && height > 0.0 && measured.width > 0.f && measured.height > 0.f) {
        // Text grows about in proportion to its size, so one ratio lands near
        // the answer. Hinting can leave it a little over, which the loop takes back.
        const double ratio = std::min(width / measured.width, height / measured.height);
        size = static_cast<float>(std::clamp(base * ratio, 1.0, static_cast<double>(kMaxScaledFontSize)));
        for (int step = 0; step < 16 && size > 1.f; ++step) {
            const ShapedText shaped = Font(style.fontFamily, size).shape(text_);
            if (shaped.width <= width && shaped.height <= height) {
                break;
            }
            const double over = std::min(width / shaped.width, height / shaped.height);
            size = std::max(1.f, std::min(static_cast<float>(size * over), size - 0.25f));
        }
    }
    fit = ScaledFit{text_, style.fontFamily, base, width, height, size};
    return size;
}

void Labeled::setGraphic(std::shared_ptr<Node> graphic) {
    if (graphic == graphic_) {
        return;
    }
    if (graphic_) {
        Node* old = graphic_.get();
        children().removeIf([old](const std::shared_ptr<Node>& child) { return child.get() == old; });
    }
    graphic_ = std::move(graphic);
    if (graphic_) {
        children().add(graphic_);
    }
}

void Labeled::detachChild(Node* child) {
    Controls::detachChild(child);
    if (graphic_ && graphic_.get() == child) {
        graphic_.reset();
    }
}

bool Labeled::showsText() const { return !text_.empty() && contentDisplay_ != ContentDisplay::GraphicOnly; }

bool Labeled::showsGraphic() const {
    return graphic_ && graphic_->isVisible() && contentDisplay_ != ContentDisplay::TextOnly;
}

Labeled::Block Labeled::arrange() const {
    Block block;
    const double boxWidth = contentWidth();
    const double boxHeight = contentHeight();
    const bool text = showsText();
    const bool graphic = showsGraphic();
    if (graphic) {
        block.graphicWidth = graphic_->measuredWidth(boxWidth);
        block.graphicHeight = graphic_->measuredHeight(block.graphicWidth, boxHeight);
    }
    const bool sideways = contentDisplay_ == ContentDisplay::Left || contentDisplay_ == ContentDisplay::Right;
    const bool stacked = contentDisplay_ == ContentDisplay::Top || contentDisplay_ == ContentDisplay::Bottom;
    const double gap = text && graphic && (sideways || stacked) ? graphicTextGap_ : 0.0;
    block.fontSize = computedStyle().fontSize;
    if (text) {
        // The text gives up width first, and ends in an ellipsis.
        const double room = sideways ? boxWidth - block.graphicWidth - gap : boxWidth;
        if (textScaled_) {
            block.fontSize = scaledFontSize(room, stacked ? boxHeight - block.graphicHeight - gap : boxHeight);
        }
        const ShapedText shaped = Font(computedStyle().fontFamily, block.fontSize).shape(text_);
        block.textWidth = std::max(0.0, std::min(static_cast<double>(shaped.width), room));
        block.textHeight = shaped.height;
    }
    const double width =
        sideways ? block.graphicWidth + gap + block.textWidth : std::max(block.graphicWidth, block.textWidth);
    const double height =
        stacked ? block.graphicHeight + gap + block.textHeight : std::max(block.graphicHeight, block.textHeight);
    const Pos align = usingAlignment();
    const int horizontal = hpos(align) == HPos::Center ? 1 : (hpos(align) == HPos::Right ? 2 : 0);
    const int vertical = vpos(align) == VPos::Center ? 1 : (vpos(align) == VPos::Bottom ? 2 : 0);
    const double left = contentLeft() + Align(boxWidth, width, horizontal);
    const double top = contentTop() + Align(boxHeight, height, vertical);
    // Across the stacking axis, the smaller of the two is centered on the larger.
    block.graphicX = left + (width - block.graphicWidth) * 0.5;
    block.textX = left + (width - block.textWidth) * 0.5;
    block.graphicY = top + (height - block.graphicHeight) * 0.5;
    block.textY = top + (height - block.textHeight) * 0.5;
    switch (contentDisplay_) {
        case ContentDisplay::Left:
            block.graphicX = left;
            block.textX = left + block.graphicWidth + gap;
            break;
        case ContentDisplay::Right:
            block.textX = left;
            block.graphicX = left + block.textWidth + gap;
            break;
        case ContentDisplay::Top:
            block.graphicY = top;
            block.textY = top + block.graphicHeight + gap;
            break;
        case ContentDisplay::Bottom:
            block.textY = top;
            block.graphicY = top + block.textHeight + gap;
            break;
        case ContentDisplay::Center:
        case ContentDisplay::TextOnly:
        case ContentDisplay::GraphicOnly:
            break;
    }
    return block;
}

void Labeled::layoutChildren() {
    if (!graphic_) {
        return;
    }
    if (!showsGraphic()) {
        graphic_->performLayout(0, 0, 0, 0);
        return;
    }
    const Block block = arrange();
    graphic_->performLayout(block.graphicX, block.graphicY, block.graphicWidth, block.graphicHeight);
}

double Labeled::preferredContentWidth(double innerAvailable) const {
    const bool text = showsText();
    const bool graphic = showsGraphic();
    const double textWidth =
        text ? static_cast<double>(Font(computedStyle().fontFamily, computedStyle().fontSize).measureWidth(text_)) : 0.0;
    const double graphicWidth = graphic ? graphic_->measuredWidth(innerAvailable) : 0.0;
    if (contentDisplay_ == ContentDisplay::Left || contentDisplay_ == ContentDisplay::Right) {
        return textWidth + graphicWidth + (text && graphic ? graphicTextGap_ : 0.0);
    }
    return std::max(textWidth, graphicWidth);
}

double Labeled::preferredContentHeight(double innerWidth) const {
    const bool text = showsText();
    const bool graphic = showsGraphic();
    const double textHeight =
        text ? static_cast<double>(Font(computedStyle().fontFamily, computedStyle().fontSize).shape(text_).height) : 0.0;
    double graphicHeight = 0.0;
    if (graphic) {
        const double graphicWidth = graphic_->measuredWidth(innerWidth);
        graphicHeight = graphic_->measuredHeight(graphicWidth, -1);
    }
    if (contentDisplay_ == ContentDisplay::Top || contentDisplay_ == ContentDisplay::Bottom) {
        return textHeight + graphicHeight + (text && graphic ? graphicTextGap_ : 0.0);
    }
    return std::max(textHeight, graphicHeight);
}

void Labeled::renderContent(UiRenderer& renderer, float opacity) {
    if (!showsText()) {
        return;
    }
    const Block block = arrange();
    const Font face(computedStyle().fontFamily, block.fontSize);
    const std::string shown = FitLine(face, text_, block.textWidth);
    if (shown.empty()) {
        return;
    }
    Color color = computedStyle().color;
    color.a *= opacity;
    renderer.text(static_cast<float>(getAbsoluteX() + block.textX), static_cast<float>(getAbsoluteY() + block.textY),
                  shown, computedStyle().fontFamily, block.fontSize, color, computedStyle().subpixel);
}

}  // namespace jadefx

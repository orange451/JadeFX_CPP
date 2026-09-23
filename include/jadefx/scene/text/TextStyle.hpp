#pragma once

#include "jadefx/paint/Color.hpp"

#include <string>

namespace jadefx {

// How one run of characters is drawn. Unset colors inherit the area's font color.
// styleClass is resolved by StyleClassedTextArea. inlineCss is the source form
// kept by InlineCssTextArea; the parsed fields are what drawing uses.
struct TextStyle {
    Color fill = Color::black();
    bool hasFill = false;
    Color background = Color::transparent();
    bool hasBackground = false;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
    // Zero uses the area's font size.
    float fontSize = 0.f;
    std::string styleClass;
    std::string inlineCss;

    bool operator==(const TextStyle& other) const {
        return hasFill == other.hasFill && hasBackground == other.hasBackground && bold == other.bold &&
               italic == other.italic && underline == other.underline && strikethrough == other.strikethrough &&
               fontSize == other.fontSize && styleClass == other.styleClass && inlineCss == other.inlineCss &&
               fill.r == other.fill.r && fill.g == other.fill.g && fill.b == other.fill.b && fill.a == other.fill.a &&
               background.r == other.background.r && background.g == other.background.g &&
               background.b == other.background.b && background.a == other.background.a;
    }

    bool operator!=(const TextStyle& other) const { return !(*this == other); }
};

// Background for a whole paragraph, including wrapped lines.
struct ParagraphStyle {
    Color background = Color::transparent();
    bool hasBackground = false;

    bool operator==(const ParagraphStyle& other) const {
        return hasBackground == other.hasBackground && background.r == other.background.r &&
               background.g == other.background.g && background.b == other.background.b &&
               background.a == other.background.a;
    }

    bool operator!=(const ParagraphStyle& other) const { return !(*this == other); }
};

}  // namespace jadefx

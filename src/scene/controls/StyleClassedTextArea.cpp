#include "jadefx/scene/controls/StyleClassedTextArea.hpp"

#include <cctype>

namespace jadefx {

void StyleClassedTextArea::defineStyleClass(std::string name, TextStyle style) {
    classes_[std::move(name)] = std::move(style);
}

void StyleClassedTextArea::setStyleClass(int start, int end, std::string className) {
    TextStyle style;
    style.styleClass = std::move(className);
    setStyle(start, end, style);
}

TextStyle StyleClassedTextArea::resolveStyle(const TextStyle& style) const {
    TextStyle out = style;
    std::string token;
    auto apply = [&](const std::string& name) {
        if (name.empty()) {
            return;
        }
        const auto found = classes_.find(name);
        if (found == classes_.end()) {
            return;
        }
        const TextStyle& defined = found->second;
        if (!out.hasFill && defined.hasFill) {
            out.fill = defined.fill;
            out.hasFill = true;
        }
        if (!out.hasBackground && defined.hasBackground) {
            out.background = defined.background;
            out.hasBackground = true;
        }
        if (defined.bold) {
            out.bold = true;
        }
        if (defined.italic) {
            out.italic = true;
        }
        if (defined.underline) {
            out.underline = true;
        }
        if (defined.strikethrough) {
            out.strikethrough = true;
        }
        if (out.fontSize <= 0.f && defined.fontSize > 0.f) {
            out.fontSize = defined.fontSize;
        }
    };
    for (char unit : style.styleClass) {
        if (std::isspace(static_cast<unsigned char>(unit))) {
            apply(token);
            token.clear();
        } else {
            token.push_back(unit);
        }
    }
    apply(token);
    return out;
}

}  // namespace jadefx

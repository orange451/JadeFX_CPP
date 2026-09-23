#pragma once

#include "jadefx/scene/text/TextStyle.hpp"

#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>

namespace jadefx {

inline std::string_view TrimCss(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return text;
}

inline bool EqualsCss(std::string_view text, std::string_view expected) {
    if (text.size() != expected.size()) {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(text[i])) != expected[i]) {
            return false;
        }
    }
    return true;
}

// A small subset of CSS and JavaFX -fx- text properties.
inline TextStyle ParseTextCss(std::string_view css) {
    TextStyle style;
    style.inlineCss = std::string(css);
    std::size_t cursor = 0;
    while (cursor < css.size()) {
        const std::size_t end = css.find(';', cursor);
        const std::string_view piece = TrimCss(css.substr(cursor, end == std::string_view::npos ? css.size() - cursor : end - cursor));
        cursor = end == std::string_view::npos ? css.size() : end + 1;
        const std::size_t colon = piece.find(':');
        if (colon == std::string_view::npos) {
            continue;
        }
        const std::string_view name = TrimCss(piece.substr(0, colon));
        const std::string_view value = TrimCss(piece.substr(colon + 1));
        if (EqualsCss(name, "color") || EqualsCss(name, "-fx-fill")) {
            bool ok = false;
            const Color color = Color::parse(value, &ok);
            if (ok) {
                style.fill = color;
                style.hasFill = true;
            }
        } else if (EqualsCss(name, "background-color") || EqualsCss(name, "-fx-background-color")) {
            bool ok = false;
            const Color color = Color::parse(value, &ok);
            if (ok) {
                style.background = color;
                style.hasBackground = true;
            }
        } else if (EqualsCss(name, "font-weight") || EqualsCss(name, "-fx-font-weight")) {
            style.bold = EqualsCss(value, "bold") || value == "700" || value == "800" || value == "900";
        } else if (EqualsCss(name, "font-style") || EqualsCss(name, "-fx-font-style")) {
            style.italic = EqualsCss(value, "italic") || EqualsCss(value, "oblique");
        } else if (EqualsCss(name, "font-size") || EqualsCss(name, "-fx-font-size")) {
            style.fontSize = static_cast<float>(std::atof(std::string(value).c_str()));
        } else if (EqualsCss(name, "text-decoration")) {
            style.underline = value.find("underline") != std::string_view::npos;
            style.strikethrough = value.find("line-through") != std::string_view::npos;
        } else if (EqualsCss(name, "-fx-underline")) {
            style.underline = EqualsCss(value, "true");
        } else if (EqualsCss(name, "-fx-strikethrough")) {
            style.strikethrough = EqualsCss(value, "true");
        }
    }
    return style;
}

}  // namespace jadefx

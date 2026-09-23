#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace jadefx {

inline std::string trimCopy(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

inline std::string lowerCopy(std::string text) {
    for (char& ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

inline std::string stripCssComments(std::string_view css) {
    std::string out;
    out.reserve(css.size());
    for (std::size_t i = 0; i < css.size(); ++i) {
        if (css[i] == '/' && i + 1 < css.size() && css[i + 1] == '*') {
            i += 2;
            while (i + 1 < css.size() && !(css[i] == '*' && css[i + 1] == '/')) {
                ++i;
            }
            if (i + 1 < css.size()) {
                ++i;
            }
            continue;
        }
        out.push_back(css[i]);
    }
    return out;
}

}  // namespace jadefx

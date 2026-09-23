#pragma once

#include <string>
#include <string_view>

namespace jadefx {

inline std::u32string Utf32(std::string_view text) {
    std::u32string out;
    out.reserve(text.size());
    for (std::size_t index = 0; index < text.size();) {
        const unsigned char lead = static_cast<unsigned char>(text[index++]);
        if (lead < 0x80) {
            out.push_back(lead);
            continue;
        }
        int need = 0;
        char32_t value = 0;
        if ((lead & 0xE0) == 0xC0 && lead >= 0xC2) {
            need = 1;
            value = lead & 0x1F;
        } else if ((lead & 0xF0) == 0xE0) {
            need = 2;
            value = lead & 0x0F;
        } else if ((lead & 0xF8) == 0xF0 && lead <= 0xF4) {
            need = 3;
            value = lead & 0x07;
        } else {
            out.push_back(0xFFFD);
            continue;
        }
        bool ok = true;
        for (int i = 0; i < need; ++i) {
            if (index >= text.size()) {
                ok = false;
                break;
            }
            const unsigned char next = static_cast<unsigned char>(text[index]);
            if ((next & 0xC0) != 0x80) {
                ok = false;
                break;
            }
            value = (value << 6) | (next & 0x3F);
            ++index;
        }
        if (!ok || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
            out.push_back(0xFFFD);
            continue;
        }
        out.push_back(value);
    }
    return out;
}

inline std::string Utf8(std::u32string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char32_t codepoint : text) {
        if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            codepoint = 0xFFFD;
        }
        if (codepoint < 0x80) {
            out.push_back(static_cast<char>(codepoint));
        } else if (codepoint < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }
    return out;
}

inline std::string Utf8(char32_t codepoint) {
    return Utf8(std::u32string_view(&codepoint, 1));
}

}  // namespace jadefx

#include "jadefx/paint/Color.hpp"

#include "internal/Text.hpp"

#include <cstdlib>
#include <string>

namespace jadefx {
namespace {

int Hex(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

int HexByte(char hi, char lo) {
    const int high = Hex(hi);
    const int low = Hex(lo);
    if (high < 0 || low < 0) {
        return -1;
    }
    return high * 16 + low;
}

float Channel8(int value) {
    if (value < 0) {
        value = 0;
    }
    if (value > 255) {
        value = 255;
    }
    return static_cast<float>(value) / 255.f;
}

bool Named(std::string_view name, Color& color) {
    const std::string key = lowerCopy(std::string(name));
    struct Entry {
        const char* name;
        unsigned rgb;
    };
    // A useful slice of the CSS names JadeFX's Color class accepts.
    static const Entry kNames[] = {
        {"black", 0x000000},   {"white", 0xFFFFFF},   {"red", 0xFF0000},    {"green", 0x008000},
        {"lime", 0x00FF00},    {"blue", 0x0000FF},    {"yellow", 0xFFFF00}, {"cyan", 0x00FFFF},
        {"aqua", 0x00FFFF},    {"magenta", 0xFF00FF}, {"fuchsia", 0xFF00FF}, {"orange", 0xFFA500},
        {"purple", 0x800080},  {"gray", 0x808080},    {"grey", 0x808080},   {"silver", 0xC0C0C0},
        {"maroon", 0x800000},  {"olive", 0x808000},   {"teal", 0x008080},   {"navy", 0x000080},
        {"whitesmoke", 0xF5F5F5}, {"snow", 0xFFFAFA}, {"gainsboro", 0xDCDCDC}, {"transparent", 0},
    };
    if (key == "transparent") {
        color = Color::transparent();
        return true;
    }
    for (const Entry& entry : kNames) {
        if (key == entry.name) {
            color = Color::rgb8((entry.rgb >> 16) & 255, (entry.rgb >> 8) & 255, entry.rgb & 255);
            return true;
        }
    }
    return false;
}

float Component(std::string_view token, bool alpha) {
    const std::string text = trimCopy(token);
    if (text.empty()) {
        return 0.f;
    }
    char* end = nullptr;
    const float number = std::strtof(text.c_str(), &end);
    const bool percent = end != nullptr && *end == '%';
    if (percent) {
        return number / 100.f;
    }
    if (alpha && number <= 1.f) {
        return number;
    }
    if (number > 1.f) {
        return number / 255.f;
    }
    return number;
}

bool ParseRgb(std::string_view text, Color& color) {
    const std::string lower = lowerCopy(std::string(text));
    const bool hasAlpha = lower.rfind("rgba", 0) == 0;
    if (lower.rfind("rgb", 0) != 0) {
        return false;
    }
    const std::size_t open = text.find('(');
    const std::size_t close = text.rfind(')');
    if (open == std::string_view::npos || close == std::string_view::npos || close < open) {
        return false;
    }
    std::string body(text.substr(open + 1, close - open - 1));
    std::string parts[4];
    int count = 0;
    int depth = 0;
    std::string current;
    for (char ch : body) {
        if (ch == '(') {
            ++depth;
        } else if (ch == ')') {
            --depth;
        }
        if (ch == ',' && depth == 0) {
            if (count < 4) {
                parts[count++] = current;
            }
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (count < 4 && !current.empty()) {
        parts[count++] = current;
    }
    if (count < 3) {
        return false;
    }
    color.r = Component(parts[0], false);
    color.g = Component(parts[1], false);
    color.b = Component(parts[2], false);
    color.a = count >= 4 || hasAlpha ? Component(count >= 4 ? parts[3] : "1", true) : 1.f;
    return true;
}

}  // namespace

Color Color::rgb8(int red, int green, int blue, int alpha) {
    return {Channel8(red), Channel8(green), Channel8(blue), Channel8(alpha)};
}

Color Color::parse(std::string_view text, bool* ok) {
    const std::string trimmed = trimCopy(text);
    auto finish = [&](bool good, Color color) {
        if (ok != nullptr) {
            *ok = good;
        }
        return color;
    };
    if (trimmed.empty()) {
        return finish(false, Color::transparent());
    }
    if (trimmed[0] == '#') {
        const std::string hex = trimmed.substr(1);
        int r = 0;
        int g = 0;
        int b = 0;
        int a = 255;
        if (hex.size() == 3 || hex.size() == 4) {
            const int rh = Hex(hex[0]);
            const int gh = Hex(hex[1]);
            const int bh = Hex(hex[2]);
            if (rh < 0 || gh < 0 || bh < 0) {
                return finish(false, Color::transparent());
            }
            r = rh * 16 + rh;
            g = gh * 16 + gh;
            b = bh * 16 + bh;
            if (hex.size() == 4) {
                const int ah = Hex(hex[3]);
                if (ah < 0) {
                    return finish(false, Color::transparent());
                }
                a = ah * 16 + ah;
            }
        } else if (hex.size() == 6 || hex.size() == 8) {
            r = HexByte(hex[0], hex[1]);
            g = HexByte(hex[2], hex[3]);
            b = HexByte(hex[4], hex[5]);
            if (r < 0 || g < 0 || b < 0) {
                return finish(false, Color::transparent());
            }
            if (hex.size() == 8) {
                a = HexByte(hex[6], hex[7]);
                if (a < 0) {
                    return finish(false, Color::transparent());
                }
            }
        } else {
            return finish(false, Color::transparent());
        }
        return finish(true, rgb8(r, g, b, a));
    }
    if (lowerCopy(trimmed).rfind("rgb", 0) == 0) {
        Color color;
        if (!ParseRgb(trimmed, color)) {
            return finish(false, Color::transparent());
        }
        return finish(true, color);
    }
    Color named;
    if (Named(trimmed, named)) {
        return finish(true, named);
    }
    return finish(false, Color::transparent());
}

}  // namespace jadefx

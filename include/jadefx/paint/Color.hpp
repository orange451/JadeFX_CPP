#pragma once

#include <string>
#include <string_view>

namespace jadefx {

struct Color {
    float r = 0.f;
    float g = 0.f;
    float b = 0.f;
    float a = 0.f;

    static constexpr Color rgba(float red, float green, float blue, float alpha) {
        return {red, green, blue, alpha};
    }

    // Channels are 0-255. Alpha defaults to opaque.
    static Color rgb8(int red, int green, int blue, int alpha = 255);

    static constexpr Color transparent() { return {0.f, 0.f, 0.f, 0.f}; }
    static constexpr Color white() { return {1.f, 1.f, 1.f, 1.f}; }
    static constexpr Color black() { return {0.f, 0.f, 0.f, 1.f}; }

    // CSS color: #RGB, #RGBA, #RRGGBB, #RRGGBBAA, rgb()/rgba(), or a name such as "white".
    static Color parse(std::string_view text, bool* ok = nullptr);
};

inline Color mix(const Color& from, const Color& to, float t) {
    if (t < 0.f) {
        t = 0.f;
    }
    if (t > 1.f) {
        t = 1.f;
    }
    const float u = 1.f - t;
    return {from.r * u + to.r * t, from.g * u + to.g * t, from.b * u + to.b * t, from.a * u + to.a * t};
}

inline bool near(const Color& a, const Color& b) {
    const float e = 0.5f / 255.f;
    auto close = [e](float x, float y) { return x - y < e && y - x < e; };
    return close(a.r, b.r) && close(a.g, b.g) && close(a.b, b.b) && close(a.a, b.a);
}

}  // namespace jadefx

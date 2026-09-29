#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace jadefx {

// One pixel on an LCD is three vertical stripes, red then green then blue.
// Coverage is rasterized at three samples per pixel and then blurred with a
// FreeType light or default filter (weights / 256). The weights sum to 256, so a
// solid run stays neutral gray; color remains only where the outline crosses a
// stripe. originSubX is the stripe index of samples[0] on that same grid.
struct SubpixelBitmap {
    std::vector<unsigned char> rgb;
    int xoff = 0;
    int yoff = 0;
    int width = 0;
    int height = 0;
};

inline int FloorDiv3(int value) {
    int quot = value / 3;
    if (value < 0 && value % 3 != 0) {
        --quot;
    }
    return quot;
}

// Glyphs are cached at this many horizontal positions per pixel. Skia, and so
// Edge and Chrome, use quarter pixels.
constexpr int kSubpixelPhases = 4;

// Nearest phase of a device-pixel position. pixel is the integer pixel the
// glyph quad should sit on; the glyph sits phase / kSubpixelPhases to its right.
inline int SubpixelPhase(float pixelX, int& pixel) {
    pixel = static_cast<int>(std::floor(pixelX));
    const float fraction = pixelX - static_cast<float>(pixel);
    int phase = static_cast<int>(std::floor(fraction * static_cast<float>(kSubpixelPhases) + 0.5f));
    if (phase >= kSubpixelPhases) {
        phase = 0;
        ++pixel;
    } else if (phase < 0) {
        phase = 0;
    }
    return phase;
}

// FreeType FT_LCD_FILTER_LIGHT. Outer taps are zero; the window is still five
// wide so the default filter shares the same bounds.
constexpr int kLcdFilterLight[5] = {0, 85, 86, 85, 0};
// FreeType FT_LCD_FILTER_DEFAULT, what fontconfig's lcddefault selects on Linux.
constexpr int kLcdFilterDefault[5] = {8, 77, 86, 77, 8};

inline SubpixelBitmap PackSubpixelCoverage(const unsigned char* samples, int sampleWidth, int sampleHeight,
                                           int stride, int originSubX, int originY,
                                           const int (&filter)[5] = kLcdFilterLight) {
    SubpixelBitmap image;
    image.yoff = originY;
    if (samples == nullptr || sampleWidth <= 0 || sampleHeight <= 0 || stride < sampleWidth) {
        return image;
    }

    auto filtered = [&](const unsigned char* row, int index) {
        int sum = 0;
        for (int tap = 0; tap < 5; ++tap) {
            const int sampleIndex = index + tap - 2;
            if (filter[tap] == 0 || sampleIndex < 0 || sampleIndex >= sampleWidth) {
                continue;
            }
            sum += filter[tap] * static_cast<int>(row[sampleIndex]);
        }
        const int value = (sum + 128) >> 8;
        return value > 255 ? 255 : value;
    };

    const int firstSub = originSubX - 2;
    const int lastSub = originSubX + sampleWidth + 1;
    const int x0 = FloorDiv3(firstSub);
    const int x1 = FloorDiv3(lastSub) + 1;
    const int width = x1 - x0;
    if (width <= 0) {
        return image;
    }

    std::vector<unsigned char> rgb(static_cast<std::size_t>(width * sampleHeight * 3), 0);
    for (int y = 0; y < sampleHeight; ++y) {
        const unsigned char* row = samples + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
        for (int x = 0; x < width; ++x) {
            unsigned char* dst = rgb.data() + static_cast<std::size_t>((y * width + x) * 3);
            const int pixel = x0 + x;
            for (int channel = 0; channel < 3; ++channel) {
                dst[channel] = static_cast<unsigned char>(filtered(row, pixel * 3 + channel - originSubX));
            }
        }
    }

    auto columnEmpty = [&](int x) {
        for (int y = 0; y < sampleHeight; ++y) {
            const unsigned char* pixel = rgb.data() + static_cast<std::size_t>((y * width + x) * 3);
            if ((pixel[0] | pixel[1] | pixel[2]) != 0) {
                return false;
            }
        }
        return true;
    };
    int left = 0;
    int right = width;
    while (left < right && columnEmpty(left)) {
        ++left;
    }
    while (right > left && columnEmpty(right - 1)) {
        --right;
    }
    const int trimmed = right - left;
    if (trimmed <= 0) {
        return image;
    }

    image.xoff = x0 + left;
    image.width = trimmed;
    image.height = sampleHeight;
    image.rgb.resize(static_cast<std::size_t>(trimmed * sampleHeight * 3));
    for (int y = 0; y < sampleHeight; ++y) {
        const unsigned char* src = rgb.data() + static_cast<std::size_t>((y * width + left) * 3);
        unsigned char* dst = image.rgb.data() + static_cast<std::size_t>(y * trimmed * 3);
        std::copy(src, src + trimmed * 3, dst);
    }
    return image;
}

}  // namespace jadefx

#include "CopyCursor.hpp"

#include <cstddef>

namespace jadefx {
namespace {

constexpr int kSize = 32;

// B is black, W is white, and anything else is clear.
constexpr const char* kArrow[] = {
    "B",
    "BB",
    "BWB",
    "BWWB",
    "BWWWB",
    "BWWWWB",
    "BWWWWWB",
    "BWWWWWWB",
    "BWWWWWWWB",
    "BWWWWWWWWB",
    "BWWWWWWWWWB",
    "BWWWWWWBBBBB",
    "BWWWBWWB",
    "BWWB BWWB",
    "BWB  BWWB",
    "BB    BWWB",
    "B     BWWB",
    "       BWWB",
    "       BBB",
};

constexpr const char* kPlus[] = {
    "BBBBBBBBBBB",
    "BWWWWWWWWWB",
    "BWWWWBWWWWB",
    "BWWWWBWWWWB",
    "BWWWWBWWWWB",
    "BWBBBBBBBWB",
    "BWWWWBWWWWB",
    "BWWWWBWWWWB",
    "BWWWWBWWWWB",
    "BWWWWWWWWWB",
    "BBBBBBBBBBB",
};

// Where the plus box's top-left corner goes, clear of the arrow.
constexpr int kPlusX = 12;
constexpr int kPlusY = 13;

template <std::size_t Rows>
void Paint(CursorImage& image, const char* const (&rows)[Rows], int left, int top) {
    for (std::size_t row = 0; row < Rows; ++row) {
        for (int column = 0; rows[row][column] != '\0'; ++column) {
            const char cell = rows[row][column];
            if (cell != 'B' && cell != 'W') {
                continue;
            }
            const unsigned char shade = cell == 'W' ? 255 : 0;
            unsigned char* pixel = &image.pixels[((top + static_cast<int>(row)) * image.width + left + column) * 4];
            pixel[0] = shade;
            pixel[1] = shade;
            pixel[2] = shade;
            pixel[3] = 255;
        }
    }
}

}  // namespace

CursorImage CopyCursorImage() {
    CursorImage image;
    image.width = kSize;
    image.height = kSize;
    image.pixels.assign(static_cast<std::size_t>(kSize * kSize * 4), 0);
    Paint(image, kArrow, 0, 0);
    Paint(image, kPlus, kPlusX, kPlusY);
    return image;
}

#if !defined(__APPLE__)
bool ShowSystemCopyCursor() { return false; }
#endif

}  // namespace jadefx

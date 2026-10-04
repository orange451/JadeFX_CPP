#pragma once

#include <algorithm>
#include <optional>

namespace jadefx {

// Where glyph bitmaps go in UiRenderer's square atlas texture: left to right
// in rows, one texel apart so linear filtering does not bleed one into the
// next, a new row under the tallest of the last.
//
// Glyphs are cached per pixel size, so each zoom fills the atlas with another
// set, and one that runs through several zooms fills it whatever its size.
// When a glyph does not fit, overflow says what to do: empty the atlas, since
// most of what it holds is likely not on screen any more; or, when that
// already happened this frame, so this frame's own glyphs do not fit an empty
// atlas, double it; or, at maxSize, give up on the glyph. The caller does the
// GL work and then calls clear with the size it made.
class GlyphAtlasPacker {
public:
    struct Spot {
        int x = 0;
        int y = 0;
    };
    enum class Overflow { Clear, Grow, GiveUp };

    GlyphAtlasPacker(int size, int maxSize) : minSize_(size), maxSize_(std::max(size, maxSize)), size_(size) {}

    int size() const { return size_; }

    // A spot for a width by height bitmap, or none when the atlas has no room left for it.
    std::optional<Spot> place(int width, int height) {
        if (penX_ + width + 1 >= size_) {
            penX_ = 1;
            penY_ += rowHeight_ + 1;
            rowHeight_ = 0;
        }
        if (penY_ + height + 1 >= size_ || width + 2 >= size_) {
            return std::nullopt;
        }
        const Spot spot{penX_, penY_};
        penX_ += width + 1;
        rowHeight_ = std::max(rowHeight_, height);
        return spot;
    }

    // What to do about a glyph place found no room for. Counts toward this frame.
    Overflow overflow() {
        ++overflowsThisFrame_;
        if (overflowsThisFrame_ == 1) {
            return Overflow::Clear;
        }
        return size_ < maxSize_ ? Overflow::Grow : Overflow::GiveUp;
    }

    // Empties it at size, kept between the size it began at and maxSize.
    void clear(int size) {
        size_ = std::clamp(size, minSize_, maxSize_);
        penX_ = 1;
        penY_ = 1;
        rowHeight_ = 0;
    }

    // Each frame's overflows are counted apart.
    void beginFrame() { overflowsThisFrame_ = 0; }

private:
    int minSize_;
    int maxSize_;
    int size_;
    int penX_ = 1;
    int penY_ = 1;
    int rowHeight_ = 0;
    int overflowsThisFrame_ = 0;
};

}  // namespace jadefx

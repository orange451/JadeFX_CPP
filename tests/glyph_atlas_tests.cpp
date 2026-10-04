#include "gl/GlyphAtlasPacker.hpp"

#include <cstdio>

// Where UiRenderer puts each glyph's bitmap in its atlas texture, and what it
// does when one no longer fits: empty the atlas, and grow it when a single
// frame's glyphs do not fit even in an empty one.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

void TestPacksRowsWithAGap() {
    jadefx::GlyphAtlasPacker packer(64, 256);
    const auto first = packer.place(20, 10);
    Expect(first && first->x == 1 && first->y == 1, "the first glyph sits one texel in from the corner");
    const auto second = packer.place(20, 12);
    Expect(second && second->x == 22 && second->y == 1, "the next sits right of it, one texel apart");
    const auto third = packer.place(30, 5);
    Expect(third && third->x == 1 && third->y == 14, "one too wide for the row starts a row under the tallest");
}

void TestFullReportsNoSpot() {
    jadefx::GlyphAtlasPacker packer(32, 32);
    Expect(packer.place(28, 28).has_value(), "a glyph that fits with a texel around it is placed");
    Expect(!packer.place(28, 28).has_value(), "one that does not is not");
    Expect(!packer.place(40, 4).has_value(), "nor one wider than the atlas");
}

void TestOverflowClearsThenGrows() {
    jadefx::GlyphAtlasPacker packer(64, 256);
    packer.beginFrame();
    Expect(packer.overflow() == jadefx::GlyphAtlasPacker::Overflow::Clear,
           "the first overflow in a frame empties the atlas: the old glyphs may be off screen now");
    packer.clear(packer.size());
    Expect(packer.place(20, 10) && packer.place(20, 10)->x == 22, "an emptied atlas packs from the corner again");
    Expect(packer.overflow() == jadefx::GlyphAtlasPacker::Overflow::Grow,
           "a second overflow in the same frame grows it: this frame's glyphs alone do not fit");
    packer.clear(packer.size() * 2);
    Expect(packer.size() == 128, "growing doubles it");
    Expect(packer.overflow() == jadefx::GlyphAtlasPacker::Overflow::Grow, "and again while there is room");
    packer.clear(packer.size() * 2);
    Expect(packer.size() == 256, "up to the most it may be");
    Expect(packer.overflow() == jadefx::GlyphAtlasPacker::Overflow::GiveUp,
           "at the most, an overflow gives up on the glyph instead of clearing again");

    packer.beginFrame();
    Expect(packer.overflow() == jadefx::GlyphAtlasPacker::Overflow::Clear,
           "a new frame starts counting again, so a later overflow only empties it");
}

void TestClearKeepsSizeWithinBounds() {
    jadefx::GlyphAtlasPacker packer(64, 128);
    packer.clear(1024);
    Expect(packer.size() == 128, "clear never makes it bigger than the most");
    packer.clear(8);
    Expect(packer.size() == 64, "nor smaller than it began");
}

}  // namespace

int RunGlyphAtlasTests() {
    gFailures = 0;
    TestPacksRowsWithAGap();
    TestFullReportsNoSpot();
    TestOverflowClearsThenGrows();
    TestClearKeepsSizeWithinBounds();
    return gFailures;
}

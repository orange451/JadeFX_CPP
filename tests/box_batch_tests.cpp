#include "gl/BoxBatch.hpp"

#include <cstdio>

// The records UiRenderer streams to box.vert, one a box. Each field holds what
// drawBox set as the uniform of the same name before boxes were batched.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

bool Same(const float field[4], float a, float b, float c, float d) {
    return field[0] == a && field[1] == b && field[2] == c && field[3] == d;
}

const float kRadius[4] = {1.f, 2.f, 3.f, 4.f};

void TestFillScalesEveryField() {
    const jadefx::Color color = jadefx::Color::rgba(0.1f, 0.2f, 0.3f, 0.4f);
    const jadefx::BoxInstance box = jadefx::MakeBoxInstance(2.f, 10.f, 20.f, 30.f, 40.f, 0.f, 0.f, 30.f, 40.f, kRadius,
                                                            color, 0.f, nullptr, 0.f, 45.f, nullptr, nullptr, false);
    Expect(Same(box.rect, 20.f, 40.f, 60.f, 80.f), "rect is in device pixels");
    Expect(Same(box.box, 0.f, 0.f, 60.f, 80.f), "so is the shape inside it");
    Expect(Same(box.radii, 2.f, 4.f, 6.f, 8.f), "and the radii");
    Expect(Same(box.params, 0.f, 0.f, 0.f, 45.f), "a soft fill keeps its mode and angle");
    Expect(Same(box.border, 0.f, 0.f, 0.f, 0.f), "no sides is no border");
    Expect(Same(box.clip, 0.f, 0.f, 0.f, 0.f), "no clip is zeros");
    Expect(Same(box.clipRadii, 0.f, 0.f, 0.f, 0.f), "no clip radii are zeros");
    Expect(Same(box.color, 0.1f, 0.2f, 0.3f, 0.4f), "the color is straight alpha, not scaled");
}

void TestExactEdgesAtFractionalScale() {
    // What fillRect passes at 1.5 pixels a point: a pixel of padding on each side.
    const float s = 1.5f;
    const float pad = 1.f / s;
    const float x = 10.f - pad;
    const float y = 7.25f - pad;
    const float width = 33.3f + 2.f * pad;
    const float height = 0.6f + 2.f * pad;
    const float none[4] = {};
    const jadefx::BoxInstance box =
        jadefx::MakeBoxInstance(s, x, y, width, height, pad, pad, 33.3f, 0.6f, none, jadefx::Color::rgba(0, 0, 0, 1),
                                0.f, nullptr, 0.f, 0.f, nullptr, nullptr, true);
    Expect(Same(box.rect, x * s, y * s, width * s, height * s), "the padded quad scales as drawBox scaled it");
    Expect(Same(box.box, pad * s, pad * s, 33.3f * s, 0.6f * s), "the box inside it too");
    Expect(Same(box.params, 0.f, 1.f, 0.f, 0.f), "exact edges are flagged");
}

void TestBorderScalesSides() {
    const float sides[4] = {1.f, 2.f, 3.f, 0.5f};
    const jadefx::BoxInstance box =
        jadefx::MakeBoxInstance(2.f, 0.f, 0.f, 10.f, 10.f, 0.f, 0.f, 10.f, 10.f, kRadius,
                                jadefx::Color::rgba(1, 0, 0, 1), 1.f, sides, 0.f, 0.f, nullptr, nullptr, false);
    Expect(Same(box.border, 2.f, 4.f, 6.f, 1.f), "sides are top, right, bottom, left, in device pixels");
    Expect(box.params[0] == 1.f, "mode 1 is a border ring");
}

void TestShadowCarriesBlurAndClip() {
    const float clip[4] = {1.f, 2.f, 3.f, 4.f};
    const float clipRadii[4] = {5.f, 6.f, 7.f, 8.f};
    const jadefx::BoxInstance outer =
        jadefx::MakeBoxInstance(2.f, 0.f, 0.f, 10.f, 10.f, 1.f, 1.f, 8.f, 8.f, kRadius,
                                jadefx::Color::rgba(0, 0, 0, 0.5f), 2.f, nullptr, 3.f, 0.f, clip, clipRadii, false);
    Expect(Same(outer.params, 2.f, 0.f, 6.f, 0.f), "the blur radius is in device pixels");
    Expect(Same(outer.clip, 2.f, 4.f, 6.f, 8.f), "the clip too");
    Expect(Same(outer.clipRadii, 10.f, 12.f, 14.f, 16.f), "and its radii");
    const jadefx::BoxInstance inner =
        jadefx::MakeBoxInstance(2.f, 0.f, 0.f, 10.f, 10.f, 0.f, 0.f, 10.f, 10.f, kRadius,
                                jadefx::Color::rgba(0, 0, 0, 0.5f), 3.f, nullptr, -1.f, 0.f, clip, clipRadii, false);
    Expect(inner.params[0] == 3.f && inner.params[2] == 0.f, "an inset shadow's negative blur is none");
}

void TestRunKeepsOrderAndStopsAtTheLimit() {
    jadefx::BoxBatch batch;
    Expect(batch.empty() && batch.size() == 0, "a new run is empty");
    jadefx::BoxInstance first{};
    first.rect[0] = 1.f;
    jadefx::BoxInstance second{};
    second.rect[0] = 2.f;
    batch.add(first);
    batch.add(second);
    Expect(batch.size() == 2 && batch.data()[0].rect[0] == 1.f && batch.data()[1].rect[0] == 2.f,
           "boxes stay in the order they were drawn");
    bool all = true;
    for (int i = 2; i < jadefx::BoxBatch::kMaxRun; ++i) {
        all = batch.add(first) && all;
    }
    Expect(all && batch.size() == jadefx::BoxBatch::kMaxRun, "a run holds kMaxRun boxes");
    Expect(!batch.add(second), "one more is refused, so the caller draws the run and starts another");
    Expect(batch.size() == jadefx::BoxBatch::kMaxRun, "and the refused box is not kept");
    batch.clear();
    Expect(batch.empty() && batch.size() == 0, "clear empties the run");
    Expect(batch.add(second) && batch.data()[0].rect[0] == 2.f, "and it fills again from the start");
}

}  // namespace

int RunBoxBatchTests() {
    gFailures = 0;
    TestFillScalesEveryField();
    TestExactEdgesAtFractionalScale();
    TestBorderScalesSides();
    TestShadowCarriesBlurAndClip();
    TestRunKeepsOrderAndStopsAtTheLimit();
    return gFailures;
}

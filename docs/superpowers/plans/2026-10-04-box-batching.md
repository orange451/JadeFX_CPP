# Box Batching Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Draw each run of consecutive one-color boxes with one instanced GL call, with byte-identical pixels.

**Architecture:** A GL-free `BoxBatch` builds 128-byte instance records from `drawBox`'s arguments and holds the pending run. `UiRenderer` streams each run into an instance buffer and draws it with `glDrawArraysInstanced`. It draws the pending run (flushes) before any other draw, clip change, screenshot, raw-GL callback, or `end()`. A gradient is drawn as a run of one, with its stops still set as uniforms. Nodes that issue their own GL opt in with `Node::setDrawsRawGl(true)`, and `Painter::flush()` covers the mixed case.

**Tech Stack:** C++17 (Xcode 13 / libc++ 13: nothing newer), OpenGL 3.3 core / 4.1 on macOS, GLES 3.0 shaders through `Preamble`, GLFW, CMake. Tests are plain `Expect` functions in `jadefx-tests`.

**Spec:** `docs/superpowers/specs/2026-10-04-box-batching-design.md`

## Global Constraints

- Nothing on screen changes: every render-check PPM and every Anarchy `assets-demo` PNG is byte-identical to the baseline captured in Task 2.
- Instancing only: `glVertexAttribDivisor` 1 and `glDrawArraysInstanced`, core in GL 3.3 and GLES 3.0. No extensions and no fallback. Shader files carry no `#version`; `Preamble` adds it.
- Instance record: `rect`, `box`, `radii`, `params`, `border`, `clip`, `clipRadii`, `color`, eight vec4s = 128 bytes, attribute locations 1–8. Location 0 stays the unit quad's corner. `uViewport` stays a uniform.
- A run is at most 4,096 boxes. The instance buffer is 4,096 × 128 B = 512 KB.
- Gradients (2–8 stops) keep their stops in uniforms (`uStops`, `uStopAt`, `uStopCount`) through `UniformCache`, with `uGradient` 1.
- Raw GL in a paint must leave the viewport, scissor, and blend enable as it found them. Every flush binds the box program, its VAO, the instance buffer, and the blend function itself.
- In the Anarchy studio, any GL error is fatal.
- Never commit on Anarchy's `main` checkout. Anarchy changes go on branch `box-batching` in its own worktree.
- Mobile (GLFM/GLES) builds are neither built nor run. macOS only.
- Comments are plain sentences, matching the density of the code around them.

## Departures from the spec (record them in the spec in Task 6)

1. **The buffer never grows.** Runs are capped at 4,096 and a run that would pass the end orphans the store and starts at 0. A fixed 4,096-box store therefore serves any frame, and doubling buys nothing.
2. **Two more flush points:** `UiRenderer::writePpm` (`Stage::frame` calls it before `end()` for `JADEFX_DUMP_PPM`), and `Stage::frame` before the `setRenderingCallback` callback.
3. **Anarchy's MaterialBall opt-in is on `IdeAssets`**, the node whose `renderContent` draws the balls. MaterialBall is not a node.
4. **The Rainbow-Triangle demo needs no opt-in.** It draws its GL before `stage->frame`, not inside a paint.

## Variables used below

- `JFX`: the JadeFX worktree this plan runs in, on branch `box-batching`.
- `ANA`: `/Users/yaoli/Documents/AnarchyEngine-CPP-box-batching`, Anarchy's worktree on branch `box-batching`, made in Task 2.
- `OUT`: `/private/tmp/claude-501/-Users-yaoli-Documents-JadeFX-CPP/85effdeb-f9a5-4b6a-9f33-4f1ce6ada603/scratchpad/box-batching`, which holds baselines and results.

## Review Focus

1. **A gradient between solid boxes:** the box after it must be solid, not take the gradient's stops (`uGradient` back to 0). Covered by render-check `gradient`, which draws a fill and a border right after gradients (Task 2).
2. **More boxes than one store, and frame after frame:** the store must wrap and orphan, and the second frame must not inherit state from the first. Covered by render-check `many` (5,200 boxes), with every scene drawn twice and the second frame saved (Task 2).
3. **A screenshot taken before `end()`**, as `JADEFX_DUMP_PPM` takes one: it must contain the queued boxes. Covered by render-check `solid-before-end` (Task 2), which `writePpm` flushing makes pass (Task 3).
4. **A `setRenderingCallback` overlay drawn over the UI** must land on top. Covered by the render-check `rendering callback` check (Task 3).
5. **A box queued just before `pushClip`** that crosses the clip's edge must be drawn unclipped. Covered by render-check `clips` (Task 2).

---

### Task 1: BoxBatch and its unit tests

**Files:**
- Create: `src/gl/BoxBatch.hpp`, `src/gl/BoxBatch.cpp`, `tests/box_batch_tests.cpp`
- Modify: `CMakeLists.txt` (`JADEFX_SOURCES` and the `jadefx-tests` sources), `tests/layout_tests.cpp` (declare and run `RunBoxBatchTests`)

**Interfaces:**
- Produces: `struct jadefx::BoxInstance` (8 × `float[4]`: `rect, box, radii, params, border, clip, clipRadii, color`, 128 bytes); `BoxInstance jadefx::MakeBoxInstance(float scale, float x, float y, float width, float height, float boxX, float boxY, float boxW, float boxH, const float radius[4], const Color& color, float mode, const float sides[4], float blur, float angleDeg, const float* clip, const float* clipRadii, bool exact)`; `class jadefx::BoxBatch { static constexpr int kMaxRun = 4096; bool add(const BoxInstance&); int size() const; bool empty() const; const BoxInstance* data() const; void clear(); }`

- [ ] **Step 1: Write the failing test** `tests/box_batch_tests.cpp`:

```cpp
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
```

Register it. In `tests/layout_tests.cpp`, add `int RunBoxBatchTests();` after `int RunGlyphAtlasTests();`, and add `gFailures += RunBoxBatchTests();` after `gFailures += RunGlyphAtlasTests();`. In `CMakeLists.txt`, add `tests/box_batch_tests.cpp` after `tests/glyph_atlas_tests.cpp` in `jadefx-tests`.

- [ ] **Step 2: Run it to see it fail**

Run: `cmake -S $JFX -B $JFX/build && cmake --build $JFX/build --target jadefx-tests --parallel`
Expected: the build fails with `'gl/BoxBatch.hpp' file not found`.

- [ ] **Step 3: Write `src/gl/BoxBatch.hpp`**

```cpp
#pragma once

#include "jadefx/paint/Color.hpp"

#include <vector>

namespace jadefx {

// One box as box.vert reads it, in device pixels. Each field is one vec4
// attribute, in attribute order from location 1.
struct BoxInstance {
    float rect[4];       // the quad drawn: x, y, width, height
    float box[4];        // the shape, in the quad's own pixels
    float radii[4];      // top left, top right, bottom right, bottom left
    float params[4];     // mode, exact edges, blur radius, gradient angle in degrees
    float border[4];     // top, right, bottom, left
    float clip[4];       // a shadow's element, in the quad's own pixels
    float clipRadii[4];
    float color[4];      // straight-alpha RGBA
};
static_assert(sizeof(BoxInstance) == 128, "box.vert reads eight vec4s a box");

// The record for UiRenderer::drawBox's arguments at scale device pixels a point.
// sides, clip, and clipRadii may be null, for zeros.
BoxInstance MakeBoxInstance(float scale, float x, float y, float width, float height, float boxX, float boxY,
                            float boxW, float boxH, const float radius[4], const Color& color, float mode,
                            const float sides[4], float blur, float angleDeg, const float* clip,
                            const float* clipRadii, bool exact);

// Boxes waiting to be drawn together, in the order they were drawn.
class BoxBatch {
public:
    // The most boxes one run holds, which is what the instance buffer holds.
    static constexpr int kMaxRun = 4096;

    BoxBatch() { boxes_.reserve(kMaxRun); }

    // False, keeping nothing, when the run is full: the caller draws it and adds again.
    bool add(const BoxInstance& box) {
        if (size() >= kMaxRun) {
            return false;
        }
        boxes_.push_back(box);
        return true;
    }
    int size() const { return static_cast<int>(boxes_.size()); }
    bool empty() const { return boxes_.empty(); }
    const BoxInstance* data() const { return boxes_.data(); }
    void clear() { boxes_.clear(); }

private:
    std::vector<BoxInstance> boxes_;
};

}  // namespace jadefx
```

- [ ] **Step 4: Write `src/gl/BoxBatch.cpp`**, the same arithmetic `drawBox` uses today:

```cpp
#include "BoxBatch.hpp"

#include <algorithm>

namespace jadefx {
namespace {

void Set(float field[4], float a, float b, float c, float d) {
    field[0] = a;
    field[1] = b;
    field[2] = c;
    field[3] = d;
}

}  // namespace

BoxInstance MakeBoxInstance(float scale, float x, float y, float width, float height, float boxX, float boxY,
                            float boxW, float boxH, const float radius[4], const Color& color, float mode,
                            const float sides[4], float blur, float angleDeg, const float* clip,
                            const float* clipRadii, bool exact) {
    const float s = scale;
    BoxInstance box{};
    Set(box.rect, x * s, y * s, width * s, height * s);
    Set(box.box, boxX * s, boxY * s, boxW * s, boxH * s);
    Set(box.radii, radius[0] * s, radius[1] * s, radius[2] * s, radius[3] * s);
    Set(box.params, mode, exact ? 1.f : 0.f, std::max(blur * s, 0.f), angleDeg);
    if (sides != nullptr) {
        Set(box.border, sides[0] * s, sides[1] * s, sides[2] * s, sides[3] * s);
    }
    if (clip != nullptr) {
        Set(box.clip, clip[0] * s, clip[1] * s, clip[2] * s, clip[3] * s);
    }
    if (clipRadii != nullptr) {
        Set(box.clipRadii, clipRadii[0] * s, clipRadii[1] * s, clipRadii[2] * s, clipRadii[3] * s);
    }
    Set(box.color, color.r, color.g, color.b, color.a);
    return box;
}

}  // namespace jadefx
```

Add `src/gl/BoxBatch.cpp` after `src/gl/UiRenderer.cpp` in `JADEFX_SOURCES`.

- [ ] **Step 5: Run the tests to see them pass**

Run: `cmake --build $JFX/build --target jadefx-tests --parallel && $JFX/build/jadefx-tests`
Expected: exit 0, no `FAIL` lines.

- [ ] **Step 6: Commit**

```bash
git -C $JFX add src/gl/BoxBatch.hpp src/gl/BoxBatch.cpp tests/box_batch_tests.cpp tests/layout_tests.cpp CMakeLists.txt
git -C $JFX commit -m "Add BoxBatch, the per-box records a batched box draw streams"
```

---

### Task 2: render-check and the before baselines

No change to the renderer. This adds the program and captures every "before" picture and timing while the renderer still draws one box at a time.

**Files:**
- Create: `tests/render_check.cpp`
- Modify: `CMakeLists.txt` (new `jadefx-render-check` target inside `if(NOT JADEFX_GLFM)`, after `jadefx-tests`)

**Interfaces:**
- Consumes: `UiRenderer` public API as it is on `master`.
- Produces: `jadefx-render-check <out-dir>` writes `<scene>@<scale>x.ppm` for scenes `solid rounded border shadow gradient mixed clips many` at scales 1, 1.5, 2, plus `solid-before-end@1x.ppm`. It returns 0 when every write and check passes. Later tasks add checks to `RunChecks(UiRenderer&, int fbW, int fbH)`. The helpers `GetProc`, `PixelAt`, `IsBlue`, `IsRed`, `ClearSquareBlue`, and `Report` are defined here for them.

- [ ] **Step 1: Write `tests/render_check.cpp`**

```cpp
// By hand, not ctest: jadefx-render-check out-dir draws fixed scenes with
// UiRenderer in a hidden window and writes each as out-dir/<scene>@<scale>x.ppm,
// for a byte comparison against the same program built before a renderer
// change. Shaders are read from the source tree. Then it runs the checks that
// read pixels back, and exits 1 if any write or check failed.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "gl/UiRenderer.hpp"
#include "gl/gl.hpp"
#include "jadefx/jadefx.hpp"
#include "scene/image/ImageData.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <string>

namespace {

using jadefx::Color;
using jadefx::UiRenderer;

// The window in points. The framebuffer is whatever the display makes of it.
constexpr int kWidth = 480;
constexpr int kHeight = 360;

void* GetProc(const char* name) { return reinterpret_cast<void*>(glfwGetProcAddress(name)); }

Color Rgba(float r, float g, float b, float a) { return Color::rgba(r, g, b, a); }

void Solid(UiRenderer& r) {
    // A grid of cells that share edges, as a table draws them.
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 12; ++col) {
            const float shade = static_cast<float>((row * 12 + col) % 7) / 7.f;
            r.fillRect(10.f + col * 17.5f, 10.f + row * 13.25f, 17.5f, 13.25f, Rgba(shade, 0.4f, 1.f - shade, 1.f));
        }
    }
    // Translucent boxes over each other: their order shows in the blend.
    r.fillRect(40.f, 60.f, 120.f, 80.f, Rgba(1.f, 0.f, 0.f, 0.5f));
    r.fillRect(90.f, 90.f, 120.f, 80.f, Rgba(0.f, 0.f, 1.f, 0.5f));
    r.fillRect(250.3f, 20.7f, 33.4f, 0.6f, Rgba(0.f, 0.f, 0.f, 1.f));
}

void Rounded(UiRenderer& r) {
    const float radii[4][4] = {{0.f, 0.f, 0.f, 0.f}, {6.f, 6.f, 6.f, 6.f}, {20.f, 4.f, 12.f, 0.f}, {40.f, 40.f, 40.f, 40.f}};
    for (int i = 0; i < 4; ++i) {
        const Color fill = Rgba(0.2f + 0.2f * i, 0.5f, 0.3f, 0.8f);
        r.fillRounded(20.f + 110.f * i, 30.f, 90.f, 70.f, radii[i], &fill, nullptr, 1, 0.f);
    }
    const Color glass = Rgba(1.f, 1.f, 1.f, 0.35f);
    const float pill[4] = {15.f, 15.f, 15.f, 15.f};
    r.fillRounded(60.f, 80.f, 300.f, 30.f, pill, &glass, nullptr, 1, 0.f);
}

void Border(UiRenderer& r) {
    const float radius[4] = {8.f, 8.f, 8.f, 8.f};
    const float sharp[4] = {};
    const float even[4] = {1.f, 1.f, 1.f, 1.f};
    const float uneven[4] = {1.f, 3.f, 6.f, 0.5f};
    r.strokeRounded(20.f, 20.f, 140.f, 90.f, radius, even, Rgba(0.f, 0.f, 0.f, 1.f));
    r.strokeRounded(180.f, 20.f, 140.f, 90.f, radius, uneven, Rgba(0.8f, 0.1f, 0.1f, 0.9f));
    r.strokeRounded(20.f, 130.f, 140.f, 90.f, sharp, uneven, Rgba(0.1f, 0.3f, 0.8f, 1.f));
    r.strokeRounded(180.5f, 130.25f, 140.f, 90.f, radius, even, Rgba(0.f, 0.5f, 0.f, 0.5f));
}

void Shadow(UiRenderer& r) {
    const float radius[4] = {10.f, 10.f, 10.f, 10.f};
    const float sharp[4] = {};
    const Color card = Rgba(1.f, 1.f, 1.f, 1.f);
    r.outerShadow(40.f, 40.f, 140.f, 100.f, radius, 0.f, 4.f, 12.f, 0.f, Rgba(0.f, 0.f, 0.f, 0.35f));
    r.fillRounded(40.f, 40.f, 140.f, 100.f, radius, &card, nullptr, 1, 0.f);
    r.outerShadow(240.f, 40.f, 140.f, 100.f, sharp, -3.f, 2.f, 0.f, 4.f, Rgba(0.2f, 0.f, 0.4f, 0.6f));
    r.innerShadow(40.f, 180.f, 140.f, 100.f, radius, 2.f, 2.f, 8.f, 1.f, Rgba(0.f, 0.f, 0.f, 0.5f));
    r.innerShadow(240.f, 180.f, 140.f, 100.f, sharp, 0.f, 0.f, 20.f, -2.f, Rgba(0.9f, 0.2f, 0.f, 0.7f));
}

void Gradient(UiRenderer& r) {
    const float radius[4] = {12.f, 12.f, 12.f, 12.f};
    const Color two[2] = {Rgba(1.f, 0.f, 0.f, 1.f), Rgba(0.f, 0.f, 1.f, 1.f)};
    const float twoAt[2] = {0.f, 1.f};
    const Color three[3] = {Rgba(1.f, 1.f, 0.f, 1.f), Rgba(0.f, 1.f, 0.f, 0.5f), Rgba(0.f, 0.f, 0.f, 1.f)};
    const float threeAt[3] = {0.f, 0.3f, 1.f};
    Color eight[8];
    float eightAt[8];
    for (int i = 0; i < 8; ++i) {
        eight[i] = Rgba(i / 7.f, 1.f - i / 7.f, (i % 2) * 1.f, 1.f);
        eightAt[i] = i / 7.f;
    }
    r.fillRect(10.f, 10.f, 100.f, 60.f, Rgba(0.f, 0.6f, 0.f, 1.f));
    r.fillRounded(30.f, 30.f, 160.f, 90.f, radius, two, twoAt, 2, 90.f);
    // Solid right after a gradient: it must not take the gradient's stops.
    r.fillRect(60.f, 60.f, 100.f, 60.f, Rgba(1.f, 1.f, 0.f, 0.7f));
    r.fillRounded(220.f, 30.f, 160.f, 90.f, radius, three, threeAt, 3, 30.f);
    r.fillRounded(30.f, 160.f, 350.f, 60.f, radius, eight, eightAt, 8, 0.f);
    const float ring[4] = {2.f, 2.f, 2.f, 2.f};
    r.strokeRounded(30.f, 160.f, 350.f, 60.f, radius, ring, Rgba(0.f, 0.f, 0.f, 1.f));
}

std::shared_ptr<jadefx::ImageData> Checker() {
    auto image = std::make_shared<jadefx::ImageData>();
    image->width = 8;
    image->height = 8;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const bool on = (x + y) % 2 == 0;
            const unsigned char alpha = static_cast<unsigned char>(128 + 16 * x);
            image->rgba.insert(image->rgba.end(), {static_cast<unsigned char>(on ? 255 : 0), 64,
                                                   static_cast<unsigned char>(on ? 0 : 255), alpha});
        }
    }
    return image;
}

void Mixed(UiRenderer& r) {
    static const std::shared_ptr<jadefx::ImageData> image = Checker();
    r.fillRect(10.f, 10.f, 300.f, 40.f, Rgba(0.9f, 0.9f, 0.6f, 1.f));
    r.text(16.f, 18.f, "Boxes under and over text", "Open Sans", 16.f, Rgba(0.f, 0.f, 0.f, 1.f), true);
    r.fillRect(100.f, 20.f, 60.f, 20.f, Rgba(0.f, 0.f, 1.f, 0.4f));
    r.drawImage(image, 20.f, 70.f, 64.f, 64.f, 1.f);
    r.fillRect(50.f, 100.f, 80.f, 20.f, Rgba(1.f, 0.f, 0.f, 0.5f));
    const Color tint = Rgba(0.f, 0.5f, 0.f, 1.f);
    r.drawImage(image, 120.f, 70.f, 64.f, 64.f, 0.6f, &tint);
    r.fillRect(110.f, 60.f, 10.f, 90.f, Rgba(0.f, 0.f, 0.f, 1.f));
    r.text(200.f, 80.f, "grayscale", "Open Sans", 14.f, Rgba(0.2f, 0.2f, 0.2f, 1.f), false);
    r.fillRect(195.f, 85.f, 90.f, 6.f, Rgba(1.f, 0.f, 1.f, 0.5f));
}

void Clips(UiRenderer& r) {
    // Queued before the clip and crossing its edge: drawn whole.
    r.fillRect(0.f, 0.f, 200.f, 200.f, Rgba(0.8f, 0.8f, 1.f, 1.f));
    r.pushClip(50.f, 50.f, 200.f, 150.f);
    r.fillRect(0.f, 0.f, 400.f, 400.f, Rgba(1.f, 0.8f, 0.8f, 1.f));
    r.pushClip(100.5f, 80.5f, 300.f, 60.f);
    r.fillRect(0.f, 0.f, 400.f, 400.f, Rgba(0.2f, 0.6f, 0.2f, 0.6f));
    r.text(90.f, 90.f, "clipped twice", "Open Sans", 18.f, Rgba(0.f, 0.f, 0.f, 1.f), true);
    r.popClip();
    r.fillRect(40.f, 170.f, 300.f, 20.f, Rgba(0.f, 0.f, 0.f, 0.5f));
    r.popClip();
    r.fillRect(300.f, 250.f, 60.f, 60.f, Rgba(0.f, 0.f, 0.f, 1.f));
}

void Many(UiRenderer& r) {
    // 5,200 boxes with nothing between them: more than one run holds.
    for (int i = 0; i < 5200; ++i) {
        const int col = i % 80;
        const int row = i / 80;
        r.fillRect(4.f + col * 5.5f, 4.f + row * 5.25f, 5.f, 4.75f,
                   Rgba((col % 5) / 5.f, (row % 7) / 7.f, 0.5f, 0.9f));
    }
}

struct SceneEntry {
    const char* name;
    void (*draw)(UiRenderer&);
};

const SceneEntry kScenes[] = {
    {"solid", Solid}, {"rounded", Rounded}, {"border", Border}, {"shadow", Shadow},
    {"gradient", Gradient}, {"mixed", Mixed}, {"clips", Clips}, {"many", Many},
};

const Color kWhite = Color::rgba(1.f, 1.f, 1.f, 1.f);

int Write(UiRenderer& renderer, const std::string& outDir, const char* name, float scale) {
    char path[1024];
    std::snprintf(path, sizeof path, "%s/%s@%gx.ppm", outDir.c_str(), name, static_cast<double>(scale));
    if (!renderer.writePpm(path)) {
        std::printf("FAIL could not write %s\n", path);
        return 1;
    }
    std::printf("wrote %s\n", path);
    return 0;
}

int WriteScenes(UiRenderer& renderer, int fbW, int fbH, const std::string& outDir) {
    const float scales[] = {1.f, 1.5f, 2.f};
    int failures = 0;
    for (const SceneEntry& scene : kScenes) {
        for (float scale : scales) {
            // Twice: the second frame starts from whatever state the first left.
            for (int frame = 0; frame < 2; ++frame) {
                renderer.begin(fbW, fbH, scale, kWhite, true);
                scene.draw(renderer);
                renderer.end();
            }
            failures += Write(renderer, outDir, scene.name, scale);
        }
    }
    // A screenshot taken before end, as JADEFX_DUMP_PPM takes one.
    renderer.begin(fbW, fbH, 1.f, kWhite, true);
    Solid(renderer);
    failures += Write(renderer, outDir, "solid-before-end", 1.f);
    renderer.end();
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::printf("FAIL OpenGL error 0x%x\n", error);
        ++failures;
    }
    return failures;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string outDir = argc > 1 ? argv[1] : ".";
    if (!glfwInit()) {
        std::fprintf(stderr, "glfwInit failed\n");
        return 1;
    }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(kWidth, kHeight, "jadefx-render-check", nullptr, nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "No GL window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    if (!jadefx_load_gl(&GetProc)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    jadefx::Font::loadDefault();
    int fbW = 0;
    int fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    std::printf("framebuffer %dx%d\n", fbW, fbH);

    int failures = 0;
    {
        UiRenderer renderer;
        if (!renderer.initialize()) {
            std::fprintf(stderr, "UiRenderer did not start\n");
            failures = 1;
        } else {
            failures += WriteScenes(renderer, fbW, fbH, outDir);
            renderer.shutdown();
        }
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    if (failures == 0) {
        std::printf("render-check: all passed\n");
        return 0;
    }
    std::printf("render-check: %d failed\n", failures);
    return 1;
}
```

`<array>`, `PixelAt`, and the check helpers come in Task 3. Leave `<array>` in now; it is harmless.

- [ ] **Step 2: Add the target** in `CMakeLists.txt`, inside `if(NOT JADEFX_GLFM)`, after `jadefx_warnings(jadefx-tests)`:

```cmake
    # By hand, not ctest: jadefx-render-check out-dir draws fixed scenes in a
    # hidden window and writes each as a PPM, for a byte comparison across a
    # renderer change, then runs the checks that read pixels back.
    add_executable(jadefx-render-check tests/render_check.cpp)
    target_include_directories(jadefx-render-check PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(jadefx-render-check PRIVATE jadefx)
    jadefx_warnings(jadefx-render-check)
```

- [ ] **Step 3: Build and capture the JadeFX baseline**

```bash
cmake -S $JFX -B $JFX/build && cmake --build $JFX/build --target jadefx-render-check --parallel
mkdir -p $OUT/render-before && cd $JFX && build/jadefx-render-check $OUT/render-before
```
Expected: 25 `wrote` lines, then `render-check: all passed`. Open `$OUT/render-before/mixed@2x.ppm` and `many@1x.ppm` (convert with `sips -s format png <f> --out <f>.png` to view) and confirm they show the scenes, not a blank or garbage frame. If the hidden window reads back blank, stop and report it: the byte comparison is worthless without real pictures.

Then check it is deterministic: run it again into `$OUT/render-before-2` and `cmp` every file against `render-before`. All must match.

- [ ] **Step 4: Make Anarchy's worktree and capture its baselines**

```bash
git -C /Users/yaoli/Documents/AnarchyEngine-CPP worktree add /Users/yaoli/Documents/AnarchyEngine-CPP-box-batching -b box-batching
cmake -S $ANA -B $ANA/build-batching -DCMAKE_BUILD_TYPE=Release -DJADEFX_CPP_DIR=$JFX
cmake --build $ANA/build-batching --parallel
cd $ANA && build-batching/assets-demo $OUT/assets-before/light light && build-batching/assets-demo $OUT/assets-before/dark dark
cd $ANA && build-batching/profiler-demo $OUT/profiler-before ~/Documents/AnarchyEngineProjects/CannonVsPigs
```
Expected: `assets-demo` saves 16 PNGs in all. Run it a second time into `$OUT/assets-before-2` and `cmp` all files: any that differ run to run are not deterministic, so list them in `$OUT/notes.txt` and leave them out of later comparisons. From the profiler-demo Scopes PNG, read the average ms for **Profiler overlay** and **Scene View** and write both into `$OUT/timings.txt` as `before`. They should be near 1.45 ms and 3.13 ms.

- [ ] **Step 5: Commit** (JadeFX only; nothing changed in Anarchy)

```bash
git -C $JFX add tests/render_check.cpp CMakeLists.txt
git -C $JFX commit -m "Add render-check, which writes fixed UiRenderer scenes as PPMs for byte comparison"
```

---

### Task 3: Instanced box runs in UiRenderer

**Files:**
- Modify: `src/gl/gl.hpp`, `src/gl/gl.cpp`, `res/shaders/box.vert`, `res/shaders/box.frag`, `src/gl/UiRenderer.hpp`, `src/gl/UiRenderer.cpp`, `src/stage/Stage.cpp`, `tests/render_check.cpp`

**Interfaces:**
- Consumes: `BoxInstance`, `MakeBoxInstance`, `BoxBatch` (Task 1); render-check (Task 2).
- Produces: `void UiRenderer::flush()` (public; draws the pending run); `bool UiRenderer::writePpm(const char*)` (no longer `const`; flushes first); in render-check, `int RunChecks(UiRenderer& renderer, int fbW, int fbH)` plus helpers `PixelAt(int x, int y) -> std::array<unsigned char, 4>`, `IsBlue`, `IsRed`, `ClearSquareBlue(int left, int bottom, int size)`, `Report(bool ok, const char* what) -> int`.

- [ ] **Step 1: Write the rendering-callback check first** (it passes on today's renderer, and must still pass after). In `tests/render_check.cpp`, add inside the anonymous namespace after `WriteScenes`:

```cpp
// The framebuffer's RGBA at x, y, counted from the bottom left as GL counts.
std::array<unsigned char, 4> PixelAt(int x, int y) {
    std::array<unsigned char, 4> pixel{};
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    return pixel;
}

bool IsBlue(const std::array<unsigned char, 4>& p) { return p[0] < 10 && p[1] < 10 && p[2] > 245; }
bool IsRed(const std::array<unsigned char, 4>& p) { return p[0] > 245 && p[1] < 10 && p[2] < 10; }

// GL of a program's own: clears a square to blue, and leaves the scissor off as it found it.
void ClearSquareBlue(int left, int bottom, int size) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(left, bottom, size, size);
    glClearColor(0.f, 0.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
}

int Report(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    return ok ? 0 : 1;
}

// A stage whose UI fills the window red, then a rendering callback that clears
// a square in the middle blue. The callback runs after the UI, so blue is on top.
int CheckRenderingCallback(int fbW, int fbH) {
    jadefx::Stage stage;
    if (!stage.initializeGraphics(&GetProc)) {
        return Report(false, "a rendering callback draws over the UI (no graphics)");
    }
    auto root = jadefx::make<jadefx::StackPane>();
    root->setStyle("background-color: #ff0000;");
    stage.getScene().setRoot(root);
    const int size = fbH / 4;
    stage.setRenderingCallback([&](int, int) { ClearSquareBlue(fbW / 2 - size / 2, fbH / 2 - size / 2, size); });
    stage.frame(kWidth, kHeight, fbW, fbH);
    const bool onTop = IsBlue(PixelAt(fbW / 2, fbH / 2));
    const bool uiDrawn = IsRed(PixelAt(2, 2));
    stage.shutdownGraphics();
    return Report(onTop && uiDrawn, "a rendering callback draws over the UI");
}

int RunChecks(UiRenderer&, int fbW, int fbH) {
    int failures = 0;
    failures += CheckRenderingCallback(fbW, fbH);
    return failures;
}
```

In `main`, after `failures += WriteScenes(renderer, fbW, fbH, outDir);`, add `failures += RunChecks(renderer, fbW, fbH);`.

- [ ] **Step 2: Run it on today's renderer**

Run: `cmake --build $JFX/build --target jadefx-render-check --parallel && cd $JFX && build/jadefx-render-check $OUT/render-pre-check`
Expected: `ok   a rendering callback draws over the UI` and `render-check: all passed`. If `jadefx/jadefx.hpp` does not provide `StackPane` or `Stage`, add `#include "jadefx/scene/layout/StackPane.hpp"` and `#include "jadefx/stage/Stage.hpp"`.

- [ ] **Step 3: Add the GL entry points.** In `src/gl/gl.hpp`, add `using GLintptr = std::ptrdiff_t;` after `GLsizeiptr`. Add an `#ifdef GL_STREAM_DRAW / #undef` block beside the others, and `constexpr GLenum GL_STREAM_DRAW = 0x88E0;` after `GL_DYNAMIC_DRAW`. Then add:

```cpp
extern void (*jadefx_glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
extern void (*jadefx_glVertexAttribDivisor)(GLuint index, GLuint divisor);
extern void (*jadefx_glDrawArraysInstanced)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
```
and `#define glBufferSubData jadefx_glBufferSubData`, `#define glVertexAttribDivisor jadefx_glVertexAttribDivisor`, `#define glDrawArraysInstanced jadefx_glDrawArraysInstanced` beside their neighbours. In `src/gl/gl.cpp`, define the three pointers `= nullptr` and add `LOAD(BufferSubData);`, `LOAD(VertexAttribDivisor);`, `LOAD(DrawArraysInstanced);` after `LOAD(DrawArrays);`.

- [ ] **Step 4: Rewrite `res/shaders/box.vert`**

```glsl
layout(location = 0) in vec2 aPos;
// One box, the same at all six corners of its quad. See BoxInstance.
layout(location = 1) in vec4 aRect;
layout(location = 2) in vec4 aBox;
layout(location = 3) in vec4 aRadii;
layout(location = 4) in vec4 aParams;
layout(location = 5) in vec4 aBorder;
layout(location = 6) in vec4 aClip;
layout(location = 7) in vec4 aClipRadii;
layout(location = 8) in vec4 aColor;

uniform vec2 uViewport;

out vec2 vLocal;
flat out vec4 vBox;
flat out vec4 vRadii;
flat out vec4 vParams;
flat out vec4 vBorder;
flat out vec4 vClip;
flat out vec4 vClipRadii;
flat out vec4 vColor;

void main() {
    vLocal = aPos * aRect.zw;
    vec2 pixel = aRect.xy + vLocal;
    vec2 clip = vec2(pixel.x / uViewport.x * 2.0 - 1.0, 1.0 - pixel.y / uViewport.y * 2.0);
    gl_Position = vec4(clip, 0.0, 1.0);
    vBox = aBox;
    vRadii = aRadii;
    vParams = aParams;
    vBorder = aBorder;
    vClip = aClip;
    vClipRadii = aClipRadii;
    vColor = aColor;
}
```

- [ ] **Step 5: Edit `res/shaders/box.frag`.** Rename the per-box uniforms to the flat inputs everywhere, comments included, and make the two `color = uStops[0];` lines take `firstStop()`:

```bash
cd $JFX && perl -pi -e 's/\bu(ClipRadii|Box|Radii|Params|Border|Clip)\b/v$1/g; s/^(\s+)color = uStops\[0\];/$1color = firstStop();/' res/shaders/box.frag
```
Then replace the declaration block at the top (from `in vec2 vLocal;` to `uniform float uStopAt[8];`) with:

```glsl
in vec2 vLocal;
flat in vec4 vBox;
flat in vec4 vRadii;
flat in vec4 vParams;
flat in vec4 vBorder;
flat in vec4 vClip;
flat in vec4 vClipRadii;
flat in vec4 vColor;

// 1 draws a gradient through the stops below. 0 draws vColor.
uniform float uGradient;
uniform float uStopCount;
uniform vec4 uStops[8];
uniform float uStopAt[8];
```
Replace the start of `sampleStops` (its signature through the `count <= 1` branch) with:

```glsl
// The box's one color, or a gradient's first stop.
vec4 firstStop() {
    return uGradient > 0.5 ? uStops[0] : vColor;
}

vec4 sampleStops(float t) {
    int count = int(uStopCount + 0.5);
    if (uGradient < 0.5 || count <= 1) {
        return firstStop();
    }
```
Verify: `grep -nE 'u(Box|Radii|Params|Border|Clip|ClipRadii)\b' res/shaders/box.frag` prints nothing. `grep -c 'firstStop()' res/shaders/box.frag` prints 4: the definition, the `count <= 1` return, and the border and shadow branches. The rest of `main` is untouched apart from the renames.

- [ ] **Step 6: Edit `src/gl/UiRenderer.hpp`.** Add `#include "gl/BoxBatch.hpp"`. Make these public changes:

```cpp
    // Draws the boxes queued so far. Fills, borders, and shadows are queued and
    // drawn in runs, before the next text, image, gradient, clip change,
    // screenshot, or end. GL of a caller's own comes after a flush, or the
    // boxes queued before it land on top of it.
    void flush();
```
Change `bool writePpm(const char* path) const;` to `bool writePpm(const char* path);`, with the comment `// Draws what is queued, then writes the framebuffer as a binary PPM.`. Make these private changes: add `void drawBoxRun(bool gradient);` with the comment `// Draws the pending run in one call and empties it. gradient draws it through the stop uniforms.`. Delete `boxRect_`, `boxBox_`, `boxRadii_`, `boxParams_`, `boxBorder_`, `boxClip_`, and `boxClipRadii_`. Replace the `BoxSlot` comment and enum, and add the batch state:

```cpp
    int boxViewport_ = -1;
    int boxGradient_ = -1;
    int boxStopCount_ = -1;
    int boxStops_[8] = {};
    int boxStopAt_[8] = {};
    // The box program's uniforms as last set. A run sets the viewport and the
    // gradient flag, and a gradient its stops, so most calls are skipped.
    enum BoxSlot { kBoxViewport, kBoxGradient, kBoxStopCount, kBoxStop0, kBoxStopAt0 = kBoxStop0 + 8,
                   kBoxSlots = kBoxStopAt0 + 8 };
    UniformCache boxUniforms_{kBoxSlots};
    // The run not drawn yet, and the stream runs are drawn from: the first
    // boxInstanceUsed_ boxes of its current store are taken this frame.
    BoxBatch boxBatch_;
    unsigned boxInstanceVbo_ = 0;
    int boxInstanceUsed_ = 0;
```

- [ ] **Step 7: Edit `src/gl/UiRenderer.cpp`.** In the anonymous namespace, after `Location`:

```cpp
// BoxInstance's eight vec4s are box.vert's attributes 1 to 8.
constexpr GLuint kBoxFirstField = 1;
constexpr GLuint kBoxFields = 8;
// The stream holds one full run.
constexpr GLsizeiptr kBoxStreamBytes = static_cast<GLsizeiptr>(BoxBatch::kMaxRun * sizeof(BoxInstance));

// Points the box attributes at the run that starts offset bytes into the bound stream.
void PointBoxFields(std::size_t offset) {
    const GLsizei stride = static_cast<GLsizei>(sizeof(BoxInstance));
    for (GLuint i = 0; i < kBoxFields; ++i) {
        const std::size_t at = offset + i * 4 * sizeof(float);
        glVertexAttribPointer(kBoxFirstField + i, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(at));
    }
}
```

In `initialize`, replace the seven `boxRect_` … `boxClipRadii_` `Location` lines with `boxGradient_ = Location(boxProgram_, "uGradient");`, keeping `boxViewport_` and the stop locations. After the quad's `glVertexAttribPointer(0, ...)`, still inside the box VAO, add:

```cpp
    // Each box is one instance: attributes 1 to 8 step once a box through the
    // stream, and drawBoxRun points them at each run.
    glGenBuffers(1, &boxInstanceVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, boxInstanceVbo_);
    glBufferData(GL_ARRAY_BUFFER, kBoxStreamBytes, nullptr, GL_STREAM_DRAW);
    for (GLuint i = 0; i < kBoxFields; ++i) {
        glEnableVertexAttribArray(kBoxFirstField + i);
        glVertexAttribDivisor(kBoxFirstField + i, 1);
    }
    PointBoxFields(0);
    boxInstanceUsed_ = BoxBatch::kMaxRun;
```

In `shutdown`, before the `boxVbo_` block:

```cpp
    boxBatch_.clear();
    if (boxInstanceVbo_ != 0) {
        glDeleteBuffers(1, &boxInstanceVbo_);
        boxInstanceVbo_ = 0;
    }
```

In `begin`, after `atlasGaveUp_ = false;`:

```cpp
    boxBatch_.clear();
    // Counted as full, so the frame's first run starts a new store.
    boxInstanceUsed_ = BoxBatch::kMaxRun;
```

In `end`, make `flush();` the first line. In `pushClip`, make `flush();` the first line, with the comment `// The scissor is GL state, so what was queued under the old clip is drawn under it.`. In `popClip`, add `flush();` right after the `if (clips_.empty()) { return; }` block. In `text`, add `flush();` immediately before `glUseProgram(textProgram_);`. In `drawImage`, add `flush();` immediately before `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);`. In `writePpm`, drop `const` and make `flush();` the first line after the argument check.

Replace `drawBox`'s body after its early return with:

```cpp
    const BoxInstance box = MakeBoxInstance(scale_, x, y, width, height, boxX, boxY, boxW, boxH, radius, stops[0], mode,
                                            sides, blur, angleDeg, clip, clipRadii, exact);
    if (stopCount == 1) {
        if (!boxBatch_.add(box)) {
            flush();
            boxBatch_.add(box);
        }
        return;
    }
    // A gradient's stops are too many to carry a box, so they stay uniforms and it is a run of its own.
    flush();
    const int count = std::min(stopCount, 8);
    glUseProgram(boxProgram_);
    UniformCache& u = boxUniforms_;
    if (u.changed(kBoxStopCount, static_cast<float>(count))) {
        glUniform1f(boxStopCount_, static_cast<float>(count));
    }
    // box.frag reads only the first uStopCount stops, so the rest are left as they are.
    for (int i = 0; i < count; ++i) {
        const Color& color = stops[i];
        const float at = stopAt != nullptr ? stopAt[i] : 1.f;
        if (u.changed(kBoxStop0 + i, color.r, color.g, color.b, color.a)) {
            glUniform4f(boxStops_[i], color.r, color.g, color.b, color.a);
        }
        if (u.changed(kBoxStopAt0 + i, at)) {
            glUniform1f(boxStopAt_[i], at);
        }
    }
    boxBatch_.add(box);
    drawBoxRun(true);
}

void UiRenderer::flush() { drawBoxRun(false); }

void UiRenderer::drawBoxRun(bool gradient) {
    const int count = boxBatch_.size();
    if (count == 0) {
        return;
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Always bound: other code, such as a 3D view drawn between UI draws, binds its own.
    glUseProgram(boxProgram_);
    // A uniform keeps its value in its program, so only what changed is sent.
    UniformCache& u = boxUniforms_;
    const float vw = static_cast<float>(viewportW_);
    const float vh = static_cast<float>(viewportH_);
    if (u.changed(kBoxViewport, vw, vh)) {
        glUniform2f(boxViewport_, vw, vh);
    }
    const float flag = gradient ? 1.f : 0.f;
    if (u.changed(kBoxGradient, flag)) {
        glUniform1f(boxGradient_, flag);
    }
    glBindVertexArray(boxVao_);
    glBindBuffer(GL_ARRAY_BUFFER, boxInstanceVbo_);
    if (boxInstanceUsed_ + count > BoxBatch::kMaxRun) {
        // A new store, so this run never overwrites boxes a draw before it may still be reading.
        glBufferData(GL_ARRAY_BUFFER, kBoxStreamBytes, nullptr, GL_STREAM_DRAW);
        boxInstanceUsed_ = 0;
    }
    const std::size_t offset = static_cast<std::size_t>(boxInstanceUsed_) * sizeof(BoxInstance);
    glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offset),
                    static_cast<GLsizeiptr>(static_cast<std::size_t>(count) * sizeof(BoxInstance)), boxBatch_.data());
    PointBoxFields(offset);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, count);
    boxInstanceUsed_ += count;
    boxBatch_.clear();
}
```
(The old `drawBox` closing brace is the one before `void UiRenderer::flush()`. Remove the old per-box uniform code, `glBindVertexArray`, and `glDrawArrays` from `drawBox`.)

- [ ] **Step 8: Flush before the rendering callback.** In `src/stage/Stage.cpp` `Stage::frame`:

```cpp
    if (afterUi_) {
        // The callback draws GL of its own, over the UI, so the queued boxes go first.
        renderer_->flush();
        afterUi_(framebufferWidth, framebufferHeight);
    }
```

- [ ] **Step 9: Build, unit tests, render-check, compare**

```bash
cmake --build $JFX/build --parallel && $JFX/build/jadefx-tests
cd $JFX && build/jadefx-render-check $OUT/render-after
cd $OUT/render-before && for f in *.ppm; do cmp -s "$f" "../render-after/$f" || echo "DIFF $f"; done; ls | wc -l; ls ../render-after | wc -l
```
Expected: `jadefx-tests` exits 0. render-check prints `ok   a rendering callback draws over the UI` and `all passed`. No `DIFF` lines, and both directories hold 25 files. A `DIFF` means stop: view both PNG conversions, find which scene and why, and fix it before going on. Do not adjust the baseline.

- [ ] **Step 10: Compare Anarchy's assets-demo against the new JadeFX**

```bash
cmake --build $ANA/build-batching --target assets-demo --parallel
cd $ANA && build-batching/assets-demo $OUT/assets-after/light light && build-batching/assets-demo $OUT/assets-after/dark dark
cd $OUT/assets-before && for f in */*.png; do cmp -s "$f" "../assets-after/$f" || echo "DIFF $f"; done
```
Expected: no `DIFF` lines, apart from files already listed as nondeterministic in `$OUT/notes.txt`.

- [ ] **Step 11: Commit**

```bash
git -C $JFX add src/gl/gl.hpp src/gl/gl.cpp res/shaders/box.vert res/shaders/box.frag src/gl/UiRenderer.hpp src/gl/UiRenderer.cpp src/stage/Stage.cpp tests/render_check.cpp
git -C $JFX commit -m "Draw runs of one-color boxes with one instanced call"
```

---

### Task 4: Raw GL inside a node: setDrawsRawGl and Painter::flush

**Files:**
- Modify: `include/jadefx/scene/Node.hpp`, `src/scene/Node.cpp`, `include/jadefx/scene/Painter.hpp`, `src/scene/Painter.cpp`, `tests/render_check.cpp`

**Interfaces:**
- Consumes: `UiRenderer::flush()` (Task 3); render-check helpers (Task 3).
- Produces: `void Node::setDrawsRawGl(bool)`, `bool Node::drawsRawGl() const`, `void Painter::flush()`.

- [ ] **Step 1: Write the checks** in `tests/render_check.cpp`, before `RunChecks`:

```cpp
// A node that clears a square to blue with GL of its own in renderContent.
// With viaPainter, it first fills the same square red through a Painter and
// flushes, so only the order of the two decides the color.
class RawSquare : public jadefx::Node {
public:
    RawSquare(int left, int bottom, int size, bool viaPainter)
        : left_(left), bottom_(bottom), size_(size), viaPainter_(viaPainter) {}

protected:
    void renderContent(UiRenderer& renderer, float) override {
        if (viaPainter_) {
            jadefx::Painter painter(renderer);
            painter.fillRect(0.f, 0.f, 64.f, 64.f, Color::rgba(1.f, 0.f, 0.f, 1.f));
            painter.flush();
        }
        ClearSquareBlue(left_, bottom_, size_);
    }

private:
    int left_;
    int bottom_;
    int size_;
    bool viaPainter_;
};

// A red box queued at points 0..64 (pixels, at scale 1), then a node whose GL
// clears pixels 16..48 blue. Returns the pixel at the square's middle.
std::array<unsigned char, 4> DrawRawNode(UiRenderer& renderer, int fbW, int fbH, bool optIn, bool viaPainter) {
    renderer.begin(fbW, fbH, 1.f, kWhite, true);
    if (!viaPainter) {
        renderer.fillRect(0.f, 0.f, 64.f, 64.f, Color::rgba(1.f, 0.f, 0.f, 1.f));
    }
    auto node = jadefx::make<RawSquare>(16, fbH - 48, 32, viaPainter);
    node->setDrawsRawGl(optIn);
    node->render(renderer, 1.f);
    renderer.end();
    return PixelAt(32, fbH - 32);
}

int CheckRawGlNodes(UiRenderer& renderer, int fbW, int fbH) {
    int failures = 0;
    failures += Report(IsBlue(DrawRawNode(renderer, fbW, fbH, true, false)),
                       "raw GL in a node that opts in lands on top of the boxes before it");
    failures += Report(IsRed(DrawRawNode(renderer, fbW, fbH, false, false)),
                       "raw GL in a node that does not opt in lands under them (the documented failure)");
    failures += Report(IsBlue(DrawRawNode(renderer, fbW, fbH, true, true)),
                       "Painter::flush puts a node's own boxes under its raw GL");
    return failures;
}
```
In `RunChecks`, name the renderer parameter (`UiRenderer& renderer`) and add `failures += CheckRawGlNodes(renderer, fbW, fbH);` before the callback check. Include `"jadefx/scene/Painter.hpp"` if `jadefx/jadefx.hpp` does not provide it.

- [ ] **Step 2: Run to see it fail**

Run: `cmake --build $JFX/build --target jadefx-render-check --parallel`
Expected: the build fails with `no member named 'setDrawsRawGl'` and `no member named 'flush'` in `Painter`.

- [ ] **Step 3: Implement.** In `include/jadefx/scene/Node.hpp`, after `isPickOnBounds`:

```cpp
    // Set by a node whose renderContent makes GL calls of its own, so the boxes
    // queued before it are drawn first and its GL lands on top. See renderContent.
    void setDrawsRawGl(bool value) { drawsRawGl_ = value; }
    bool drawsRawGl() const { return drawsRawGl_; }
```
After `bool pickOnBounds_ = true;`, add `bool drawsRawGl_ = false;`. Above `virtual void renderContent(UiRenderer& renderer, float opacity);`:

```cpp
    // Draws the node's own content through renderer, or a Painter on it. Boxes
    // are queued and drawn in runs, so a node that makes GL calls of its own
    // sets setDrawsRawGl(true), and one that draws boxes and then GL here calls
    // Painter::flush between them. Otherwise its GL lands under the boxes
    // queued before it. GL of its own leaves the viewport, scissor, and blend
    // enable as it found them.
```
In `src/scene/Node.cpp` `Node::render`:

```cpp
    drawChrome(renderer, next);
    renderChildren(renderer, next);
    if (drawsRawGl_) {
        // Its GL draws as soon as it is called, so what is queued goes first.
        renderer.flush();
    }
    renderContent(renderer, next);
```
In `include/jadefx/scene/Painter.hpp`, after `popClip`:

```cpp
    // Draws the boxes queued so far, this painter's and those before it, so GL
    // calls of the node's own that follow land on top. See Node::setDrawsRawGl.
    void flush();
```
In `src/scene/Painter.cpp`: `void Painter::flush() { renderer_.flush(); }`.

- [ ] **Step 4: Run to see it pass**

```bash
cmake --build $JFX/build --parallel && $JFX/build/jadefx-tests
cd $JFX && build/jadefx-render-check $OUT/render-after-4
cd $OUT/render-before && for f in *.ppm; do cmp -s "$f" "../render-after-4/$f" || echo "DIFF $f"; done
```
Expected: three `ok` raw-GL lines, the callback `ok` line, `all passed`, and no `DIFF` lines.

- [ ] **Step 5: Commit**

```bash
git -C $JFX add include/jadefx/scene/Node.hpp src/scene/Node.cpp include/jadefx/scene/Painter.hpp src/scene/Painter.cpp tests/render_check.cpp
git -C $JFX commit -m "Let a node that draws its own GL opt in to a flush, through Node::setDrawsRawGl and Painter::flush"
```

---

### Task 5: Anarchy's raw-GL nodes opt in

**Files (Anarchy worktree `$ANA`):**
- Modify: `src/runner/GameView.cpp` (constructor body), `src/ide/IdeAssets.cpp` (constructor body)

**Interfaces:**
- Consumes: `jadefx::Node::setDrawsRawGl(bool)` (Task 4).

- [ ] **Step 1: Opt in.** At the start of `GameView::GameView`'s body:

```cpp
    // The 3D view draws with GL of its own in renderContent.
    setDrawsRawGl(true);
```
At the start of `IdeAssets::IdeAssets`'s body, before `watch_ = ...`:

```cpp
    // The material balls draw with GL of their own in renderContent.
    setDrawsRawGl(true);
```

- [ ] **Step 2: Build everything and run Anarchy's suites**

```bash
cmake --build $ANA/build-batching --parallel
cd $ANA/build-batching && ctest -C Release --output-on-failure
```
Expected: every test passes. If one fails, run the same test on Anarchy `main`'s own `build/` to see whether it already failed before this change, and report which.

- [ ] **Step 3: Compare assets-demo again, and look at the Scene View**

```bash
cd $ANA && build-batching/assets-demo $OUT/assets-after-5/light light && build-batching/assets-demo $OUT/assets-after-5/dark dark
cd $OUT/assets-before && for f in */*.png; do cmp -s "$f" "../assets-after-5/$f" || echo "DIFF $f"; done
cd $ANA && build-batching/profiler-demo $OUT/profiler-check ~/Documents/AnarchyEngineProjects/CannonVsPigs
```
Expected: no `DIFF` lines (nondeterministic files excepted). Open the profiler-demo PNGs and confirm the overlay and labels sit on top of the 3D view, with no boxes hidden under it. Write what was seen in `$OUT/notes.txt`.

- [ ] **Step 4: Commit on the Anarchy branch** (in `$ANA`, never on `main`)

```bash
git -C $ANA add src/runner/GameView.cpp src/ide/IdeAssets.cpp
git -C $ANA commit -m "Opt the Scene View and the Assets pane in to JadeFX's flush before their own GL"
```

---

### Task 6: Measure, update the spec and README

**Files:**
- Modify: `docs/superpowers/specs/2026-10-04-box-batching-design.md`, `README.md`

- [ ] **Step 1: Measure**

```bash
cd $ANA && build-batching/profiler-demo $OUT/profiler-after ~/Documents/AnarchyEngineProjects/CannonVsPigs
```
Read the average ms for **Profiler overlay** and **Scene View** from the Scopes PNG and add them to `$OUT/timings.txt` as `after`. Run it twice and take the second run's numbers if the first was cold. Do not tune anything here. If a goal is missed, report the numbers as they are.

- [ ] **Step 2: Update the spec.** Add an "after batching" column to the Why table with the measured numbers, and add a short "Departures" section listing the four departures at the top of this plan.

- [ ] **Step 3: Update `README.md`.** After the paragraph that starts "`frame` lays out the scene", add:

```markdown
Fills, borders, and shadows are queued and drawn in runs, one instanced draw call per run: before the next text, image, gradient, or clip change, and at the end of the frame. A node whose `renderContent` makes GL calls of its own calls `setDrawsRawGl(true)`, so what was queued before it is drawn first and its GL lands on top. A node that draws through a `Painter` and then with GL in the same `renderContent` calls `painter.flush()` between the two. A node that does neither has its GL drawn under the boxes queued before it. The `setRenderingCallback` callback needs neither, because the stage draws what is queued before calling it. GL of your own must leave the viewport, scissor, and blend enable as it found them.
```

- [ ] **Step 4: Commit**

```bash
git -C $JFX add docs/superpowers/specs/2026-10-04-box-batching-design.md README.md
git -C $JFX commit -m "Record the box batching measurements, and document raw GL in a node"
```

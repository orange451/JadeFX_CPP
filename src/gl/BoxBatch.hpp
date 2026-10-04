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

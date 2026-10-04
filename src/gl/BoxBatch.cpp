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

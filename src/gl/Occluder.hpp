#pragma once

namespace jadefx {

// What UiRenderer hides UI fragments behind while it is set: a depth texture,
// the framebuffer rectangle it covers (x, y from the bottom left, width,
// height, in pixels), and the depth of what is being drawn. A fragment in the
// rectangle whose texel is nearer than depth is not drawn. revision moves on
// every change, so a program sends the uniforms again only then.
struct Occluder {
    unsigned texture = 0;
    float rect[4] = {0.f, 0.f, 0.f, 0.f};
    float depth = 0.f;
    unsigned revision = 0;

    bool active() const { return texture != 0 && rect[2] > 0.f && rect[3] > 0.f; }
};

}  // namespace jadefx

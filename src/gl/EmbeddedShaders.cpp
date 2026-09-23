#include "Resources.hpp"
#include <cstring>

const char* EmbeddedShader(const char* filename) {
    if (std::strcmp(filename, "box.vert") == 0) {
        return R"jadefx(
layout(location = 0) in vec2 aPos;

uniform vec4 uRect;
uniform vec2 uViewport;

out vec2 vLocal;

void main() {
    vLocal = aPos * uRect.zw;
    vec2 pixel = uRect.xy + vLocal;
    vec2 clip = vec2(pixel.x / uViewport.x * 2.0 - 1.0, 1.0 - pixel.y / uViewport.y * 2.0);
    gl_Position = vec4(clip, 0.0, 1.0);
}
)jadefx";
    }
    if (std::strcmp(filename, "box.frag") == 0) {
        return R"jadefx(
in vec2 vLocal;

uniform vec4 uBox;
uniform vec4 uRadii;
uniform vec4 uParams;
uniform vec4 uBorder;
uniform float uStopCount;
uniform vec4 uStops[8];
uniform float uStopAt[8];

out vec4 fragColor;

// uParams: mode, unused, blur, gradient angle in degrees.
// uBorder: top, right, bottom, left.
// mode 0 fills, 1 is a border ring, 2 is an outer shadow, 3 is an inset shadow.
// Radii are top-left, top-right, bottom-right, bottom-left, with y growing downward.

float roundedDistance(vec2 point, vec2 halfSize, vec4 radii) {
    float radius;
    if (point.y < 0.0) {
        radius = point.x < 0.0 ? radii.x : radii.y;
    } else {
        radius = point.x < 0.0 ? radii.w : radii.z;
    }
    radius = min(radius, min(halfSize.x, halfSize.y));
    vec2 q = abs(point) - halfSize + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

vec4 sampleStops(float t) {
    int count = int(uStopCount + 0.5);
    if (count <= 1) {
        return uStops[0];
    }
    vec4 color = uStops[0];
    for (int i = 1; i < 8; ++i) {
        if (i >= count) {
            break;
        }
        float t0 = uStopAt[i - 1];
        float t1 = uStopAt[i];
        if (t <= t1 || i == count - 1) {
            float span = max(t1 - t0, 0.0001);
            return mix(uStops[i - 1], uStops[i], clamp((t - t0) / span, 0.0, 1.0));
        }
        color = uStops[i];
    }
    return color;
}

void main() {
    vec2 outerSize = max(uBox.zw, vec2(0.0));
    vec2 outerHalf = outerSize * 0.5;
    vec2 outerCenter = uBox.xy + outerHalf;
    vec2 point = vLocal - outerCenter;
    float dist = roundedDistance(point, outerHalf, uRadii);
    float cover = 1.0 - smoothstep(-0.75, 0.75, dist);

    float angle = radians(uParams.w);
    vec2 direction = vec2(sin(angle), -cos(angle));
    float span = abs(direction.x) * outerHalf.x + abs(direction.y) * outerHalf.y;
    float gradientT = span > 0.0 ? dot(point, direction) / span : 0.0;
    gradientT = clamp(gradientT * 0.5 + 0.5, 0.0, 1.0);

    float mode = uParams.x;
    vec4 color = sampleStops(gradientT);
    float alpha = cover;
    if (mode < 0.5) {
        alpha = cover;
    } else if (mode < 1.5) {
        float top = max(uBorder.x, 0.0);
        float right = max(uBorder.y, 0.0);
        float bottom = max(uBorder.z, 0.0);
        float left = max(uBorder.w, 0.0);
        vec2 innerSize = max(outerSize - vec2(left + right, top + bottom), vec2(0.0));
        vec2 innerHalf = innerSize * 0.5;
        vec2 innerCenter = uBox.xy + vec2(left, top) + innerHalf;
        vec4 innerRadii = max(uRadii - vec4(max(top, left), max(top, right), max(bottom, right), max(bottom, left)),
                              0.0);
        float inner = roundedDistance(vLocal - innerCenter, innerHalf, innerRadii);
        float innerCover = 1.0 - smoothstep(-0.75, 0.75, inner);
        alpha = clamp(cover - innerCover, 0.0, 1.0);
        color = uStops[0];
    } else if (mode < 2.5) {
        alpha = 1.0 - smoothstep(0.0, max(uParams.z, 0.5), dist);
        color = uStops[0];
    } else {
        alpha = (1.0 - smoothstep(0.0, max(uParams.z, 0.5), -dist)) * cover;
        color = uStops[0];
    }

    color.a *= alpha;
    if (color.a <= 0.001) {
        discard;
    }
    fragColor = color;
}
)jadefx";
    }
    if (std::strcmp(filename, "text.vert") == 0) {
        return R"jadefx(
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;

uniform vec2 uViewport;

out vec2 vUv;

void main() {
    vUv = aUv;
    vec2 clip = vec2(aPos.x / uViewport.x * 2.0 - 1.0, 1.0 - aPos.y / uViewport.y * 2.0);
    gl_Position = vec4(clip, 0.0, 1.0);
}
)jadefx";
    }
    if (std::strcmp(filename, "text.frag") == 0) {
        return R"jadefx(
in vec2 vUv;

uniform sampler2D uTex;
uniform vec4 uColor;

// uTex.rgb is coverage of the red, green, and blue stripes. The second output
// is the blend factor, so each stripe composites against the framebuffer on its own.
layout(location = 0, index = 0) out vec4 fragColor;
layout(location = 0, index = 1) out vec4 fragMask;

void main() {
    vec3 coverage = texture(uTex, vUv).rgb;
    float mask = max(coverage.r, max(coverage.g, coverage.b));
    if (mask * uColor.a <= 0.001) {
        discard;
    }
    fragColor = vec4(uColor.rgb, 1.0);
    fragMask = vec4(coverage * uColor.a, mask * uColor.a);
}
)jadefx";
    }
    return "";
}

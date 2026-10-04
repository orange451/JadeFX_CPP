// Grayscale coverage, used when the context cannot dual-source blend text.frag.
in vec2 vUv;

uniform sampler2D uTex;
uniform vec4 uColor;
// The same mask gamma as text.frag. z is unused; this path is always grayscale.
uniform vec3 uGamma;

// While uOccluded is 1, a fragment inside uOccluderRect (framebuffer pixels:
// x, y from the bottom left, width, height) whose depth in uOccluder is
// nearer than uOccluderDepth is not drawn: UI drawn inside a 3D view, behind
// what the view drew in front of it.
uniform sampler2D uOccluder;
uniform vec4 uOccluderRect;
uniform float uOccluderDepth;
uniform float uOccluded;

bool occluded() {
    if (uOccluded < 0.5) {
        return false;
    }
    vec2 uv = (gl_FragCoord.xy - uOccluderRect.xy) / uOccluderRect.zw;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        return false;
    }
    return texture(uOccluder, uv).r < uOccluderDepth;
}

out vec4 fragColor;

float toLinear(float v) {
    if (uGamma.x == 0.0) {
        return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
    }
    return pow(v, uGamma.x);
}

float fromLinear(float v) {
    if (uGamma.x == 0.0) {
        return v <= 0.0031308 ? v * 12.92 : 1.055 * pow(v, 1.0 / 2.4) - 0.055;
    }
    return pow(v, 1.0 / uGamma.x);
}

// SkTMaskGamma_build_correcting_lut for the text's luminance. See text.frag.
float correct(float coverage, float src) {
    float dst = 1.0 - src;
    float linSrc = toLinear(src);
    float linDst = toLinear(dst);
    float boosted = coverage + (1.0 - coverage) * uGamma.y * linDst * coverage;
    if (abs(src - dst) < 1.0 / 256.0) {
        return boosted;
    }
    return (fromLinear(linSrc * boosted + linDst * (1.0 - boosted)) - dst) / (src - dst);
}

void main() {
    if (occluded()) {
        discard;
    }
    float coverage = texture(uTex, vUv).r;
    vec3 linear = vec3(toLinear(uColor.r), toLinear(uColor.g), toLinear(uColor.b));
    float luminance = fromLinear(dot(linear, vec3(0.2126, 0.7152, 0.0722)));
    fragColor = vec4(uColor.rgb, uColor.a * clamp(correct(coverage, luminance), 0.0, 1.0));
    if (fragColor.a <= 0.001) {
        discard;
    }
}

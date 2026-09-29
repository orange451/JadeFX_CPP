in vec2 vUv;

uniform sampler2D uTex;
uniform vec4 uColor;
// Skia's mask gamma. x is the display gamma, where 0 means the sRGB curve, and y
// is the contrast boost. z is 1 for grayscale masks, which correct by luminance.
uniform vec3 uGamma;

// uTex.rgb is coverage of the red, green, and blue stripes. The second output
// is the blend factor, so each stripe composites against the framebuffer on its own.
layout(location = 0, index = 0) out vec4 fragColor;
layout(location = 0, index = 1) out vec4 fragMask;

vec3 toLinear(vec3 v) {
    if (uGamma.x == 0.0) {
        return mix(pow((v + 0.055) / 1.055, vec3(2.4)), v / 12.92, lessThanEqual(v, vec3(0.04045)));
    }
    return pow(v, vec3(uGamma.x));
}

vec3 fromLinear(vec3 v) {
    if (uGamma.x == 0.0) {
        return mix(1.055 * pow(v, vec3(1.0 / 2.4)) - 0.055, v * 12.92, lessThanEqual(v, vec3(0.0031308)));
    }
    return pow(v, vec3(1.0 / uGamma.x));
}

// SkTMaskGamma_build_correcting_lut. The blend below mixes sRGB values, so pick
// the coverage that lands where a blend in linear light would, against a
// background guessed to be the opposite of the text.
vec3 correct(vec3 coverage, vec3 src) {
    vec3 dst = 1.0 - src;
    vec3 linSrc = toLinear(src);
    vec3 linDst = toLinear(dst);
    vec3 boosted = coverage + (1.0 - coverage) * uGamma.y * linDst * coverage;
    vec3 blended = fromLinear(linSrc * boosted + linDst * (1.0 - boosted));
    bvec3 flat_ = lessThan(abs(src - dst), vec3(1.0 / 256.0));
    return mix((blended - dst) / mix(src - dst, vec3(1.0), flat_), boosted, flat_);
}

void main() {
    vec3 coverage = texture(uTex, vUv).rgb;
    float mask = max(coverage.r, max(coverage.g, coverage.b));
    if (mask * uColor.a <= 0.001) {
        discard;
    }
    vec3 src = uColor.rgb;
    if (uGamma.z > 0.5) {
        src = fromLinear(vec3(dot(toLinear(src), vec3(0.2126, 0.7152, 0.0722))));
    }
    coverage = clamp(correct(coverage, src), 0.0, 1.0);
    mask = max(coverage.r, max(coverage.g, coverage.b));
    fragColor = vec4(uColor.rgb, 1.0);
    fragMask = vec4(coverage * uColor.a, mask * uColor.a);
}

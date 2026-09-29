in vec2 vUv;

uniform sampler2D uTex;
uniform vec4 uColor;
// x is the display gamma and y the contrast boost that Skia applies to glyph masks.
uniform vec2 uGamma;

// uTex.rgb is coverage of the red, green, and blue stripes. The second output
// is the blend factor, so each stripe composites against the framebuffer on its own.
layout(location = 0, index = 0) out vec4 fragColor;
layout(location = 0, index = 1) out vec4 fragMask;

// Skia's correcting lookup, SkTMaskGamma_build_correcting_lut. The blend below
// mixes sRGB values, so pick the coverage that lands where a blend in linear
// light would, against a background guessed to be the opposite of the text.
vec3 correct(vec3 coverage, vec3 src) {
    vec3 dst = 1.0 - src;
    vec3 linSrc = pow(src, vec3(uGamma.x));
    vec3 linDst = pow(dst, vec3(uGamma.x));
    vec3 boosted = coverage + (1.0 - coverage) * uGamma.y * linDst * coverage;
    vec3 linOut = linSrc * boosted + linDst * (1.0 - boosted);
    vec3 out_ = pow(linOut, vec3(1.0 / uGamma.x));
    vec3 span = src - dst;
    vec3 safe = mix(span, vec3(1.0), lessThan(abs(span), vec3(1.0 / 256.0)));
    return mix((out_ - dst) / safe, boosted, lessThan(abs(span), vec3(1.0 / 256.0)));
}

void main() {
    vec3 coverage = texture(uTex, vUv).rgb;
    float mask = max(coverage.r, max(coverage.g, coverage.b));
    if (mask * uColor.a <= 0.001) {
        discard;
    }
    coverage = clamp(correct(coverage, uColor.rgb), 0.0, 1.0);
    mask = max(coverage.r, max(coverage.g, coverage.b));
    fragColor = vec4(uColor.rgb, 1.0);
    fragMask = vec4(coverage * uColor.a, mask * uColor.a);
}

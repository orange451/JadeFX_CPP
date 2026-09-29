// Grayscale coverage, used when the context cannot dual-source blend text.frag.
in vec2 vUv;

uniform sampler2D uTex;
uniform vec4 uColor;
// The same mask gamma as text.frag. z is unused; this path is always grayscale.
uniform vec3 uGamma;

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
    float coverage = texture(uTex, vUv).r;
    vec3 linear = vec3(toLinear(uColor.r), toLinear(uColor.g), toLinear(uColor.b));
    float luminance = fromLinear(dot(linear, vec3(0.2126, 0.7152, 0.0722)));
    fragColor = vec4(uColor.rgb, uColor.a * clamp(correct(coverage, luminance), 0.0, 1.0));
    if (fragColor.a <= 0.001) {
        discard;
    }
}

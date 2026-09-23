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

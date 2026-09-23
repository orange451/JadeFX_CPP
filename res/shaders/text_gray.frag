// Grayscale coverage, used when the context cannot dual-source blend text.frag.
in vec2 vUv;

uniform sampler2D uTex;
uniform vec4 uColor;

out vec4 fragColor;

void main() {
    float coverage = texture(uTex, vUv).r;
    fragColor = vec4(uColor.rgb, uColor.a * coverage);
    if (fragColor.a <= 0.001) {
        discard;
    }
}

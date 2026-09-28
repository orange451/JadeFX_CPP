// Straight-alpha bitmap. v is top to bottom, matching the decoded rows.
// When uTinted is set, the bitmap is only a mask: its alpha is filled with uTint.
in vec2 vUv;

uniform sampler2D uTex;
uniform float uOpacity;
uniform vec4 uTint;
uniform float uTinted;

out vec4 fragColor;

void main() {
    vec4 color = texture(uTex, vUv);
    if (uTinted > 0.5) {
        color = vec4(uTint.rgb, color.a * uTint.a);
    }
    color.a *= uOpacity;
    if (color.a <= 0.001) {
        discard;
    }
    fragColor = color;
}

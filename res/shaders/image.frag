// Straight-alpha bitmap. v is top to bottom, matching the decoded rows.
in vec2 vUv;

uniform sampler2D uTex;
uniform float uOpacity;

out vec4 fragColor;

void main() {
    vec4 color = texture(uTex, vUv);
    color.a *= uOpacity;
    if (color.a <= 0.001) {
        discard;
    }
    fragColor = color;
}

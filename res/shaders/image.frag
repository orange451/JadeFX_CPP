// Straight-alpha bitmap. v is top to bottom, matching the decoded rows.
// When uTinted is set, the bitmap is only a mask: its alpha is filled with uTint.
in vec2 vUv;

uniform sampler2D uTex;
uniform float uOpacity;
uniform vec4 uTint;
uniform float uTinted;

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

void main() {
    if (occluded()) {
        discard;
    }
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

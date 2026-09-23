layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;

uniform vec2 uViewport;

out vec2 vUv;

void main() {
    vUv = aUv;
    vec2 clip = vec2(aPos.x / uViewport.x * 2.0 - 1.0, 1.0 - aPos.y / uViewport.y * 2.0);
    gl_Position = vec4(clip, 0.0, 1.0);
}

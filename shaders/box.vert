layout(location = 0) in vec2 aPos;

uniform vec4 uRect;
uniform vec2 uViewport;

out vec2 vLocal;

void main() {
    vLocal = aPos * uRect.zw;
    vec2 pixel = uRect.xy + vLocal;
    vec2 clip = vec2(pixel.x / uViewport.x * 2.0 - 1.0, 1.0 - pixel.y / uViewport.y * 2.0);
    gl_Position = vec4(clip, 0.0, 1.0);
}

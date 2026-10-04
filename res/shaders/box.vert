layout(location = 0) in vec2 aPos;
// One box, the same at all six corners of its quad. See BoxInstance.
layout(location = 1) in vec4 aRect;
layout(location = 2) in vec4 aBox;
layout(location = 3) in vec4 aRadii;
layout(location = 4) in vec4 aParams;
layout(location = 5) in vec4 aBorder;
layout(location = 6) in vec4 aClip;
layout(location = 7) in vec4 aClipRadii;
layout(location = 8) in vec4 aColor;

uniform vec2 uViewport;

out vec2 vLocal;
flat out vec4 vBox;
flat out vec4 vRadii;
flat out vec4 vParams;
flat out vec4 vBorder;
flat out vec4 vClip;
flat out vec4 vClipRadii;
flat out vec4 vColor;

void main() {
    vLocal = aPos * aRect.zw;
    vec2 pixel = aRect.xy + vLocal;
    vec2 clip = vec2(pixel.x / uViewport.x * 2.0 - 1.0, 1.0 - pixel.y / uViewport.y * 2.0);
    gl_Position = vec4(clip, 0.0, 1.0);
    vBox = aBox;
    vRadii = aRadii;
    vParams = aParams;
    vBorder = aBorder;
    vClip = aClip;
    vClipRadii = aClipRadii;
    vColor = aColor;
}

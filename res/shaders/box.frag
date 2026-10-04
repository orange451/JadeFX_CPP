in vec2 vLocal;
flat in vec4 vBox;
flat in vec4 vRadii;
flat in vec4 vParams;
flat in vec4 vBorder;
flat in vec4 vClip;
flat in vec4 vClipRadii;
flat in vec4 vColor;

// 1 draws a gradient through the stops below. 0 draws vColor.
uniform float uGradient;
uniform float uStopCount;
uniform vec4 uStops[8];
uniform float uStopAt[8];

out vec4 fragColor;

// vParams: mode, exact edges, blur radius, gradient angle in degrees.
// Exact edges cover each pixel by the area inside the box, so an edge on a whole
// pixel is sharp and two boxes that share one meet without a seam. Otherwise
// edges soften over a pixel and a half.
// vBorder: top, right, bottom, left.
// vClip: element x, y, width, height in the same local pixels as vBox.
// mode 0 fills, 1 is a border ring, 2 is an outer shadow, 3 is an inset shadow.
// Radii are top-left, top-right, bottom-right, bottom-left, with y growing downward.
// Blur radius is the Gaussian diameter: sigma = radius / 2, centered on the edge.

float roundedDistance(vec2 point, vec2 halfSize, vec4 radii) {
    float radius;
    if (point.y < 0.0) {
        radius = point.x < 0.0 ? radii.x : radii.y;
    } else {
        radius = point.x < 0.0 ? radii.w : radii.z;
    }
    radius = min(radius, min(halfSize.x, halfSize.y));
    vec2 q = abs(point) - halfSize + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

float roundedCoverage(vec2 origin, vec2 size, vec4 radii, vec2 local) {
    vec2 halfSize = max(size, vec2(0.0)) * 0.5;
    if (halfSize.x <= 0.0 || halfSize.y <= 0.0) {
        return 0.0;
    }
    float dist = roundedDistance(local - (origin + halfSize), halfSize, radii);
    return 1.0 - smoothstep(-0.75, 0.75, dist);
}

float gaussian(float x, float sigma) {
    const float pi = 3.141592653589793;
    return exp(-(x * x) / (2.0 * sigma * sigma)) / (sqrt(2.0 * pi) * sigma);
}

// Abramowitz and Stegun 7.1.26, the same approximation the Java shadow shader uses.
float erfApprox(float x) {
    float s = sign(x);
    float a = abs(x);
    float t = 1.0 + (0.278393 + (0.230389 + 0.078108 * (a * a)) * a) * a;
    t *= t;
    return s - s / (t * t);
}

// One row of a rounded box, integrated against a Gaussian of the given sigma.
// y is center-relative and grows downward.
float roundedBoxShadowX(float x, float y, float sigma, vec4 radii, vec2 halfSize) {
    float leftRadius = y < 0.0 ? radii.x : radii.w;
    float rightRadius = y < 0.0 ? radii.y : radii.z;
    float limit = min(halfSize.x, halfSize.y);
    leftRadius = clamp(leftRadius, 0.0, limit);
    rightRadius = clamp(rightRadius, 0.0, limit);
    float leftDelta = min(halfSize.y - leftRadius - abs(y), 0.0);
    float rightDelta = min(halfSize.y - rightRadius - abs(y), 0.0);
    float leftExtent = halfSize.x - leftRadius + sqrt(max(0.0, leftRadius * leftRadius - leftDelta * leftDelta));
    float rightExtent = halfSize.x - rightRadius + sqrt(max(0.0, rightRadius * rightRadius - rightDelta * rightDelta));
    float scale = 0.7071067811865 / sigma;
    float upper = 0.5 + 0.5 * erfApprox((x + leftExtent) * scale);
    float lower = 0.5 + 0.5 * erfApprox((x - rightExtent) * scale);
    return upper - lower;
}

float roundedBoxShadow(vec2 lower, vec2 upper, vec2 point, float sigma, vec4 radii) {
    vec2 center = (lower + upper) * 0.5;
    vec2 halfSize = (upper - lower) * 0.5;
    point -= center;

    float low = point.y - halfSize.y;
    float high = point.y + halfSize.y;
    float start = clamp(-3.0 * sigma, low, high);
    float end = clamp(3.0 * sigma, low, high);
    float stepSize = (end - start) / 4.0;
    float y = start + stepSize * 0.5;
    float value = 0.0;
    for (int i = 0; i < 4; i++) {
        value += roundedBoxShadowX(point.x, point.y - y, sigma, radii, halfSize) * gaussian(y, sigma) * stepSize;
        y += stepSize;
    }
    return value;
}

// The box's one color, or a gradient's first stop.
vec4 firstStop() {
    return uGradient > 0.5 ? uStops[0] : vColor;
}

vec4 sampleStops(float t) {
    int count = int(uStopCount + 0.5);
    if (uGradient < 0.5 || count <= 1) {
        return firstStop();
    }
    vec4 color = uStops[0];
    for (int i = 1; i < 8; ++i) {
        if (i >= count) {
            break;
        }
        float t0 = uStopAt[i - 1];
        float t1 = uStopAt[i];
        if (t <= t1 || i == count - 1) {
            float span = max(t1 - t0, 0.0001);
            return mix(uStops[i - 1], uStops[i], clamp((t - t0) / span, 0.0, 1.0));
        }
        color = uStops[i];
    }
    return color;
}

void main() {
    vec2 outerSize = max(vBox.zw, vec2(0.0));
    vec2 outerHalf = outerSize * 0.5;
    vec2 outerCenter = vBox.xy + outerHalf;
    vec2 point = vLocal - outerCenter;
    float dist = roundedDistance(point, outerHalf, vRadii);
    float cover = vParams.y > 0.5 ? clamp(0.5 - dist, 0.0, 1.0) : 1.0 - smoothstep(-0.75, 0.75, dist);

    float angle = radians(vParams.w);
    vec2 direction = vec2(sin(angle), -cos(angle));
    float span = abs(direction.x) * outerHalf.x + abs(direction.y) * outerHalf.y;
    float gradientT = span > 0.0 ? dot(point, direction) / span : 0.0;
    gradientT = clamp(gradientT * 0.5 + 0.5, 0.0, 1.0);

    float mode = vParams.x;
    vec4 color = sampleStops(gradientT);
    float alpha = cover;
    if (mode < 0.5) {
        alpha = cover;
    } else if (mode < 1.5) {
        float top = max(vBorder.x, 0.0);
        float right = max(vBorder.y, 0.0);
        float bottom = max(vBorder.z, 0.0);
        float left = max(vBorder.w, 0.0);
        vec2 innerSize = max(outerSize - vec2(left + right, top + bottom), vec2(0.0));
        vec2 innerHalf = innerSize * 0.5;
        vec2 innerCenter = vBox.xy + vec2(left, top) + innerHalf;
        vec4 innerRadii = max(vRadii - vec4(max(top, left), max(top, right), max(bottom, right), max(bottom, left)),
                              0.0);
        float inner = roundedDistance(vLocal - innerCenter, innerHalf, innerRadii);
        float innerCover = 1.0 - smoothstep(-0.75, 0.75, inner);
        alpha = clamp(cover - innerCover, 0.0, 1.0);
        color = firstStop();
    } else {
        float sigma = max(vParams.z, 0.5) * 0.5;
        vec2 shadowSize = max(vBox.zw, vec2(0.0));
        float shadow = 0.0;
        if (shadowSize.x > 0.0 && shadowSize.y > 0.0) {
            shadow = clamp(roundedBoxShadow(vBox.xy, vBox.xy + shadowSize, vLocal, sigma, vRadii), 0.0, 1.0);
        }
        float shape = roundedCoverage(vClip.xy, vClip.zw, vClipRadii, vLocal);
        alpha = mode < 2.5 ? shadow * (1.0 - shape) : (1.0 - shadow) * shape;
        color = firstStop();
    }

    color.a *= alpha;
    if (color.a <= 0.001) {
        discard;
    }
    fragColor = color;
}

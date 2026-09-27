#include "jadefx/scene/controls/ScrollTrack.hpp"

#include "jadefx/paint/Color.hpp"
#include "gl/UiRenderer.hpp"

namespace jadefx {

void ScrollTrack::draw(UiRenderer& renderer, float absoluteX, float absoluteY, float opacity, Color color) const {
    if (!visible || thumbLength <= 0.f || thickness <= 0.f) {
        return;
    }
    const float x = absoluteX + (sideways ? thumb : cross);
    const float y = absoluteY + (sideways ? cross : thumb);
    const float width = sideways ? thumbLength : thickness;
    const float height = sideways ? thickness : thumbLength;
    const float radius[4] = {};
    const float at = 0.f;
    color.a *= opacity;
    renderer.fillRounded(x, y, width, height, radius, &color, &at, 1, 0.f);
}

}  // namespace jadefx

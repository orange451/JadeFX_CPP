#include "jadefx/scene/Controls/ScrollBar.hpp"

#include "jadefx/paint/Color.hpp"
#include "gl/UiRenderer.hpp"

namespace jadefx {

void ScrollBar::draw(UiRenderer& renderer, float absoluteX, float absoluteY, float opacity) const {
    if (!visible || thumbLength <= 0.f || thickness <= 0.f) {
        return;
    }
    const float x = absoluteX + (sideways ? thumb : cross);
    const float y = absoluteY + (sideways ? cross : thumb);
    const float width = sideways ? thumbLength : thickness;
    const float height = sideways ? thickness : thumbLength;
    const float radius[4] = {};
    const float at = 0.f;
    const Color color = Color::rgba(0.f, 0.f, 0.f, 0.35f * opacity);
    renderer.fillRounded(x, y, width, height, radius, &color, &at, 1, 0.f);
}

}  // namespace jadefx

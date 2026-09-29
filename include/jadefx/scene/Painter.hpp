#pragma once

#include "jadefx/paint/Color.hpp"

#include <string>

namespace jadefx {

class UiRenderer;

// Draws for a node that paints its own content from renderContent, as a
// terminal grid or a chart does. Coordinates are window points, the same as
// getAbsoluteX and getAbsoluteY. The renderer itself stays private.
class Painter {
public:
    explicit Painter(UiRenderer& renderer) : renderer_(renderer) {}

    // Edges are exact: an edge on a whole pixel is sharp, so two rectangles that
    // share one meet without a seam, as a grid of cells needs. A pixel an edge
    // crosses is covered by the part inside.
    void fillRect(float x, float y, float width, float height, const Color& color);
    // A border as CSS draws one. radius is top left, top right, bottom right,
    // bottom left; sides are the top, right, bottom, and left widths.
    void strokeRounded(float x, float y, float width, float height, const float radius[4], const float sides[4],
                       const Color& color);
    // y is the top of the line. The family must be registered with Font.
    void text(float x, float y, const std::string& utf8, const std::string& family, float size, const Color& color,
              bool subpixel = true);
    // Clip later draws to this rectangle. Clips nest by intersection.
    void pushClip(float x, float y, float width, float height);
    void popClip();
    // Device pixels in one point, for drawing that lands on whole pixels.
    float pixelsPerPoint() const;

private:
    UiRenderer& renderer_;
};

}  // namespace jadefx

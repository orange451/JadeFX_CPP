#pragma once

#include "jadefx/scene/Node.hpp"
#include "jadefx/scene/text/Font.hpp"

// What the boxed controls draw around themselves, in the theme's colors.
namespace jadefx::chrome {

// The node's CSS font: its family and size, or Open Sans at 16 points.
Font FontOf(const Node& node);

// A theme color with the pass's opacity applied.
Color Themed(const Node& node, ThemeColor color, float opacity);

// A 1-point --border-color outline around the node, unless CSS gives it a solid border of its own.
void DrawBorder(UiRenderer& renderer, const Node& node, float opacity, float corner = 4.f);

// The 2-point --outline-color ring of a focused control.
void DrawFocusRing(UiRenderer& renderer, const Node& node, float opacity, float corner = 4.f);

// A filled 8 by 5 point triangle centered on the point, its tip toward points.
void DrawArrowHead(UiRenderer& renderer, float centerX, float centerY, Side points, const Color& color);

// The light and dark squares shown behind a translucent color, cell points on a side.
void DrawCheckerboard(UiRenderer& renderer, float x, float y, float width, float height, float opacity,
                      float cell = 6.f);

// A color over the checkerboard when it is translucent, outlined in --border-color.
void DrawColorSwatch(UiRenderer& renderer, const Node& node, float x, float y, float width, float height,
                     const Color& color, float opacity, float corner = 2.f);

// The --wash-color overlay on a hovered control, twice as strong while pressed.
void DrawWash(UiRenderer& renderer, const Node& node, float opacity, bool pressed, float corner = 4.f);

}  // namespace jadefx::chrome

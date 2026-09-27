#pragma once

#include "jadefx/scene/Node.hpp"

// What the boxed controls draw around themselves, in the theme's colors.
namespace jadefx::chrome {

// A theme color with the pass's opacity applied.
Color Themed(const Node& node, ThemeColor color, float opacity);

// A 1-point --border-color outline around the node, unless CSS gives it a solid border of its own.
void DrawBorder(UiRenderer& renderer, const Node& node, float opacity, float corner = 4.f);

// The 2-point --outline-color ring of a focused control.
void DrawFocusRing(UiRenderer& renderer, const Node& node, float opacity, float corner = 4.f);

// The --wash-color overlay on a hovered control, twice as strong while pressed.
void DrawWash(UiRenderer& renderer, const Node& node, float opacity, bool pressed, float corner = 4.f);

}  // namespace jadefx::chrome

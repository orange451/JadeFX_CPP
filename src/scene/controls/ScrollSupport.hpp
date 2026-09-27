#pragma once

#include "jadefx/scene/controls/ScrollBar.hpp"
#include "jadefx/scene/controls/ScrollPane.hpp"
#include "jadefx/scene/layout/Region.hpp"

#include <functional>
#include <memory>

// What ScrollPane and the virtualized views share: a region that clips what it
// holds, the choice of which scroll bars show, and where they go.
namespace jadefx::scroll {

// Points per wheel notch. A trackpad sends fractions of a notch.
constexpr double kWheelStep = 40;
// Points per arrow key or bar arrow step.
constexpr double kLineStep = 20;

// Draws its children clipped to its bounds. Picking stops at its bounds too, so
// a part of a child scrolled out of view cannot take a click.
class ClipRegion : public Region {
public:
    explicit ClipRegion(const char* styleClass) { getClassList().add(styleClass); }

    const char* getElementType() const override { return "viewport"; }

    ObservableList<std::shared_ptr<Node>>& items() { return children(); }

    // Runs after a child is taken by another parent.
    void setOnChildDetached(std::function<void(Node*)> handler) { onDetached_ = std::move(handler); }

    void detachChild(Node* child) override;

protected:
    void renderChildren(UiRenderer& renderer, float opacity) override;

private:
    std::function<void(Node*)> onDetached_;
};

// The bars a box needs and the viewport they leave.
struct BarLayout {
    bool horizontal = false;
    bool vertical = false;
    Size viewport;
    // The content size measure returned for that viewport.
    Size content;
};

// Each bar narrows the other axis, which can bring in the other bar. measure
// returns the content size for a viewport size.
BarLayout ResolveBars(ScrollBarPolicy horizontal, ScrollBarPolicy vertical, Size box,
                      const std::function<Size(Size viewport)>& measure);

// Sets a bar up to scroll content through a viewport, and lays it out along the
// viewport's trailing edge, kHitSlop over it. min and max are the bar's range
// and value its place in it. A bar that is not shown is hidden.
struct BarPlacement {
    bool shown = false;
    double min = 0;
    double max = 1;
    double value = 0;
    double viewport = 0;
    double content = 0;
    // The top left of the viewport, in the owner's local points.
    double left = 0;
    double top = 0;
    // The viewport's size across and along the bar.
    Size box;
};
void PlaceBar(ScrollBar& bar, const BarPlacement& placement);

// A wheel event in points to scroll by, positive toward the end. Shift turns a
// vertical wheel sideways.
Size WheelPoints(const Node& owner, const ScrollEvent& event);

// A bar that belongs to a control: hidden until placed, and never focused.
std::shared_ptr<ScrollBar> MakeOwnedBar(Orientation orientation, std::function<void(double)> onValue);

}  // namespace jadefx::scroll

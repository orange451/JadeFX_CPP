#pragma once

#include "jadefx/scene/controls/Controls.hpp"
#include "jadefx/scene/controls/ScrollTrack.hpp"

#include <functional>

namespace jadefx {

// A scroll bar, in the shape of OpenJFX ScrollBar.
// value runs from min to max. visibleAmount is how much of that range is on
// screen, and sets the thumb's share of the track. With a visibleAmount that
// covers the whole range there is nothing to scroll and no thumb is drawn.
// Dragging the thumb moves value with the pointer. A press on the track moves
// value by blockIncrement toward the press. The arrow keys move it by
// unitIncrement, and Page Up and Page Down by blockIncrement.
// ScrollPane, ListView, and TreeView own one for each axis.
class ScrollBar : public Controls {
public:
    // The thumb's breadth across the scroll axis. A bar laid out broader keeps
    // the thumb on its trailing edge, and the rest is grab margin. Owners lay
    // their bars out kHitSlop broader, over the edge of the content.
    static constexpr double kThickness = ScrollTrack::kThickness;
    static constexpr double kHitSlop = ScrollTrack::kHitSlop;

    ScrollBar();
    explicit ScrollBar(Orientation orientation);

    const char* getElementType() const override { return "scroll-bar"; }

    void setOrientation(Orientation orientation);
    Orientation getOrientation() const { return orientation_; }

    // A min above max raises max, and a max below min lowers min. value is clamped.
    void setMin(double value);
    double getMin() const { return min_; }
    void setMax(double value);
    double getMax() const { return max_; }
    // Clamped to min and max.
    void setValue(double value);
    double getValue() const { return value_; }
    // Negative values are treated as zero.
    void setVisibleAmount(double value);
    double getVisibleAmount() const { return visible_; }
    void setUnitIncrement(double value) { unit_ = value; }
    double getUnitIncrement() const { return unit_; }
    void setBlockIncrement(double value) { block_ = value; }
    double getBlockIncrement() const { return block_; }

    // True while the thumb is dragged.
    bool isValueChanging() const { return dragging_; }

    // For a bar whose range covers scrolling content of contentLength points
    // through a viewport of viewportLength points, from its start to its end:
    // sets visibleAmount to the viewport's share of the content, and
    // blockIncrement to the value one viewport scrolls.
    void setVisiblePortion(double viewportLength, double contentLength);

    // Clamps, then sets value. This is what the thumb, the track, and the keys use.
    void adjustValue(double value);
    void increment();
    void decrement();

    // Runs after value changes, from any source.
    void setOnValueChanged(std::function<void()> handler) { onChanged_ = std::move(handler); }

    void handleMousePressed(const MouseEvent& event) override;
    void handleMouseDragged(const MouseEvent& event) override;
    void handleMouseReleased(const MouseEvent& event) override;
    void handleKey(KeyEvent& event) override;

protected:
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;
    void renderContent(UiRenderer& renderer, float opacity) override;

private:
    // The thumb and track in this bar's local points.
    ScrollTrack track() const;
    // How far value sits along its range, from 0 to 1.
    double fraction() const;
    void syncPseudos();

    Orientation orientation_ = Orientation::Horizontal;
    double min_ = 0;
    double max_ = 100;
    double value_ = 0;
    double visible_ = 15;
    double unit_ = 1;
    double block_ = 10;
    bool dragging_ = false;
    float grab_ = 0.f;
    std::function<void()> onChanged_;
};

}  // namespace jadefx

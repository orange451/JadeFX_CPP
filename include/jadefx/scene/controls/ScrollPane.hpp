#pragma once

#include "jadefx/scene/controls/Controls.hpp"

#include <functional>
#include <memory>

namespace jadefx {

class ScrollBar;
class ScrollPaneViewport;

// When a ScrollPane shows a scroll bar. AsNeeded shows it while the content
// is longer than the viewport along that axis.
enum class ScrollBarPolicy { Never, Always, AsNeeded };

// Shows one node through a clipped viewport, in the shape of OpenJFX ScrollPane.
// The content keeps its preferred size, or the viewport's width or height with
// fitToWidth or fitToHeight. hvalue and vvalue place the viewport: hmin shows
// the left edge and hmax the right, and the same for vmin, vmax, top and
// bottom. They keep their place when the content or the viewport resizes.
// The wheel scrolls vertically, and horizontally with Shift or a sideways
// swipe. A wheel event the pane cannot move by goes on to its ancestors, so a
// pane nested in another hands the scroll over at its ends.
// With focus inside the pane, the arrow keys scroll by a step, Page Up and Page
// Down by a viewport, and Home and End to the top and bottom.
class ScrollPane : public Controls {
public:
    ScrollPane();
    explicit ScrollPane(std::shared_ptr<Node> content);
    ~ScrollPane() override;

    const char* getElementType() const override { return "scroll-pane"; }

    // Null clears it. A node in another parent moves here.
    void setContent(std::shared_ptr<Node> content);
    const std::shared_ptr<Node>& getContent() const { return content_; }

    void setHvalue(double value);
    double getHvalue() const { return hvalue_; }
    void setVvalue(double value);
    double getVvalue() const { return vvalue_; }
    // A min above max raises max, and a max below min lowers min. The value is clamped.
    void setHmin(double value);
    double getHmin() const { return hmin_; }
    void setHmax(double value);
    double getHmax() const { return hmax_; }
    void setVmin(double value);
    double getVmin() const { return vmin_; }
    void setVmax(double value);
    double getVmax() const { return vmax_; }

    void setFitToWidth(bool value) { fitToWidth_ = value; }
    bool isFitToWidth() const { return fitToWidth_; }
    void setFitToHeight(bool value) { fitToHeight_ = value; }
    bool isFitToHeight() const { return fitToHeight_; }

    void setHbarPolicy(ScrollBarPolicy policy) { hbarPolicy_ = policy; }
    ScrollBarPolicy getHbarPolicy() const { return hbarPolicy_; }
    void setVbarPolicy(ScrollBarPolicy policy) { vbarPolicy_ = policy; }
    ScrollBarPolicy getVbarPolicy() const { return vbarPolicy_; }

    // The viewport size this pane asks its parent for. Zero or less uses the
    // content's preferred size.
    void setPrefViewportWidth(double value) { prefViewportWidth_ = value; }
    double getPrefViewportWidth() const { return prefViewportWidth_; }
    void setPrefViewportHeight(double value) { prefViewportHeight_ = value; }
    double getPrefViewportHeight() const { return prefViewportHeight_; }

    // The viewport as of the last layout, in points.
    Size getViewportBounds() const { return viewport_; }
    // The content's laid-out size as of the last layout.
    Size getContentBounds() const { return contentSize_; }

    // Run after hvalue or vvalue changes, from the bars, the wheel, the keys, or a call.
    void setOnHvalueChanged(std::function<void()> handler) { onHvalue_ = std::move(handler); }
    void setOnVvalueChanged(std::function<void()> handler) { onVvalue_ = std::move(handler); }

    void handleScroll(ScrollEvent& event) override;
    void handleKey(KeyEvent& event) override;

protected:
    void layoutChildren() override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;

private:
    friend class ScrollPaneViewport;

    // The viewport's content was taken by another parent.
    void contentDetached();
    // Moves the viewport by points along each axis. False when neither axis moved.
    bool scrollBy(double dx, double dy);
    // The value that shows the content offset by this many points, and back.
    double valueForOffset(double offset, double maxOffset, bool horizontal) const;
    double offsetForValue(double value, double maxOffset, bool horizontal) const;
    void changeHvalue(double value);
    void changeVvalue(double value);
    bool showsBar(ScrollBarPolicy policy, double content, double viewport) const;

    std::shared_ptr<ScrollPaneViewport> viewportNode_;
    std::shared_ptr<ScrollBar> hbar_;
    std::shared_ptr<ScrollBar> vbar_;
    std::shared_ptr<Node> content_;
    double hvalue_ = 0;
    double vvalue_ = 0;
    double hmin_ = 0;
    double hmax_ = 1;
    double vmin_ = 0;
    double vmax_ = 1;
    bool fitToWidth_ = false;
    bool fitToHeight_ = false;
    ScrollBarPolicy hbarPolicy_ = ScrollBarPolicy::AsNeeded;
    ScrollBarPolicy vbarPolicy_ = ScrollBarPolicy::AsNeeded;
    double prefViewportWidth_ = 0;
    double prefViewportHeight_ = 0;
    Size viewport_;
    Size contentSize_;
    // How far the content can move along each axis, as of the last layout.
    Size scrollRange_;
    std::function<void()> onHvalue_;
    std::function<void()> onVvalue_;
};

}  // namespace jadefx

#pragma once

#include "jadefx/scene/Controls/Controls.hpp"

#include <memory>

namespace jadefx {

class ProgressFill;

// A horizontal progress bar, in the shape of JavaFX ProgressBar.
// Progress below 0 is indeterminate: a short bar travels the track and turns
// around. A value from 0 to 1 fills that fraction of the track, and a value
// above 1 fills it. getProgress returns the value that was set.
// The fill width snaps to half a pixel, the same truncation JavaFX uses.
// Preferred width is at least 100. Preferred height is the bar's height.
// The bar's padding defaults to 0.75em on every side, so the track is 1.5em tall.
// Maximum size starts at that preferred size, so the bar does not stretch
// until setPrefWidth, setMaxWidth, or a stylesheet changes it.
// The control type is progress-bar. The groove is track and the fill is bar.
// :determinate and :indeterminate follow the progress.
// indeterminate-bar-length, indeterminate-bar-escape, indeterminate-bar-flip,
// and indeterminate-bar-animation-time style the traveling bar. Defaults are
// 60px, escaping past both ends, flipping at the ends, and 2 seconds each way.
// The motion eases in and out, matching JavaFX's ease-both transition.
class ProgressBar : public Controls {
public:
    static constexpr double INDETERMINATE_PROGRESS = -1;

    ProgressBar();
    explicit ProgressBar(double progress);

    const char* getElementType() const override { return "progress-bar"; }

    void setProgress(double value);
    double getProgress() const { return progress_; }
    // NaN is determinate. JavaFX treats only a negative progress as indeterminate.
    bool isIndeterminate() const { return progress_ < 0.0; }

    void setIndeterminateBarLength(double value);
    double getIndeterminateBarLength() const { return usedLength_; }
    void setIndeterminateBarEscape(bool value);
    bool isIndeterminateBarEscape() const { return usedEscape_; }
    void setIndeterminateBarFlip(bool value);
    bool isIndeterminateBarFlip() const { return usedFlip_; }
    void setIndeterminateBarAnimationTime(double seconds);
    double getIndeterminateBarAnimationTime() const { return usedAnimationTime_; }

    // The groove and the fill. Their types are track and bar.
    Node* getTrack() const { return track_.get(); }
    Node* getBar() const { return bar_.get(); }

protected:
    void layoutChildren() override;
    void renderChildren(UiRenderer& renderer, float opacity) override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;

private:
    friend class ProgressFill;

    void syncPseudos();
    void applyDefaultMaximum();
    void useSkinValues(double span);

    double progress_ = INDETERMINATE_PROGRESS;
    SizeSpec lengthSpec_ = SizeSpec::px(60);
    bool escape_ = true;
    bool flip_ = true;
    double animationTime_ = 2;
    double usedLength_ = 60;
    bool usedEscape_ = true;
    bool usedFlip_ = true;
    double usedAnimationTime_ = 2;
    bool barUsesDefaultFill_ = true;

    std::shared_ptr<Node> track_;
    std::shared_ptr<Node> bar_;
};

}  // namespace jadefx

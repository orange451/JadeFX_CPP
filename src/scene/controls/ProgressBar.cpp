#include "jadefx/scene/controls/ProgressBar.hpp"

#include "gl/UiRenderer.hpp"
#include "jadefx/scene/Scene.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace jadefx {
namespace {

const Color kAccent = Color::rgb8(26, 115, 232);
const Color kTrackFill = Color::rgb8(241, 243, 244);
const Color kTrackBorder = Color::rgb8(218, 220, 224);

// JavaFX Transition uses Interpolator.EASE_BOTH: SMIL ease with acceleration
// and deceleration of 0.2.
double EaseBoth(double t) {
    if (t < 0) {
        t = 0;
    }
    if (t > 1) {
        t = 1;
    }
    double curved = 1.25 * t - 0.125;
    if (t < 0.2) {
        curved = 3.125 * t * t;
    } else if (t > 0.8) {
        curved = -3.125 * t * t + 6.25 * t - 2.125;
    }
    if (curved < 0) {
        return 0;
    }
    if (curved > 1) {
        return 1;
    }
    return curved;
}

// ((int)(span * 2 * progress)) / 2, with progress clamped to 0..1.
// Truncation matches the Java cast. A non-finite progress draws an empty bar.
double SnapBarWidth(double span, double progress) {
    double visual = progress;
    if (!(visual > 0.0)) {
        visual = 0;
    }
    if (visual > 1.0) {
        visual = 1;
    }
    if (!(span > 0.0)) {
        return 0;
    }
    const double scaled = span * 2.0 * visual;
    if (scaled >= static_cast<double>(std::numeric_limits<int>::max())) {
        return span * visual;
    }
    return static_cast<double>(static_cast<int>(scaled)) / 2.0;
}

struct Travel {
    double translate = 0;
    bool mirrored = false;
};

// One cycle of the OpenJFX indeterminate transition. flip makes the cycle
// twice the animation time: forward while the fraction is at most 0.5, then
// back. The bar is mirrored on the way forward so the bright end leads.
Travel IndeterminateTravel(double time, double span, double length, bool escape, bool flip, double seconds) {
    const double startX = escape ? -length : 0;
    const double endX = escape ? span : span - length;
    const double cycle = seconds * (flip ? 2.0 : 1.0);
    double raw = 0;
    if (cycle > 0.0 && time > 0.0) {
        raw = std::fmod(time, cycle) / cycle;
        if (raw < 0) {
            raw += 1;
        }
    }
    const double frac = EaseBoth(raw);
    const double delta = endX - startX;
    Travel travel;
    if (frac <= 0.5 || !flip) {
        travel.mirrored = true;
        travel.translate = startX + (flip ? 2.0 : 1.0) * frac * delta;
    } else {
        travel.mirrored = false;
        travel.translate = startX + 2.0 * (1.0 - frac) * delta;
    }
    return travel;
}

bool RadiusUnset(const ComputedStyle& style) {
    for (const SizeSpec& radius : style.radius) {
        if (radius.set()) {
            return false;
        }
    }
    return true;
}

bool InsetsEmpty(const Insets& insets) {
    return insets.top == 0 && insets.right == 0 && insets.bottom == 0 && insets.left == 0;
}

}  // namespace

class ProgressTrack : public Region {
public:
    ProgressTrack() {
        setBackground(kTrackFill);
        setBorder(Insets::uniform(1));
    }

    const char* getElementType() const override { return "track"; }

protected:
    void styleDidApply() override {
        ComputedStyle& style = computed();
        // The width comes from setBorder, so a stylesheet border-width of 0 removes it.
        if (style.borderStyle == BorderStyle::None && !InsetsEmpty(style.border)) {
            style.borderStyle = BorderStyle::Solid;
            if (style.borderColor.a <= 0.f) {
                style.borderColor = kTrackBorder;
            }
        }
        if (RadiusUnset(style)) {
            for (SizeSpec& radius : style.radius) {
                radius = SizeSpec::px(4);
            }
        }
    }
};

class ProgressFill : public Region {
public:
    explicit ProgressFill(ProgressBar& owner) : owner_(owner) {}

    const char* getElementType() const override { return "bar"; }

    void setTravelFill(bool mirrored) {
        ComputedStyle& style = computed();
        style.background.hasColor = false;
        style.background.color = Color::transparent();
        style.background.gradient = true;
        style.background.visible = true;
        style.background.stopCount = 2;
        style.background.stops[0] = Color::transparent();
        style.background.stops[1] = kAccent;
        style.background.stopAt[0] = 0;
        style.background.stopAt[1] = 1;
        // to right when mirrored (bright end leads on the way forward), else to left.
        style.background.angleDeg = mirrored ? 90.f : 270.f;
    }

protected:
    void styleDidApply() override {
        ComputedStyle& style = computed();
        owner_.barUsesDefaultFill_ = !style.background.visible;
        if (owner_.barUsesDefaultFill_) {
            style.background.color = kAccent;
            style.background.hasColor = true;
            style.background.gradient = false;
            style.background.stopCount = 0;
            style.background.visible = true;
        }
        if (InsetsEmpty(style.padding)) {
            const double pad = 0.75 * static_cast<double>(style.fontSize);
            style.padding = Insets::uniform(pad);
        }
        if (RadiusUnset(style)) {
            for (SizeSpec& radius : style.radius) {
                radius = SizeSpec::px(2);
            }
        }
        owner_.applyDefaultMaximum();
    }

private:
    ProgressBar& owner_;
};

ProgressBar::ProgressBar() : ProgressBar(INDETERMINATE_PROGRESS) {}

ProgressBar::ProgressBar(double progress) : progress_(progress) {
    syncPseudos();
    track_ = std::make_shared<ProgressTrack>();
    bar_ = std::make_shared<ProgressFill>(*this);
    track_->setMouseTransparent(true);
    bar_->setMouseTransparent(true);
    children().add(track_);
    children().add(bar_);
}

void ProgressBar::setProgress(double value) {
    if (progress_ == value) {
        return;
    }
    progress_ = value;
    syncPseudos();
}

void ProgressBar::setIndeterminateBarLength(double value) {
    if (!(value > 0)) {
        value = 0;
    }
    lengthSpec_ = SizeSpec::px(value);
    usedLength_ = value;
}

void ProgressBar::setIndeterminateBarEscape(bool value) {
    escape_ = value;
    usedEscape_ = value;
}

void ProgressBar::setIndeterminateBarFlip(bool value) {
    flip_ = value;
    usedFlip_ = value;
}

void ProgressBar::setIndeterminateBarAnimationTime(double seconds) {
    if (!(seconds > 0)) {
        seconds = 0;
    }
    animationTime_ = seconds;
    usedAnimationTime_ = seconds;
}

void ProgressBar::syncPseudos() {
    const bool indeterminate = isIndeterminate();
    setPseudoState("indeterminate", indeterminate);
    setPseudoState("determinate", !indeterminate);
}

void ProgressBar::useSkinValues(double span) {
    const ComputedStyle& style = computedStyle();
    const SizeSpec& length = style.indeterminateBarLengthSet ? style.indeterminateBarLength : lengthSpec_;
    double resolved = resolveSize(length, std::max(0.0, span), style.fontSize);
    if (!(resolved > 0)) {
        resolved = 0;
    }
    usedLength_ = resolved;
    usedEscape_ = style.indeterminateBarEscapeSet ? style.indeterminateBarEscape : escape_;
    usedFlip_ = style.indeterminateBarFlipSet ? style.indeterminateBarFlip : flip_;
    if (style.indeterminateBarAnimationTimeSet) {
        usedAnimationTime_ = style.indeterminateBarAnimationTime > 0 ? style.indeterminateBarAnimationTime : 0;
    } else if (!(animationTime_ > 0)) {
        usedAnimationTime_ = 0;
    } else {
        usedAnimationTime_ = animationTime_;
    }
}

void ProgressBar::applyDefaultMaximum() {
    // OpenJFX reports the preferred size as the maximum, so a bar in a
    // stretching slot stays at its preferred size until a max or width is set.
    ComputedStyle& style = computed();
    if (!style.width.set() && !style.maxWidth.set()) {
        style.maxWidth = SizeSpec::px(measuredWidth(0));
    }
    if (!style.height.set() && !style.maxHeight.set()) {
        const double width = measuredWidth(0);
        style.maxHeight = SizeSpec::px(measuredHeight(width, -1));
    }
}

double ProgressBar::preferredContentWidth(double innerAvailable) const {
    const double pad = computedStyle().padding.width() + computedStyle().border.width();
    const double barWidth = bar_ ? bar_->measuredWidth(innerAvailable) : 0;
    return std::max(0.0, std::max(100.0 - pad, barWidth));
}

double ProgressBar::preferredContentHeight(double innerWidth) const {
    return bar_ ? bar_->measuredHeight(innerWidth, -1) : 0;
}

void ProgressBar::layoutChildren() {
    const double left = contentLeft();
    const double top = contentTop();
    const double span = contentWidth();
    const double breadth = contentHeight();
    useSkinValues(span);

    if (track_ != nullptr) {
        track_->setTranslateX(0);
        track_->performLayout(left, top, span, breadth);
    }
    if (bar_ == nullptr) {
        return;
    }

    double length = 0;
    double travel = 0;
    bool mirrored = false;
    if (isIndeterminate()) {
        length = usedLength_;
        const double time = getScene() != nullptr ? getScene()->timeSeconds() : 0;
        const Travel motion =
            IndeterminateTravel(time, span, length, usedEscape_, usedFlip_, usedAnimationTime_);
        travel = motion.translate;
        mirrored = motion.mirrored;
    } else {
        length = SnapBarWidth(span, progress_);
    }
    bar_->setTranslateX(travel);
    bar_->performLayout(left, top, length, breadth);
    if (isIndeterminate() && barUsesDefaultFill_) {
        static_cast<ProgressFill*>(bar_.get())->setTravelFill(mirrored);
    }
}

void ProgressBar::renderChildren(UiRenderer& renderer, float opacity) {
    if (track_ != nullptr) {
        track_->render(renderer, opacity);
    }
    if (bar_ == nullptr) {
        return;
    }
    const float x = static_cast<float>(getAbsoluteX() + contentLeft());
    const float y = static_cast<float>(getAbsoluteY() + contentTop());
    const float w = static_cast<float>(contentWidth());
    const float h = static_cast<float>(contentHeight());
    renderer.pushClip(x, y, w, h);
    bar_->render(renderer, opacity);
    renderer.popClip();
}

}  // namespace jadefx

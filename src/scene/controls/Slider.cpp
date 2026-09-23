#include "jadefx/scene/controls/Slider.hpp"

#include "gl/UiRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

namespace jadefx {
namespace {

constexpr double kThumb = 16;
constexpr double kTrack = 4;
constexpr double kTickGap = 2;
constexpr double kMajorTick = 8;
constexpr double kMinorTick = 4;
constexpr double kPrefSpan = 140;
constexpr int kMaxTicks = 256;

double Clamp(double min, double value, double max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

// A tie lands on the higher tick, matching OpenJFX Utils.nearest.
double Nearest(double less, double value, double more) {
    if (value - less < more - value) {
        return less;
    }
    return more;
}

std::string TrimmedNumber(double value) {
    if (!std::isfinite(value)) {
        return {};
    }
    const double rounded = std::round(value);
    if (std::fabs(value - rounded) < 1e-6 && std::fabs(rounded) < 1e15) {
        return std::to_string(static_cast<long long>(rounded));
    }
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(4);
    out << value;
    std::string text = out.str();
    const std::size_t dot = text.find('.');
    if (dot != std::string::npos) {
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
    }
    return text;
}

Font FaceOf(const Slider& slider) {
    const ComputedStyle& style = slider.computedStyle();
    const float size = style.fontSize > 0.f ? style.fontSize : 16.f;
    return Font(style.fontFamily.empty() ? "Open Sans" : style.fontFamily, size);
}

}  // namespace

struct Slider::Track {
    bool horizontal = true;
    // Absolute position of the thumb center at the low end of the travel.
    // For a vertical slider that end is the bottom, while alongStart is the top.
    double alongStart = 0;
    double alongLength = 0;
    double crossCenter = 0;
    double thumbAlong = 0;
};

Slider::Slider() : Slider(0, 100, 0) {}

Slider::Slider(double min, double max, double value) {
    setMax(max);
    setMin(min);
    setValue(value);
    setOrientation(Orientation::Horizontal);
}

void Slider::setMin(double value) {
    if (!std::isfinite(value)) {
        return;
    }
    min_ = value;
    if (min_ > max_) {
        max_ = min_;
    }
    clampValue();
}

void Slider::setMax(double value) {
    if (!std::isfinite(value)) {
        return;
    }
    max_ = value;
    if (max_ < min_) {
        min_ = max_;
    }
    clampValue();
}

void Slider::setValue(double value) {
    if (!std::isfinite(value)) {
        return;
    }
    value = Clamp(min_, value, max_);
    if (value == value_) {
        return;
    }
    value_ = value;
    changed();
}

void Slider::setValueChanging(bool value) {
    if (valueChanging_ == value) {
        return;
    }
    valueChanging_ = value;
    changed();
}

void Slider::setOrientation(Orientation orientation) {
    orientation_ = orientation;
    syncPseudos();
}

void Slider::setMajorTickUnit(double value) {
    if (!std::isfinite(value) || !(value > 0)) {
        return;
    }
    major_ = value;
}

void Slider::setBlockIncrement(double value) {
    if (!std::isfinite(value)) {
        return;
    }
    block_ = value;
}

void Slider::setSnapToTicks(bool value) {
    if (snap_ == value) {
        return;
    }
    snap_ = value;
    if (snap_) {
        adjustValue(value_);
    }
}

void Slider::adjustValue(double value) {
    if (!(max_ > min_) || !std::isfinite(value)) {
        return;
    }
    setValue(snap(Clamp(min_, value, max_)));
}

void Slider::increment() { adjustValue(value_ + block_); }

void Slider::decrement() { adjustValue(value_ - block_); }

void Slider::styleDidApply() {
    if (computed().orientationFromCss) {
        setOrientation(computed().orientation);
    }
}

void Slider::clampValue() {
    const double next = Clamp(min_, value_, max_);
    if (next == value_) {
        return;
    }
    value_ = next;
    changed();
}

void Slider::changed() {
    if (onChanged_) {
        onChanged_();
    }
}

void Slider::syncPseudos() {
    const bool vertical = orientation_ == Orientation::Vertical;
    setPseudoState("vertical", vertical);
    setPseudoState("horizontal", !vertical);
}

void Slider::finishThumbDrag() {
    if (!draggingThumb_) {
        draggingTrack_ = false;
        return;
    }
    draggingThumb_ = false;
    draggingTrack_ = false;
    setValueChanging(false);
    adjustValue(value_);
}

double Slider::snap(double value) const {
    value = Clamp(min_, value, max_);
    if (!snap_) {
        return value;
    }
    double spacing = major_;
    if (minor_ != 0) {
        spacing = major_ / (static_cast<double>(std::max(minor_, 0)) + 1.0);
    }
    if (!(spacing > 0) || !std::isfinite(spacing)) {
        return value;
    }
    const double quotient = (value - min_) / spacing;
    const double prevTick = std::trunc(quotient);
    const double prev = prevTick * spacing + min_;
    const double next = (prevTick + 1.0) * spacing + min_;
    return Clamp(min_, Nearest(prev, value, next), max_);
}

double Slider::fraction() const {
    if (!(max_ > min_)) {
        return 0;
    }
    return (value_ - min_) / (max_ - min_);
}

double Slider::tickExtra() const {
    double extra = 0;
    if (!showMarks_ && !showLabels_) {
        return extra;
    }
    extra += kTickGap;
    if (showMarks_) {
        extra += kMajorTick;
    }
    if (showLabels_) {
        if (orientation_ == Orientation::Horizontal) {
            extra += FaceOf(*this).lineHeight();
        } else {
            double widest = 0;
            const Font face = FaceOf(*this);
            const double span = max_ - min_;
            int guard = 0;
            for (double tick = min_; tick <= max_ + major_ * 1e-6 && guard < kMaxTicks; tick += major_, ++guard) {
                if (!(span > 0) && guard > 0) {
                    break;
                }
                widest = std::max(widest, static_cast<double>(face.measureWidth(labelFor(tick))));
                if (!(major_ > 0)) {
                    break;
                }
            }
            extra += widest;
        }
    }
    return extra;
}

Slider::Track Slider::track() const {
    Track metrics;
    metrics.horizontal = orientation_ != Orientation::Vertical;
    const double alongSpan = metrics.horizontal ? contentWidth() : contentHeight();
    const double crossSpan = metrics.horizontal ? contentHeight() : contentWidth();
    const double alongOrigin = metrics.horizontal ? getAbsoluteX() + contentLeft() : getAbsoluteY() + contentTop();
    const double crossOrigin = metrics.horizontal ? getAbsoluteY() + contentTop() : getAbsoluteX() + contentLeft();
    metrics.alongLength = std::max(0.0, alongSpan - kThumb);
    metrics.alongStart = alongOrigin + kThumb * 0.5;
    const double extra = tickExtra();
    const double block = kThumb + extra;
    const double blockStart = crossOrigin + std::max(0.0, crossSpan - block) * 0.5;
    metrics.crossCenter = blockStart + kThumb * 0.5;
    const double t = fraction();
    metrics.thumbAlong = metrics.horizontal ? metrics.alongStart + t * metrics.alongLength
                                            : metrics.alongStart + (1.0 - t) * metrics.alongLength;
    return metrics;
}

double Slider::valueForPosition(double position) const {
    if (position < 0) {
        position = 0;
    }
    if (position > 1) {
        position = 1;
    }
    const double span = max_ - min_;
    if (orientation_ == Orientation::Vertical) {
        return min_ + (1.0 - position) * span;
    }
    return min_ + position * span;
}

bool Slider::hitsThumb(double x, double y, const Track& metrics) const {
    const double along = metrics.horizontal ? x : y;
    const double cross = metrics.horizontal ? y : x;
    const double da = along - metrics.thumbAlong;
    const double dc = cross - metrics.crossCenter;
    const double radius = kThumb * 0.5;
    return da * da + dc * dc <= radius * radius;
}

bool Slider::hitsTrack(double x, double y, const Track& metrics) const {
    const double along = metrics.horizontal ? x : y;
    const double cross = metrics.horizontal ? y : x;
    if (std::fabs(cross - metrics.crossCenter) > kThumb * 0.5) {
        return false;
    }
    const double lead = metrics.horizontal ? getAbsoluteX() : getAbsoluteY();
    const double span = metrics.horizontal ? getWidth() : getHeight();
    return along >= lead && along <= lead + span;
}

std::string Slider::labelFor(double value) const {
    if (formatter_) {
        return formatter_(value);
    }
    return TrimmedNumber(value);
}

double Slider::preferredContentWidth(double) const {
    if (orientation_ != Orientation::Vertical) {
        return kPrefSpan;
    }
    return kThumb + tickExtra();
}

double Slider::preferredContentHeight(double) const {
    if (orientation_ == Orientation::Vertical) {
        return kPrefSpan;
    }
    return kThumb + tickExtra();
}

void Slider::handleMousePressed(const MouseEvent& event) {
    if (isDisabled()) {
        return;
    }
    draggingThumb_ = false;
    draggingTrack_ = false;
    const Track metrics = track();
    if (hitsThumb(event.x, event.y, metrics)) {
        draggingThumb_ = true;
        pressAlong_ = metrics.horizontal ? event.x : event.y;
        pressFraction_ = fraction();
        setValueChanging(true);
        return;
    }
    if (!hitsTrack(event.x, event.y, metrics)) {
        return;
    }
    draggingTrack_ = true;
    const double along = metrics.horizontal ? event.x : event.y;
    const double position = metrics.alongLength > 0 ? (along - metrics.alongStart) / metrics.alongLength : 0;
    adjustValue(valueForPosition(position));
}

void Slider::handleMouseDragged(const MouseEvent& event) {
    if (isDisabled()) {
        return;
    }
    const Track metrics = track();
    const double along = metrics.horizontal ? event.x : event.y;
    if (draggingThumb_) {
        const double delta = metrics.horizontal ? along - pressAlong_ : pressAlong_ - along;
        double next = pressFraction_;
        if (metrics.alongLength > 0) {
            next += delta / metrics.alongLength;
        }
        if (next < 0) {
            next = 0;
        }
        if (next > 1) {
            next = 1;
        }
        setValue(min_ + next * (max_ - min_));
        return;
    }
    if (!draggingTrack_) {
        return;
    }
    const double position = metrics.alongLength > 0 ? (along - metrics.alongStart) / metrics.alongLength : 0;
    adjustValue(valueForPosition(position));
}

void Slider::handleMouseReleased(const MouseEvent&) {
    if (draggingThumb_) {
        finishThumbDrag();
        return;
    }
    draggingTrack_ = false;
}

void Slider::handleKey(KeyEvent& event) {
    if (isDisabled()) {
        return;
    }
    const bool home = event.key == Key::Home;
    const bool end = event.key == Key::End;
    if (!event.pressed) {
        if (home) {
            adjustValue(min_);
            event.consume();
        } else if (end) {
            adjustValue(max_);
            event.consume();
        }
        return;
    }
    const bool vertical = orientation_ == Orientation::Vertical;
    const bool decrease = vertical ? (event.key == Key::Down) : (event.key == Key::Left);
    const bool increase = vertical ? (event.key == Key::Up) : (event.key == Key::Right);
    if (!decrease && !increase) {
        return;
    }
    if (snap_) {
        double spacing = major_;
        if (minor_ != 0) {
            spacing = major_ / (static_cast<double>(std::max(minor_, 0)) + 1.0);
        }
        double step = block_;
        if (block_ > 0 && spacing > 0 && block_ < spacing) {
            step = spacing;
        }
        adjustValue(decrease ? value_ - step : value_ + step);
    } else if (decrease) {
        decrement();
    } else {
        increment();
    }
    event.consume();
}

void Slider::sceneChanged(Scene*) {
    if (isTearingDown()) {
        return;
    }
    finishThumbDrag();
    draggingTrack_ = false;
}

void Slider::render(UiRenderer& renderer, float opacity) {
    Node::render(renderer, isDisabled() ? opacity * 0.45f : opacity);
}

void Slider::renderContent(UiRenderer& renderer, float opacity) {
    const Track metrics = track();
    const float radius = static_cast<float>(kThumb * 0.5);
    const float trackRadius = static_cast<float>(kTrack * 0.5);
    const float pill[4] = {trackRadius, trackRadius, trackRadius, trackRadius};
    const float at = 0.f;
    const float along0 = static_cast<float>(metrics.alongStart);
    const float along1 = static_cast<float>(metrics.alongStart + metrics.alongLength);
    const float cross = static_cast<float>(metrics.crossCenter);
    if (metrics.alongLength > 0) {
        Color groove = Color::rgb8(218, 220, 224);
        groove.a *= opacity;
        if (metrics.horizontal) {
            renderer.fillRounded(along0, cross - trackRadius, along1 - along0, static_cast<float>(kTrack), pill, &groove,
                                 &at, 1, 0.f);
        } else {
            renderer.fillRounded(cross - trackRadius, along0, static_cast<float>(kTrack), along1 - along0, pill, &groove,
                                 &at, 1, 0.f);
        }
    }

    if ((showMarks_ || showLabels_) && major_ > 0 && max_ >= min_) {
        const Font face = FaceOf(*this);
        Color mark = Color::rgb8(95, 99, 104);
        mark.a *= opacity;
        Color minorMark = Color::rgb8(128, 134, 139);
        minorMark.a *= opacity;
        Color text = computedStyle().color;
        text.a *= opacity;
        const float square[4] = {};
        const int minors = std::max(minor_, 0);
        const double minorSpacing = major_ / (static_cast<double>(minors) + 1.0);
        int guard = 0;
        for (double tick = min_; tick <= max_ + major_ * 1e-6 && guard < kMaxTicks; tick += major_, ++guard) {
            const double tickFraction = max_ > min_ ? (tick - min_) / (max_ - min_) : 0;
            const double along = metrics.horizontal ? metrics.alongStart + tickFraction * metrics.alongLength
                                                    : metrics.alongStart + (1.0 - tickFraction) * metrics.alongLength;
            if (showMarks_) {
                if (metrics.horizontal) {
                    const float x = static_cast<float>(along);
                    const float y = cross + radius + static_cast<float>(kTickGap);
                    renderer.fillRounded(x, y, 1.f, static_cast<float>(kMajorTick), square, &mark, &at, 1, 0.f);
                } else {
                    const float y = static_cast<float>(along);
                    const float x = cross + radius + static_cast<float>(kTickGap);
                    renderer.fillRounded(x, y, static_cast<float>(kMajorTick), 1.f, square, &mark, &at, 1, 0.f);
                }
            }
            if (showLabels_) {
                const std::string textValue = labelFor(tick);
                const float textWidth = face.measureWidth(textValue);
                const float textHeight = face.lineHeight();
                if (metrics.horizontal) {
                    const float x = static_cast<float>(along) - textWidth * 0.5f;
                    const float y = cross + radius + static_cast<float>(kTickGap + (showMarks_ ? kMajorTick : 0));
                    renderer.text(x, y, textValue, face.family(), face.size(), text, computedStyle().subpixel);
                } else {
                    const float x = cross + radius + static_cast<float>(kTickGap + (showMarks_ ? kMajorTick : 0));
                    const float y = static_cast<float>(along) - textHeight * 0.5f;
                    renderer.text(x, y, textValue, face.family(), face.size(), text, computedStyle().subpixel);
                }
            }
            if (!(major_ > 0)) {
                break;
            }
            if (!showMarks_ || minors <= 0) {
                continue;
            }
            for (int i = 1; i <= minors; ++i) {
                const double minorValue = tick + minorSpacing * static_cast<double>(i);
                if (minorValue >= max_ - major_ * 1e-6) {
                    break;
                }
                const double minorFraction = max_ > min_ ? (minorValue - min_) / (max_ - min_) : 0;
                const double minorAlong = metrics.horizontal
                                               ? metrics.alongStart + minorFraction * metrics.alongLength
                                               : metrics.alongStart + (1.0 - minorFraction) * metrics.alongLength;
                if (metrics.horizontal) {
                    const float x = static_cast<float>(minorAlong);
                    const float y = cross + radius + static_cast<float>(kTickGap + (kMajorTick - kMinorTick));
                    renderer.fillRounded(x, y, 1.f, static_cast<float>(kMinorTick), square, &minorMark, &at, 1, 0.f);
                } else {
                    const float y = static_cast<float>(minorAlong);
                    const float x = cross + radius + static_cast<float>(kTickGap + (kMajorTick - kMinorTick));
                    renderer.fillRounded(x, y, static_cast<float>(kMinorTick), 1.f, square, &minorMark, &at, 1, 0.f);
                }
            }
        }
    }

    Color fill = Color::white();
    Color line = (isFocused() || isHovered() || isPressed()) ? Color::rgb8(26, 115, 232) : Color::rgb8(95, 99, 104);
    if (isPressed() && draggingThumb_) {
        fill = Color::rgb8(232, 240, 254);
    }
    fill.a *= opacity;
    line.a *= opacity;
    const float thumbRadius[4] = {radius, radius, radius, radius};
    const float sides[4] = {2.f, 2.f, 2.f, 2.f};
    const float thumbX = metrics.horizontal ? static_cast<float>(metrics.thumbAlong - radius)
                                            : static_cast<float>(metrics.crossCenter - radius);
    const float thumbY = metrics.horizontal ? static_cast<float>(metrics.crossCenter - radius)
                                            : static_cast<float>(metrics.thumbAlong - radius);
    renderer.fillRounded(thumbX, thumbY, radius * 2.f, radius * 2.f, thumbRadius, &fill, &at, 1, 0.f);
    renderer.strokeRounded(thumbX, thumbY, radius * 2.f, radius * 2.f, thumbRadius, sides, line);
}

}  // namespace jadefx

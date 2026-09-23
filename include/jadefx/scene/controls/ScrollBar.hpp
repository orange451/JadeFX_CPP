#pragma once

#include <algorithm>

namespace jadefx {

class UiRenderer;

// The scrollbar used by TreeView and StyledTextArea.
// An 8px thumb tracks a drag. A click on the track pages by one viewport.
// Coordinates are local to the control, with the origin at its top left.
struct ScrollBar {
    static constexpr float kThickness = 8.f;
    static constexpr float kMinThumb = 18.f;
    static constexpr float kHitSlop = 2.f;

    bool visible = false;
    bool sideways = false;
    // cross is x for a vertical bar and y for a horizontal bar.
    // origin is the start of the track along the scroll axis.
    float cross = 0.f;
    float origin = 0.f;
    float thickness = kThickness;
    float track = 0.f;
    float thumb = 0.f;
    float thumbLength = 0.f;
    float content = 0.f;
    float viewport = 0.f;

    enum class Part { None, Thumb, Before, After };

    static ScrollBar vertical(float x, float y, float trackLength, float content, float viewport, double offset) {
        return make(false, x, y, trackLength, content, viewport, offset);
    }

    static ScrollBar horizontal(float x, float y, float trackLength, float content, float viewport, double offset) {
        return make(true, y, x, trackLength, content, viewport, offset);
    }

    float maxOffset() const { return std::max(0.f, content - viewport); }

    Part part(float localX, float localY) const {
        if (!visible) {
            return Part::None;
        }
        const float along = sideways ? localX : localY;
        const float other = sideways ? localY : localX;
        if (other < cross - kHitSlop || other > cross + thickness + kHitSlop) {
            return Part::None;
        }
        if (along < origin || along > origin + track) {
            return Part::None;
        }
        if (along >= thumb && along <= thumb + thumbLength) {
            return Part::Thumb;
        }
        return along < thumb ? Part::Before : Part::After;
    }

    // grab is the pointer's distance from the thumb's leading edge at the press.
    double offsetFromDrag(float localX, float localY, float grab) const {
        const float along = sideways ? localX : localY;
        const float travel = std::max(1.f, track - thumbLength);
        const float pos = along - grab - origin;
        const float t = std::max(0.f, std::min(1.f, pos / travel));
        return static_cast<double>(maxOffset()) * static_cast<double>(t);
    }

    double offsetFromPage(double offset, bool forward) const {
        double next = offset + (forward ? static_cast<double>(viewport) : -static_cast<double>(viewport));
        const double limit = maxOffset();
        if (next < 0.0) {
            next = 0.0;
        }
        if (next > limit) {
            next = limit;
        }
        return next;
    }

    void draw(UiRenderer& renderer, float absoluteX, float absoluteY, float opacity) const;

private:
    static ScrollBar make(bool horizontal, float cross, float origin, float trackLength, float content, float viewport,
                          double offset) {
        ScrollBar bar;
        bar.sideways = horizontal;
        bar.cross = cross;
        bar.origin = origin;
        bar.track = std::max(0.f, trackLength);
        bar.content = std::max(0.f, content);
        bar.viewport = std::max(0.f, viewport);
        bar.visible = bar.content > bar.viewport + 0.5f && bar.content > 0.f && bar.track > 0.f;
        if (!bar.visible) {
            return bar;
        }
        bar.thumbLength = std::max(kMinThumb, bar.track * (bar.track / bar.content));
        if (bar.thumbLength > bar.track) {
            bar.thumbLength = bar.track;
        }
        const float travel = std::max(0.f, bar.track - bar.thumbLength);
        const float limit = bar.maxOffset();
        const float ratio = limit <= 0.f ? 0.f : static_cast<float>(offset / static_cast<double>(limit));
        bar.thumb = bar.origin + travel * std::max(0.f, std::min(1.f, ratio));
        return bar;
    }
};

}  // namespace jadefx

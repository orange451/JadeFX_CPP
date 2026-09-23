#pragma once

#include "jadefx/scene/controls/Controls.hpp"

#include <functional>
#include <string>

namespace jadefx {

// A ranged thumb on a track, in the shape of OpenJFX Slider.
// value stays inside min and max. A min above max raises max, and a max below
// min lowers min. setValue stores the number as given. adjustValue is the call
// that snaps, and it is what the track and the keys use.
// Dragging the thumb sets valueChanging until the pointer is released, and the
// value snaps on that release when snapToTicks is on. A track click or drag
// jumps to the pointer.
// Left and Right move a horizontal slider. Up and Down move a vertical one,
// with Up toward max. Home and End run when the key is released.
// Tick marks and labels sit under a horizontal track and to the right of a
// vertical one. The thumb is 16 points across, and the travel is the content
// span minus that.
class Slider : public Controls {
public:
    Slider();
    Slider(double min, double max, double value);

    const char* getElementType() const override { return "slider"; }

    void setMin(double value);
    double getMin() const { return min_; }
    void setMax(double value);
    double getMax() const { return max_; }
    // Clamped to min and max. Does not snap.
    void setValue(double value);
    double getValue() const { return value_; }

    void setValueChanging(bool value);
    bool isValueChanging() const { return valueChanging_; }

    void setOrientation(Orientation orientation);
    Orientation getOrientation() const { return orientation_; }

    void setShowTickLabels(bool value) { showLabels_ = value; }
    bool isShowTickLabels() const { return showLabels_; }
    void setShowTickMarks(bool value) { showMarks_ = value; }
    bool isShowTickMarks() const { return showMarks_; }

    // Non-positive values are ignored. The distance is in value units.
    void setMajorTickUnit(double value);
    double getMajorTickUnit() const { return major_; }
    // Minor ticks drawn between two majors. Zero draws majors only.
    void setMinorTickCount(int value) { minor_ = value; }
    int getMinorTickCount() const { return minor_; }

    // Turning this on snaps the current value. The snap grid is the minor
    // spacing, or the major unit when the minor count is zero.
    void setSnapToTicks(bool value);
    bool isSnapToTicks() const { return snap_; }

    void setBlockIncrement(double value);
    double getBlockIncrement() const { return block_; }

    // Replaces the default major-tick text. An empty function restores it.
    void setLabelFormatter(std::function<std::string(double)> formatter) { formatter_ = std::move(formatter); }

    // Snaps when snapToTicks is on, then clamps. max <= min does nothing.
    void adjustValue(double value);
    void increment();
    void decrement();

    // Runs after value or valueChanging changes.
    void setOnValueChanged(std::function<void()> handler) { onChanged_ = std::move(handler); }

protected:
    void styleDidApply() override;
    void render(UiRenderer& renderer, float opacity) override;
    void renderContent(UiRenderer& renderer, float opacity) override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;
    void handleMousePressed(const MouseEvent& event) override;
    void handleMouseDragged(const MouseEvent& event) override;
    void handleMouseReleased(const MouseEvent& event) override;
    void handleKey(KeyEvent& event) override;
    void sceneChanged(Scene* previous) override;

private:
    struct Track;

    void clampValue();
    void changed();
    void syncPseudos();
    void finishThumbDrag();
    double snap(double value) const;
    double fraction() const;
    double tickExtra() const;
    Track track() const;
    double valueForPosition(double position) const;
    bool hitsThumb(double x, double y, const Track& metrics) const;
    bool hitsTrack(double x, double y, const Track& metrics) const;
    std::string labelFor(double value) const;

    double min_ = 0;
    double max_ = 100;
    double value_ = 0;
    double block_ = 10;
    double major_ = 25;
    int minor_ = 3;
    Orientation orientation_ = Orientation::Horizontal;
    bool valueChanging_ = false;
    bool showLabels_ = false;
    bool showMarks_ = false;
    bool snap_ = false;
    bool draggingThumb_ = false;
    bool draggingTrack_ = false;
    double pressAlong_ = 0;
    double pressFraction_ = 0;
    std::function<std::string(double)> formatter_;
    std::function<void()> onChanged_;
};

}  // namespace jadefx

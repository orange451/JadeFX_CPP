#include "jadefx/jadefx.hpp"
#include "jadefx/scene/Controls/Slider.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

void ExpectNear(double actual, double wanted, const char* message) {
    if (std::fabs(actual - wanted) > 0.001) {
        std::fprintf(stderr, "FAIL %s (got %.4f, wanted %.4f)\n", message, actual, wanted);
        ++gFailures;
    }
}

void Click(jadefx::Scene& scene, double x, double y) {
    scene.noteButton(0, true, x, y);
    scene.noteButton(0, false, x, y);
}

void Key(jadefx::Scene& scene, int key, bool pressed) { scene.noteKey(key, pressed, false, 0); }

// The thumb is 16 points. Position 0 is its center at min.
double Travel(const jadefx::Slider& slider) {
    const bool horizontal = slider.getOrientation() != jadefx::Orientation::Vertical;
    return (horizontal ? slider.getWidth() : slider.getHeight()) - 16.0;
}

double ThumbAlong(const jadefx::Slider& slider) {
    const bool horizontal = slider.getOrientation() != jadefx::Orientation::Vertical;
    const double origin = horizontal ? slider.getAbsoluteX() : slider.getAbsoluteY();
    const double span = slider.getMax() - slider.getMin();
    const double fraction = span > 0 ? (slider.getValue() - slider.getMin()) / span : 0;
    const double start = origin + 8.0;
    if (horizontal) {
        return start + fraction * Travel(slider);
    }
    return start + (1.0 - fraction) * Travel(slider);
}

std::shared_ptr<jadefx::Scene> Show(const std::shared_ptr<jadefx::Slider>& slider, double width = 400,
                                    double height = 160) {
    auto scene = jadefx::make<jadefx::Scene>(slider, width, height);
    scene->layout(width, height, 0);
    return scene;
}

void TestRange() {
    auto slider = jadefx::make<jadefx::Slider>();
    Expect(std::strcmp(slider->getElementType(), "slider") == 0, "element type is slider");
    ExpectNear(slider->getMin(), 0, "min defaults to 0");
    ExpectNear(slider->getMax(), 100, "max defaults to 100");
    ExpectNear(slider->getValue(), 0, "value defaults to 0");
    ExpectNear(slider->getBlockIncrement(), 10, "block increment defaults to 10");
    ExpectNear(slider->getMajorTickUnit(), 25, "major tick unit defaults to 25");
    Expect(slider->getMinorTickCount() == 3, "three minor ticks sit between majors");
    Expect(!slider->isSnapToTicks(), "snap is off");
    Expect(!slider->isShowTickMarks(), "tick marks are hidden");
    Expect(!slider->isShowTickLabels(), "tick labels are hidden");
    Expect(slider->getOrientation() == jadefx::Orientation::Horizontal, "the slider starts horizontal");
    Expect(slider->pseudoState("horizontal"), "the horizontal pseudo is set");
    Expect(!slider->pseudoState("vertical"), "the vertical pseudo is clear");

    auto swapped = jadefx::make<jadefx::Slider>(80, 50, 10);
    ExpectNear(swapped->getMin(), 80, "a min above max raises max");
    ExpectNear(swapped->getMax(), 80, "max follows a higher min");
    ExpectNear(swapped->getValue(), 80, "the value is clamped into the raised range");

    slider->setValue(140);
    ExpectNear(slider->getValue(), 100, "setValue clamps to max");
    slider->setValue(-4);
    ExpectNear(slider->getValue(), 0, "setValue clamps to min");
    slider->setMax(30);
    ExpectNear(slider->getMax(), 30, "max can move");
    slider->setValue(30);
    slider->setMin(40);
    ExpectNear(slider->getMin(), 40, "a min above max raises max from setMin");
    ExpectNear(slider->getMax(), 40, "setMin keeps max at least min");
    ExpectNear(slider->getValue(), 40, "the value follows the raised min");
    slider->setMajorTickUnit(0);
    ExpectNear(slider->getMajorTickUnit(), 25, "a non-positive major unit is ignored");
    slider->setMajorTickUnit(-2);
    ExpectNear(slider->getMajorTickUnit(), 25, "a negative major unit is ignored");
}

void TestCallbacksAndSnap() {
    auto slider = jadefx::make<jadefx::Slider>(0, 100, 0);
    int changes = 0;
    slider->setOnValueChanged([&] { ++changes; });
    slider->setValue(10);
    Expect(changes == 1, "setValue notifies");
    ExpectNear(slider->getValue(), 10, "setValue keeps a value that is not on a tick");
    slider->setValue(10);
    Expect(changes == 1, "the same value does not notify again");
    slider->setValueChanging(true);
    Expect(slider->isValueChanging(), "valueChanging can be set");
    Expect(changes == 2, "valueChanging notifies");
    slider->setValueChanging(true);
    Expect(changes == 2, "the same valueChanging does not notify again");
    slider->setValueChanging(false);

    slider->setSnapToTicks(true);
    ExpectNear(slider->getValue(), 12.5, "turning snap on moves to the nearest tick");
    slider->setValue(10);
    ExpectNear(slider->getValue(), 10, "setValue does not snap");
    slider->adjustValue(10);
    ExpectNear(slider->getValue(), 12.5, "adjustValue snaps");
    slider->adjustValue(1000);
    ExpectNear(slider->getValue(), 100, "adjustValue clamps to max");
    slider->setSnapToTicks(false);
    slider->setValue(95);
    slider->increment();
    ExpectNear(slider->getValue(), 100, "increment stops at max");
    slider->setValue(4);
    slider->decrement();
    ExpectNear(slider->getValue(), 0, "decrement stops at min");
    slider->setBlockIncrement(1);
    slider->setSnapToTicks(true);
    slider->setValue(0);
    slider->adjustValue(0);
    ExpectNear(slider->getValue(), 0, "zero is already on a tick");
}

void TestPreferredSize() {
    auto slider = jadefx::make<jadefx::Slider>();
    auto scene = Show(slider);
    ExpectNear(slider->getWidth(), 140, "a horizontal slider prefers 140");
    ExpectNear(slider->getHeight(), 16, "the thumb is 16 points tall");
    const double plain = slider->getHeight();
    slider->setShowTickMarks(true);
    scene->layout(400, 160, 0);
    Expect(slider->getHeight() > plain + 8, "tick marks add a row under the track");
    const double marks = slider->getHeight();
    slider->setShowTickLabels(true);
    scene->layout(400, 160, 0);
    Expect(slider->getHeight() > marks, "tick labels add another row");

    slider->setOrientation(jadefx::Orientation::Vertical);
    scene->layout(400, 200, 0);
    Expect(slider->getOrientation() == jadefx::Orientation::Vertical, "setOrientation switches the axis");
    Expect(slider->pseudoState("vertical"), "the vertical pseudo follows the orientation");
    Expect(!slider->pseudoState("horizontal"), "the horizontal pseudo clears");
    ExpectNear(slider->getHeight(), 140, "a vertical slider prefers 140 along the track");
    Expect(slider->getWidth() > 16, "labels sit to the right of a vertical track");

    slider->setStyle("orientation: horizontal;");
    scene->layout(400, 160, 0);
    Expect(slider->getOrientation() == jadefx::Orientation::Horizontal, "orientation in CSS is applied");
    Expect(slider->pseudoState("horizontal"), "the horizontal pseudo follows the stylesheet");
}

void TestTrackAndThumb() {
    auto slider = jadefx::make<jadefx::Slider>(0, 100, 50);
    slider->setPrefSize(200, 16);
    auto scene = Show(slider);
    const double midY = slider->getAbsoluteY() + slider->getHeight() * 0.5;
    Expect(slider->isValueChanging() == false, "a slider is not changing at rest");

    scene->noteButton(0, true, ThumbAlong(*slider), midY);
    Expect(slider->isValueChanging(), "pressing the thumb marks the value as changing");
    ExpectNear(slider->getValue(), 50, "pressing the thumb does not jump");
    scene->noteMove(slider->getAbsoluteX() + 146, midY);
    ExpectNear(slider->getValue(), 75, "dragging the thumb follows the pointer");
    Expect(slider->isValueChanging(), "the value stays changing during the drag");
    scene->noteButton(0, false, slider->getAbsoluteX() + 146, midY);
    Expect(!slider->isValueChanging(), "releasing the thumb clears valueChanging");
    ExpectNear(slider->getValue(), 75, "a drag without snap keeps the pointer value");

    Click(*scene, slider->getAbsoluteX() + slider->getWidth() - 1, midY);
    ExpectNear(slider->getValue(), 100, "a click at the end of the track jumps to max");
    Expect(!slider->isValueChanging(), "a track click does not mark the value as changing");

    slider->setValue(0);
    slider->setSnapToTicks(true);
    const double towardTen = slider->getAbsoluteX() + 8.0 + 0.1 * Travel(*slider);
    Click(*scene, towardTen, midY);
    ExpectNear(slider->getValue(), 12.5, "a track click snaps to the nearest tick");

    slider->setValue(10);
    scene->noteButton(0, true, ThumbAlong(*slider), midY);
    ExpectNear(slider->getValue(), 10, "the thumb drag does not snap while it is held");
    scene->noteButton(0, false, ThumbAlong(*slider), midY);
    ExpectNear(slider->getValue(), 12.5, "releasing the thumb snaps");
    Expect(!slider->isValueChanging(), "snap on release clears valueChanging");
}

void TestKeys() {
    auto slider = jadefx::make<jadefx::Slider>(0, 100, 40);
    auto scene = Show(slider);
    scene->requestFocus(slider.get());
    Key(*scene, jadefx::Key::Right, true);
    ExpectNear(slider->getValue(), 50, "Right adds the block increment");
    Key(*scene, jadefx::Key::Left, true);
    ExpectNear(slider->getValue(), 40, "Left subtracts the block increment");
    Key(*scene, jadefx::Key::Up, true);
    ExpectNear(slider->getValue(), 40, "Up does not move a horizontal slider");
    Expect(!scene->noteKey(jadefx::Key::Up, true, false, 0), "Up is not consumed on a horizontal slider");
    Key(*scene, jadefx::Key::Home, true);
    ExpectNear(slider->getValue(), 40, "Home waits for the key release");
    Key(*scene, jadefx::Key::Home, false);
    ExpectNear(slider->getValue(), 0, "Home release jumps to min");
    Key(*scene, jadefx::Key::End, false);
    ExpectNear(slider->getValue(), 100, "End release jumps to max");

    slider->setOrientation(jadefx::Orientation::Vertical);
    slider->setValue(40);
    Key(*scene, jadefx::Key::Up, true);
    ExpectNear(slider->getValue(), 50, "Up moves a vertical slider toward max");
    Key(*scene, jadefx::Key::Down, true);
    ExpectNear(slider->getValue(), 40, "Down moves a vertical slider toward min");
    Expect(!scene->noteKey(jadefx::Key::Left, true, false, 0), "Left is not consumed on a vertical slider");

    slider->setSnapToTicks(true);
    slider->setBlockIncrement(1);
    slider->setValue(0);
    Key(*scene, jadefx::Key::Up, true);
    ExpectNear(slider->getValue(), 6.25, "a block smaller than the tick spacing steps one tick");

    slider->setDisable(true);
    const double frozen = slider->getValue();
    Key(*scene, jadefx::Key::Up, true);
    Click(*scene, slider->getAbsoluteX() + 8, slider->getAbsoluteY() + 4);
    ExpectNear(slider->getValue(), frozen, "a disabled slider ignores keys and clicks");
}

void TestVerticalTrack() {
    auto slider = jadefx::make<jadefx::Slider>(0, 100, 0);
    slider->setPrefSize(16, 200);
    slider->setOrientation(jadefx::Orientation::Vertical);
    auto scene = Show(slider, 200, 300);
    const double midX = slider->getAbsoluteX() + slider->getWidth() * 0.5;
    Click(*scene, midX, slider->getAbsoluteY() + 2);
    ExpectNear(slider->getValue(), 100, "the top of a vertical track is max");
    slider->setValue(100);
    Click(*scene, midX, slider->getAbsoluteY() + slider->getHeight() - 2);
    ExpectNear(slider->getValue(), 0, "the bottom of a vertical track is min");
}

}  // namespace

int RunSliderTests() {
    TestRange();
    TestCallbacksAndSnap();
    TestPreferredSize();
    TestTrackAndThumb();
    TestKeys();
    TestVerticalTrack();
    return gFailures;
}

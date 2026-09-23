#include "jadefx/jadefx.hpp"
#include "jadefx/scene/Controls/RadioButton.hpp"
#include "jadefx/scene/Controls/ToggleGroup.hpp"

#include <cstdio>

namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

void Click(jadefx::Scene& scene, const jadefx::Node& node) {
    const double x = node.getAbsoluteX() + node.getWidth() * 0.5;
    const double y = node.getAbsoluteY() + node.getHeight() * 0.5;
    scene.noteButton(0, true, x, y);
    scene.noteButton(0, false, x, y);
}

void TestToggleGroup() {
    jadefx::ToggleGroup group;
    auto first = jadefx::make<jadefx::RadioButton>("First");
    auto second = jadefx::make<jadefx::RadioButton>("Second");
    first->setPrefSize(160, 32);
    second->setPrefSize(160, 32);
    first->setToggleGroup(&group);
    second->setToggleGroup(&group);
    int firstClicks = 0;
    int secondClicks = 0;
    first->setOnAction([&](jadefx::ActionEvent&) { ++firstClicks; });
    second->setOnAction([&](jadefx::ActionEvent&) { ++secondClicks; });

    auto box = jadefx::make<jadefx::VBox>();
    box->getChildren().add(first);
    box->getChildren().add(second);
    auto scene = jadefx::make<jadefx::Scene>(box, 200, 120);
    scene->layout(200, 120, 0);

    first->setSelected(true);
    Expect(first->isSelected(), "the first radio in the group is selected");
    Expect(!second->isSelected(), "the other radio in the group starts clear");
    Expect(group.getSelectedToggle() == first.get(), "the group tracks the selected radio");
    const int firstBefore = firstClicks;
    const int secondBefore = secondClicks;

    Click(*scene, *second);
    Expect(!first->isSelected(), "clicking the second radio clears the first");
    Expect(second->isSelected(), "clicking the second radio selects it");
    Expect(group.getSelectedToggle() == second.get(), "the group follows the clicked radio");
    Expect(firstClicks == firstBefore, "clicking the second radio does not fire the first");
    Expect(secondClicks == secondBefore + 1, "clicking the second radio fires only its action");

    Click(*scene, *second);
    Expect(second->isSelected(), "clicking the selected radio leaves it selected");
    Expect(!first->isSelected(), "clicking the selected radio does not clear the group");
    Expect(secondClicks == secondBefore + 1, "clicking the selected radio does not fire");
    Expect(firstClicks == firstBefore, "clicking the selected radio does not fire the other radio");
}

void TestUngroupedStaysSelected() {
    int clicks = 0;
    auto radio = jadefx::make<jadefx::RadioButton>("Solo");
    radio->setOnAction([&](jadefx::ActionEvent&) { ++clicks; });
    auto scene = jadefx::make<jadefx::Scene>(radio, 200, 80);
    scene->layout(200, 80, 0);

    Click(*scene, *radio);
    Expect(radio->isSelected(), "a click selects a radio with no group");
    Expect(radio->getToggleGroup() == nullptr, "a radio with no group stays ungrouped");
    Expect(clicks == 1, "the first click fires the radio");

    Click(*scene, *radio);
    Expect(radio->isSelected(), "a second click keeps a radio with no group selected");
    Expect(clicks == 1, "a second click on a radio with no group does not fire");
}

void TestDisabledIgnoresClick() {
    int clicks = 0;
    auto radio = jadefx::make<jadefx::RadioButton>("Off");
    radio->setOnAction([&](jadefx::ActionEvent&) { ++clicks; });
    auto scene = jadefx::make<jadefx::Scene>(radio, 200, 80);
    scene->layout(200, 80, 0);
    radio->setDisable(true);
    scene->layout(200, 80, 0);

    Click(*scene, *radio);
    Expect(!radio->isSelected(), "a disabled radio ignores the click");
    Expect(clicks == 0, "a disabled radio does not fire from a click");
    radio->fire();
    Expect(!radio->isSelected(), "fire() on a disabled radio does not select it");
    Expect(clicks == 0, "fire() on a disabled radio does not run the action");
}

}  // namespace

int RunRadioButtonTests() {
    TestToggleGroup();
    TestUngroupedStaysSelected();
    TestDisabledIgnoresClick();
    return gFailures;
}

#include "jadefx/jadefx.hpp"
#include "jadefx/scene/controls/CheckBox.hpp"

#include <cstdio>
#include <string>

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

bool Near(float a, float b) {
    const float delta = a - b;
    return delta < 0.02f && delta > -0.02f;
}

bool SameColor(const jadefx::Color& have, const jadefx::Color& want) {
    return Near(have.r, want.r) && Near(have.g, want.g) && Near(have.b, want.b);
}

void TestDefaults() {
    auto box = jadefx::make<jadefx::CheckBox>("Yes");
    Expect(std::string(box->getElementType()) == "checkbox", "the element type is checkbox");
    Expect(!box->isSelected(), "a check box starts unchecked");
    Expect(!box->isIndeterminate(), "a check box starts determinate");
    Expect(!box->isAllowIndeterminate(), "three-state cycling starts off");
    Expect(box->pseudoState("determinate") && !box->pseudoState("indeterminate"), "the determinate pseudo starts on");
    Expect(box->getAlignment() == jadefx::Pos::CenterLeft, "the label sits to the right of the box");
    Expect(box->getPadding().left == 30, "left padding clears the box");
    jadefx::Node* node = box.get();
    Expect(!node->isSelected(), "the node selected flag starts clear");
}

void TestClickToggles() {
    int clicks = 0;
    auto box = jadefx::make<jadefx::CheckBox>("Mail");
    box->setOnAction([&](jadefx::ActionEvent&) { ++clicks; });
    auto scene = jadefx::make<jadefx::Scene>(box, 200, 80);
    scene->layout(200, 80, 0);

    Click(*scene, *box);
    Expect(box->isSelected(), "a click checks the box");
    Expect(!box->isIndeterminate(), "a click leaves a two-state box determinate");
    Expect(clicks == 1, "checking fires the action");
    jadefx::Node* node = box.get();
    Expect(node->isSelected(), "checking sets the node selected flag");

    Click(*scene, *box);
    Expect(!box->isSelected(), "a second click clears the box");
    Expect(clicks == 2, "clearing fires the action");
    Expect(!node->isSelected(), "clearing clears the node selected flag");

    const double x = box->getAbsoluteX() + box->getWidth() * 0.5;
    const double y = box->getAbsoluteY() + box->getHeight() * 0.5;
    scene->noteButton(0, true, x, y);
    scene->noteButton(0, false, 2, 2);
    Expect(!box->isSelected(), "releasing outside the box does not check it");
    Expect(clicks == 2, "releasing outside the box does not fire");
}

void TestTwoStateClearsIndeterminate() {
    int clicks = 0;
    auto box = jadefx::make<jadefx::CheckBox>("Pages");
    box->setIndeterminate(true);
    box->setOnAction([&](jadefx::ActionEvent&) { ++clicks; });
    box->fire();
    Expect(box->isSelected(), "firing an indeterminate two-state box checks it");
    Expect(!box->isIndeterminate(), "firing a two-state box clears indeterminate");
    Expect(clicks == 1, "clearing indeterminate still fires the action");

    box->setSelected(true);
    box->setIndeterminate(true);
    box->fire();
    Expect(!box->isSelected(), "firing a checked indeterminate two-state box clears it");
    Expect(!box->isIndeterminate(), "that fire also clears indeterminate");
    Expect(clicks == 2, "the two-state fire runs the action");
}

void TestTriStateCycle() {
    int clicks = 0;
    auto box = jadefx::make<jadefx::CheckBox>("All");
    box->setAllowIndeterminate(true);
    box->setOnAction([&](jadefx::ActionEvent&) { ++clicks; });

    box->fire();
    Expect(!box->isSelected() && box->isIndeterminate(), "the first fire makes the box indeterminate");
    Expect(clicks == 1, "entering indeterminate fires the action");

    box->fire();
    Expect(box->isSelected() && !box->isIndeterminate(), "the second fire checks the box");
    Expect(clicks == 2, "checking from indeterminate fires the action");

    box->fire();
    Expect(!box->isSelected() && !box->isIndeterminate(), "the third fire clears the box");
    Expect(clicks == 3, "clearing from checked fires the action");

    box->fire();
    Expect(!box->isSelected() && box->isIndeterminate(), "the cycle returns to indeterminate");

    box->setSelected(true);
    box->setIndeterminate(true);
    box->fire();
    Expect(box->isSelected() && !box->isIndeterminate(), "firing while both flags are set checks the box");
}

void TestPropertiesStayIndependent() {
    auto box = jadefx::make<jadefx::CheckBox>("Mix");
    box->setSelected(true);
    box->setIndeterminate(true);
    Expect(box->isSelected() && box->isIndeterminate(), "selected and indeterminate can both be set");
    Expect(box->pseudoState("indeterminate") && !box->pseudoState("determinate"), "indeterminate replaces determinate");
    jadefx::Node* node = box.get();
    Expect(node->isSelected(), "setSelected(true) sets the node flag");

    box->setIndeterminate(false);
    Expect(box->isSelected(), "clearing indeterminate leaves the check");
    Expect(box->pseudoState("determinate") && !box->pseudoState("indeterminate"), "clearing indeterminate restores determinate");

    box->setIndeterminate(false);
    Expect(box->pseudoState("determinate"), "setting indeterminate false again keeps determinate");
}

void TestDisabledIgnoresInput() {
    int clicks = 0;
    auto box = jadefx::make<jadefx::CheckBox>("Off");
    box->setOnAction([&](jadefx::ActionEvent&) { ++clicks; });
    auto scene = jadefx::make<jadefx::Scene>(box, 200, 80);
    scene->layout(200, 80, 0);
    box->setDisable(true);
    scene->layout(200, 80, 0);

    Click(*scene, *box);
    Expect(!box->isSelected(), "a disabled check box ignores the click");
    Expect(clicks == 0, "a disabled check box does not fire from a click");
    box->setAllowIndeterminate(true);
    box->fire();
    Expect(!box->isSelected() && !box->isIndeterminate(), "fire() on a disabled check box does not change state");
    Expect(clicks == 0, "fire() on a disabled check box does not run the action");
}

void TestKeysCycle() {
    int clicks = 0;
    auto box = jadefx::make<jadefx::CheckBox>("Keys");
    box->setOnAction([&](jadefx::ActionEvent&) { ++clicks; });
    auto scene = jadefx::make<jadefx::Scene>(box, 200, 80);
    scene->layout(200, 80, 0);
    Click(*scene, *box);
    Expect(box->isFocused(), "clicking a check box focuses it");
    Expect(box->isSelected() && clicks == 1, "the click checks the box");

    scene->noteKey(jadefx::Key::Space, true, false, 0);
    Expect(!box->isSelected(), "Space clears the focused check box");
    Expect(clicks == 2, "Space fires the action");
    scene->noteKey(jadefx::Key::Enter, true, false, 0);
    Expect(box->isSelected(), "Enter checks the focused check box");
    Expect(clicks == 3, "Enter fires the action");
    scene->noteKey(jadefx::Key::Space, true, true, 0);
    Expect(box->isSelected() && clicks == 3, "a repeated Space does not fire again");
}

void TestStateStyles() {
    auto box = jadefx::make<jadefx::CheckBox>("Styled");
    auto scene = jadefx::make<jadefx::Scene>(box, 200, 80);
    scene->setStylesheet(
        "checkbox { background-color: #cccccc; }"
        "checkbox:determinate { background-color: #112233; }"
        "checkbox:indeterminate { background-color: #aa5500; }"
        "checkbox:selected { background-color: #00aa00; }");
    scene->layout(200, 80, 0);
    Expect(SameColor(box->computedStyle().background.color, jadefx::Color::parse("#112233")),
           ":determinate matches a check box that is not indeterminate");

    box->setIndeterminate(true);
    scene->layout(200, 80, 0);
    Expect(SameColor(box->computedStyle().background.color, jadefx::Color::parse("#aa5500")),
           ":indeterminate matches an indeterminate check box");

    box->setIndeterminate(false);
    box->setSelected(true);
    scene->layout(200, 80, 0);
    Expect(SameColor(box->computedStyle().background.color, jadefx::Color::parse("#00aa00")),
           ":selected matches a checked check box");

    box->setSelected(false);
    scene->layout(200, 80, 0);
    Expect(SameColor(box->computedStyle().background.color, jadefx::Color::parse("#112233")),
           ":selected does not match a cleared check box");
}

}  // namespace

int RunCheckBoxTests() {
    TestDefaults();
    TestClickToggles();
    TestTwoStateClearsIndeterminate();
    TestTriStateCycle();
    TestPropertiesStayIndependent();
    TestDisabledIgnoresInput();
    TestKeysCycle();
    TestStateStyles();
    return gFailures;
}

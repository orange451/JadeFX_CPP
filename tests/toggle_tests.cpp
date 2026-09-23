#include "jadefx/jadefx.hpp"
#include "jadefx/scene/Controls/ToggleButton.hpp"
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

void Click(jadefx::Scene& scene, double x, double y) {
    scene.noteButton(0, true, x, y);
    scene.noteButton(0, false, x, y);
}

void ClickNode(jadefx::Scene& scene, jadefx::Node& node) {
    Click(scene, node.getAbsoluteX() + node.getWidth() * 0.5, node.getAbsoluteY() + node.getHeight() * 0.5);
}

bool Near(float a, float b) {
    const float delta = a - b;
    return delta < 0.02f && delta > -0.02f;
}

void TestClickTogglesAndFires() {
    int clicks = 0;
    auto button = jadefx::make<jadefx::ToggleButton>("Go");
    button->setOnAction([&](jadefx::ActionEvent&) { ++clicks; });
    auto scene = jadefx::make<jadefx::Scene>(button, 200, 80);
    scene->layout(200, 80, 0);
    ClickNode(*scene, *button);
    Expect(button->isSelected(), "a click selects the toggle button");
    Expect(clicks == 1, "selecting fires the action");
    ClickNode(*scene, *button);
    Expect(!button->isSelected(), "a second click clears the toggle button");
    Expect(clicks == 2, "clearing fires the action");
}

void TestSelectedFlag() {
    auto button = jadefx::make<jadefx::ToggleButton>("On");
    button->setSelected(true);
    Expect(button->isSelected(), "setSelected(true) selects the toggle");
    jadefx::Node* node = button.get();
    Expect(node->isSelected(), "setSelected(true) sets the Node flag");

    auto scene = jadefx::make<jadefx::Scene>(button, 200, 80);
    scene->setStylesheet(
        "togglebutton { background-color: #ddeeff; } togglebutton:selected { background-color: #112233; }");
    scene->layout(200, 80, 0);
    const jadefx::Color want = jadefx::Color::parse("#112233");
    const jadefx::Color& have = button->computedStyle().background.color;
    Expect(Near(have.r, want.r) && Near(have.g, want.g) && Near(have.b, want.b),
           ":selected matches a selected toggle button");

    button->setSelected(false);
    scene->layout(200, 80, 0);
    const jadefx::Color idle = jadefx::Color::parse("#ddeeff");
    const jadefx::Color& cleared = button->computedStyle().background.color;
    Expect(!node->isSelected(), "setSelected(false) clears the Node flag");
    Expect(Near(cleared.r, idle.r) && Near(cleared.g, idle.g) && Near(cleared.b, idle.b),
           ":selected does not match a cleared toggle button");
}

void TestGroupExclusive() {
    jadefx::ToggleGroup group;
    auto first = jadefx::make<jadefx::ToggleButton>("One");
    auto second = jadefx::make<jadefx::ToggleButton>("Two");
    first->setToggleGroup(&group);
    second->setToggleGroup(&group);
    Expect(group.getToggles().size() == 2, "the group lists both toggles");
    first->setToggleGroup(&group);
    Expect(group.getToggles().size() == 2, "setting the same group again is a no-op");

    first->setSelected(true);
    Expect(group.getSelectedToggle() == first.get(), "the first toggle becomes the selection");
    second->setSelected(true);
    Expect(second->isSelected() && !first->isSelected(), "selecting the second clears the first");
    Expect(group.getSelectedToggle() == second.get(), "the group follows the second toggle");
    jadefx::Node* firstNode = first.get();
    jadefx::Node* secondNode = second.get();
    Expect(!firstNode->isSelected() && secondNode->isSelected(), "the Node flag follows the group selection");
}

void TestClickClearsGroup() {
    jadefx::ToggleGroup group;
    auto first = jadefx::make<jadefx::ToggleButton>("One");
    auto second = jadefx::make<jadefx::ToggleButton>("Two");
    first->setPrefSize(80, 32);
    second->setPrefSize(80, 32);
    first->setToggleGroup(&group);
    second->setToggleGroup(&group);
    int clicks = 0;
    second->setOnAction([&](jadefx::ActionEvent&) { ++clicks; });
    auto row = jadefx::make<jadefx::HBox>();
    row->getChildren().add(first);
    row->getChildren().add(second);
    auto scene = jadefx::make<jadefx::Scene>(row, 240, 80);
    scene->layout(240, 80, 0);

    ClickNode(*scene, *second);
    Expect(second->isSelected() && !first->isSelected(), "a click selects that toggle in the group");
    Expect(group.getSelectedToggle() == second.get(), "the group records the clicked toggle");
    Expect(clicks == 1, "the grouped click fires the action");

    ClickNode(*scene, *second);
    Expect(!second->isSelected() && !first->isSelected(), "clicking the selected toggle in a group turns it off");
    Expect(group.getSelectedToggle() == nullptr, "turning the selected toggle off clears the group");
    Expect(clicks == 2, "turning a grouped toggle off fires the action");
}

void TestRemoveSelected() {
    jadefx::ToggleGroup group;
    auto button = jadefx::make<jadefx::ToggleButton>("A");
    auto other = jadefx::make<jadefx::ToggleButton>("B");
    button->setToggleGroup(&group);
    other->setToggleGroup(&group);
    button->setSelected(true);
    group.removeToggle(button.get());
    Expect(group.getSelectedToggle() == nullptr, "removing the selected toggle clears the group");
    Expect(group.getToggles().size() == 1 && group.getToggles()[0] == other.get(),
           "removeToggle drops only that toggle");
    Expect(other->getToggleGroup() == &group, "the other toggle stays in the group");
    Expect(!other->isSelected(), "removing the selection does not select another toggle");
}

void TestDestructorRemoves() {
    jadefx::ToggleGroup group;
    auto keep = jadefx::make<jadefx::ToggleButton>("Keep");
    keep->setToggleGroup(&group);
    keep->setSelected(true);
    {
        auto drop = jadefx::make<jadefx::ToggleButton>("Drop");
        drop->setToggleGroup(&group);
        Expect(group.getToggles().size() == 2, "both toggles join the group");
    }
    Expect(group.getToggles().size() == 1 && group.getToggles()[0] == keep.get(),
           "the destructor removes the toggle");
    Expect(group.getSelectedToggle() == keep.get(), "dropping another toggle keeps the selection");

    keep.reset();
    Expect(group.getSelectedToggle() == nullptr, "destroying the selected toggle clears the group");
    Expect(group.getToggles().empty(), "the group holds no dangling pointer");
}

void TestDisabledClick() {
    int clicks = 0;
    auto button = jadefx::make<jadefx::ToggleButton>("Off");
    button->setOnAction([&](jadefx::ActionEvent&) { ++clicks; });
    button->setDisable(true);
    auto scene = jadefx::make<jadefx::Scene>(button, 200, 80);
    scene->layout(200, 80, 0);
    ClickNode(*scene, *button);
    Expect(!button->isSelected(), "a disabled toggle does not toggle on click");
    Expect(clicks == 0, "a disabled toggle does not fire");
}

void TestJoinGroup() {
    jadefx::ToggleGroup group;
    auto resident = jadefx::make<jadefx::ToggleButton>("Resident");
    resident->setToggleGroup(&group);
    resident->setSelected(true);

    auto joining = jadefx::make<jadefx::ToggleButton>("Joining");
    joining->setSelected(true);
    joining->setToggleGroup(&group);
    Expect(!joining->isSelected(), "a selected button yields to the group's selection");
    Expect(group.getSelectedToggle() == resident.get() && resident->isSelected(),
           "the group's selection wins when a selected button joins");

    jadefx::ToggleGroup empty;
    auto taken = jadefx::make<jadefx::ToggleButton>("Taken");
    taken->setSelected(true);
    taken->setToggleGroup(&empty);
    Expect(taken->isSelected() && empty.getSelectedToggle() == taken.get(),
           "a selected button becomes the selection of an empty group");

    taken->setToggleGroup(nullptr);
    Expect(taken->getToggleGroup() == nullptr, "setToggleGroup(nullptr) leaves the group");
    Expect(empty.getToggles().empty() && empty.getSelectedToggle() == nullptr,
           "leaving clears the previous group");
    Expect(taken->isSelected(), "leaving the group keeps the button selected");
}

}  // namespace

int RunToggleTests() {
    TestClickTogglesAndFires();
    TestSelectedFlag();
    TestGroupExclusive();
    TestClickClearsGroup();
    TestRemoveSelected();
    TestDestructorRemoves();
    TestDisabledClick();
    TestJoinGroup();
    return gFailures;
}

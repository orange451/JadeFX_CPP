#include "jadefx/jadefx.hpp"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// The shared pieces the collection controls stand on: Labeled graphics, list
// listeners, click counts, focus listeners, :nth-child, and applyCss.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

bool Near(double a, double b) { return std::fabs(a - b) <= 0.5; }

std::shared_ptr<jadefx::Pane> Block(double width, double height) {
    auto block = jadefx::make<jadefx::Pane>();
    block->setPrefSize(width, height);
    return block;
}

void TestLabeledGraphic() {
    auto label = jadefx::make<jadefx::Label>("Text");
    auto icon = Block(16, 16);
    label->setGraphic(icon);
    auto root = jadefx::make<jadefx::Pane>();
    root->getChildren().add(label);
    auto scene = jadefx::make<jadefx::Scene>(root, 300, 200);
    scene->layout(300, 200, 0);
    const double textWidth = jadefx::Font("Open Sans", 16).measureWidth("Text");
    Expect(Near(label->getWidth(), 16 + 4 + textWidth), "the graphic, the gap, and the text set the width");
    Expect(Near(icon->getX(), 0) && icon->getParent() == label.get(), "the graphic sits on the left");

    label->setContentDisplay(jadefx::ContentDisplay::Top);
    scene->layout(300, 200, 0);
    Expect(Near(label->getWidth(), std::max(16.0, textWidth)) && icon->getY() < 1,
           "Top stacks the graphic over the text");

    label->setContentDisplay(jadefx::ContentDisplay::GraphicOnly);
    scene->layout(300, 200, 0);
    Expect(Near(label->getWidth(), 16) && label->displayedText().empty(), "GraphicOnly hides the text");

    label->setContentDisplay(jadefx::ContentDisplay::Left);
    label->setPrefWidth(40);
    scene->layout(300, 200, 0);
    const std::string shown = label->displayedText();
    Expect(!shown.empty() && shown != "Text", "text that does not fit beside the graphic ends in an ellipsis");

    auto other = jadefx::make<jadefx::Pane>();
    other->getChildren().add(icon);
    Expect(label->getGraphic() == nullptr, "a graphic taken by another parent leaves the label");
}

void TestListListeners() {
    jadefx::ObservableList<int> list;
    std::vector<std::string> log;
    auto id = list.addListener([&](const jadefx::ObservableList<int>::Change& change) {
        const char* kind = change.kind == jadefx::ObservableList<int>::Change::Kind::Added     ? "add"
                           : change.kind == jadefx::ObservableList<int>::Change::Kind::Removed ? "remove"
                                                                                               : "set";
        log.push_back(std::string(kind) + " " + std::to_string(change.index) + " " + std::to_string(change.item));
    });
    int second = 0;
    list.addListener([&](const jadefx::ObservableList<int>::Change&) { ++second; });
    list.add(1);
    list.insert(0, 2);
    list.set(1, 5);
    list.removeAt(0);
    Expect(log.size() == 4 && log[0] == "add 0 1" && log[1] == "add 0 2" && log[2] == "set 1 5" && log[3] == "remove 0 2",
           "listeners hear each change with its index and item");
    Expect(second == 4, "every listener hears every change");
    list.removeListener(id);
    list.setAll({7, 8});
    Expect(log.size() == 4 && second == 7 && list.size() == 2, "a removed listener hears nothing more");
}

void TestClickCountAndFocus() {
    auto first = jadefx::make<jadefx::Button>("First");
    auto second = jadefx::make<jadefx::Button>("Second");
    auto row = jadefx::make<jadefx::HBox>();
    row->getChildren().add(first);
    row->getChildren().add(second);
    auto scene = jadefx::make<jadefx::Scene>(row, 300, 100);
    scene->layout(300, 100, 0);
    std::vector<int> counts;
    first->setOnMousePressed([&](const jadefx::MouseEvent& event) { counts.push_back(event.clickCount); });
    std::vector<std::string> focus;
    first->setOnFocusChanged([&](bool on) { focus.push_back(on ? "first on" : "first off"); });
    second->setOnFocusChanged([&](bool on) { focus.push_back(on ? "second on" : "second off"); });

    const double x = first->getAbsoluteX() + 5;
    const double y = first->getAbsoluteY() + 5;
    for (int i = 0; i < 3; ++i) {
        scene->noteButton(0, true, x, y, 0);
        scene->noteButton(0, false, x, y, 0);
    }
    Expect(counts.size() == 3 && counts[0] == 1 && counts[1] == 2 && counts[2] == 3, "quick presses count up");
    scene->noteButton(0, true, x, y, 0);
    scene->noteMove(x + 30, y);
    scene->noteButton(0, false, x + 30, y, 0);
    scene->noteButton(0, true, x, y, 0);
    scene->noteButton(0, false, x, y, 0);
    Expect(counts.size() == 5 && counts[4] == 1, "a drag ends the run of clicks");

    second->requestFocus();
    Expect(focus.size() == 3 && focus[0] == "first on" && focus[1] == "first off" && focus[2] == "second on", "a press focuses the button, and focusing another takes it away");
    focus.clear();
    first->requestFocus();
    Expect(focus.size() == 2 && focus[0] == "second off" && focus[1] == "first on",
           "the node losing focus hears first, then the node taking it");
}

void TestNthChild() {
    auto column = jadefx::make<jadefx::VBox>();
    std::vector<std::shared_ptr<jadefx::Pane>> rows;
    for (int i = 0; i < 6; ++i) {
        rows.push_back(Block(10, 10));
        column->getChildren().add(rows.back());
    }
    column->setStylesheet(
        "pane:nth-child(2n+1) { background-color: #ff0000; }"
        "pane:nth-child(-n+2) { border-color: #00ff00; }"
        "pane:nth-child(5) { background-color: #0000ff; }");
    auto scene = jadefx::make<jadefx::Scene>(column, 100, 100);
    scene->layout(100, 100, 0);
    auto red = [&](int i) { return rows[static_cast<std::size_t>(i)]->computedStyle().background.color.r > 0.5f; };
    auto blue = [&](int i) { return rows[static_cast<std::size_t>(i)]->computedStyle().background.color.b > 0.5f; };
    auto green = [&](int i) { return rows[static_cast<std::size_t>(i)]->computedStyle().borderColor.g > 0.5f; };
    Expect(red(0) && !red(1) && red(2) && !red(3), "2n+1 matches the first, third, and so on");
    Expect(blue(4), "a plain number matches that child");
    Expect(green(0) && green(1) && !green(2), "-n+2 matches the first two");
}

void TestApplyCss() {
    auto root = jadefx::make<jadefx::Pane>();
    root->setStylesheet(".late { font-size: 30px; }");
    auto scene = jadefx::make<jadefx::Scene>(root, 100, 100);
    scene->layout(100, 100, 0);
    auto label = jadefx::make<jadefx::Label>("Late");
    label->getClassList().add("late");
    root->getChildren().add(label);
    label->applyCss();
    Expect(Near(label->computedStyle().fontSize, 30), "applyCss styles a node before the next layout");
}

void TestPickOnBounds() {
    // Two full-size overlays, the top one empty but for a small block.
    auto root = jadefx::make<jadefx::StackPane>();
    auto under = jadefx::make<jadefx::StackPane>();
    auto button = jadefx::make<jadefx::Button>("Under");
    under->getChildren().add(button);
    auto over = jadefx::make<jadefx::Pane>();
    over->setPrefSize(200, 200);
    auto block = Block(20, 20);
    over->getChildren().add(block);
    root->getChildren().add(under);
    root->getChildren().add(over);
    auto scene = jadefx::make<jadefx::Scene>(root, 200, 200);
    scene->layout(200, 200, 0);
    const double x = button->getAbsoluteX() + button->getWidth() / 2;
    const double y = button->getAbsoluteY() + button->getHeight() / 2;
    Expect(scene->pick(x, y) == over.get(), "an overlay's own box takes the pick by default");
    over->setPickOnBounds(false);
    Expect(scene->pick(x, y) == button.get(), "without pickOnBounds the pick passes through its empty box");
    Expect(scene->pick(5, 5) == block.get(), "and still finds its children");
}

}  // namespace

int RunCoreTests() {
    TestPickOnBounds();
    TestLabeledGraphic();
    TestListListeners();
    TestClickCountAndFocus();
    TestNthChild();
    TestApplyCss();
    if (gFailures == 0) {
        std::printf("core tests passed\n");
    }
    return gFailures;
}

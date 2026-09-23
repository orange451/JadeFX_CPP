#include "jadefx/jadefx.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

void ExpectNear(double actual, double wanted, const char* message, double tolerance = 0.75) {
    if (std::fabs(actual - wanted) > tolerance) {
        std::fprintf(stderr, "FAIL %s (got %.3f, wanted %.3f)\n", message, actual, wanted);
        ++gFailures;
    }
}

std::shared_ptr<jadefx::Scene> Show(const std::shared_ptr<jadefx::SplitPane>& split, double width, double height) {
    auto scene = jadefx::make<jadefx::Scene>(split, width, height);
    scene->layout(width, height, 0);
    return scene;
}

std::shared_ptr<jadefx::Pane> Box() { return jadefx::make<jadefx::Pane>(); }

std::vector<jadefx::Node*> Dividers(jadefx::Node& root) {
    return root.getElementsByClassName("split-pane-divider");
}

void TestEmptyAndSingle() {
    auto split = jadefx::make<jadefx::SplitPane>();
    Expect(std::string(split->getElementType()) == "split-pane", "the control type is split-pane");
    Expect(split->getOrientation() == jadefx::Orientation::Horizontal, "a split pane starts horizontal");
    Expect(split->pseudoState("horizontal") && !split->pseudoState("vertical"), "the horizontal pseudo starts on");
    Expect(split->getDividers().empty() && split->getDividerPositions().empty(), "no items means no dividers");
    split->getItems().add(split);
    Expect(split->getItems().empty(), "a split pane cannot contain itself");

    auto only = Box();
    jadefx::Node* raw = only.get();
    split->getItems().add(only);
    split->getItems().add(only);
    Expect(split->getItems().size() == 1, "the same item is not inserted twice");
    split->setPrefSize(300, 120);
    Show(split, 300, 120);
    Expect(split->getDividers().empty(), "one item has no divider");
    ExpectNear(raw->getWidth(), 300, "one item fills the width");
    ExpectNear(raw->getHeight(), 120, "one item fills the height");
}

void TestEqualSplit() {
    auto split = jadefx::make<jadefx::SplitPane>();
    auto left = Box();
    auto right = Box();
    jadefx::Node* leftNode = left.get();
    jadefx::Node* rightNode = right.get();
    split->getItems().add(std::move(left));
    split->getItems().add(std::move(right));
    Expect(split->getDividers().size() == 1, "two items make one divider");
    ExpectNear(split->getDividerPositions()[0], 0.5, "a new divider starts at the middle");
    split->setPrefSize(400, 200);
    auto scene = Show(split, 400, 200);
    ExpectNear(leftNode->getWidth(), 196, "the left half leaves room for the divider");
    ExpectNear(rightNode->getWidth(), 196, "the right half matches the left");
    ExpectNear(leftNode->getHeight(), 200, "both items fill the height");
    const std::vector<jadefx::Node*> dividers = Dividers(*split);
    Expect(dividers.size() == 1, "the divider is in the scene");
    if (!dividers.empty()) {
        Expect(std::string(dividers[0]->getElementType()) == "split-pane-divider", "the divider type is split-pane-divider");
        ExpectNear(dividers[0]->getWidth(), 8, "default divider thickness is 8");
        ExpectNear(dividers[0]->getX(), leftNode->getWidth(), "the divider starts where the left item ends");
        Expect(!dividers[0]->getElementsByClassName("horizontal-grabber").empty(), "a horizontal split uses the horizontal grip");
    }
    scene->noteMove(leftNode->getAbsoluteX() + leftNode->getWidth() + 4, 40);
    Expect(scene->hoverCursor() == jadefx::Cursor::EwResize, "the divider uses the horizontal resize cursor");
    ExpectNear(split->getDividerPositions()[0], 0.5, "laying out an even split keeps the fraction");
}

void TestDividerPositionAndDrag() {
    auto split = jadefx::make<jadefx::SplitPane>();
    auto left = Box();
    auto right = Box();
    left->setMinSize(80, 0);
    jadefx::Node* leftNode = left.get();
    jadefx::Node* rightNode = right.get();
    split->setDividerPositions({0.25});
    split->getItems().add(left);
    split->getItems().add(right);
    ExpectNear(split->getDividerPositions()[0], 0.25, "a position set before the items exist is kept");
    split->setPrefSize(400, 160);
    auto scene = Show(split, 400, 160);
    ExpectNear(leftNode->getWidth(), 96, "a quarter position places the left edge just before 25%");
    ExpectNear(leftNode->getWidth() + rightNode->getWidth() + 8, 400, "the two items and the divider fill the pane");

    const std::vector<jadefx::Node*> dividers = Dividers(*split);
    Expect(!dividers.empty(), "the dragged divider is present");
    if (dividers.empty()) {
        return;
    }
    const double x = dividers[0]->getAbsoluteX() + 4;
    const double y = dividers[0]->getAbsoluteY() + 20;
    scene->noteButton(0, true, x, y);
    scene->noteMove(x - 40, y);
    scene->noteButton(0, false, x - 40, y);
    scene->layout(400, 160, 0);
    ExpectNear(leftNode->getWidth(), 80, "dragging left stops at the item minimum");
    Expect(split->getDividerPositions()[0] < 0.25, "a drag writes a smaller fraction");

    scene->noteButton(0, true, dividers[0]->getAbsoluteX() + 4, y);
    scene->noteMove(dividers[0]->getAbsoluteX() + 4 + 30, y);
    scene->layout(400, 160, 0);
    ExpectNear(leftNode->getWidth(), 110, "dragging right grows the left item");
}

void TestMinAndMax() {
    auto split = jadefx::make<jadefx::SplitPane>();
    auto left = Box();
    auto right = Box();
    left->setMinSize(150, 0);
    left->setMaxSize(220, 10000);
    jadefx::Node* leftNode = left.get();
    jadefx::Node* rightNode = right.get();
    split->getItems().add(left);
    split->getItems().add(right);
    split->setDividerPosition(0, 0.1);
    split->setPrefSize(400, 100);
    Show(split, 400, 100);
    ExpectNear(leftNode->getWidth(), 150, "a small fraction is raised to the minimum");
    ExpectNear(rightNode->getWidth(), 242, "the other item gives up the minimum's extra");

    split->setDividerPosition(0, 0.9);
    Show(split, 400, 100);
    ExpectNear(leftNode->getWidth(), 220, "a large fraction is capped at the maximum");
    ExpectNear(rightNode->getWidth(), 172, "the leftover goes to the other item");
}

void TestResizeKeepsFixedItem() {
    auto split = jadefx::make<jadefx::SplitPane>();
    auto left = Box();
    auto right = Box();
    jadefx::Node* leftNode = left.get();
    jadefx::Node* rightNode = right.get();
    jadefx::SplitPane::setResizableWithParent(*right, false);
    Expect(!jadefx::SplitPane::isResizableWithParent(*right), "the flag is stored on the item");
    jadefx::SplitPane::setResizableWithParent(*right, std::nullopt);
    Expect(jadefx::SplitPane::isResizableWithParent(*right), "clearing the flag makes the item resizable again");
    jadefx::SplitPane::setResizableWithParent(*right, false);

    split->getItems().add(left);
    split->getItems().add(right);
    split->setPrefSize(400, 120);
    auto scene = Show(split, 400, 120);
    ExpectNear(rightNode->getWidth(), 196, "the fixed item starts at half");
    split->setPrefSize(500, 120);
    scene->layout(500, 120, 0);
    ExpectNear(rightNode->getWidth(), 196, "a fixed item keeps its size when the pane grows");
    ExpectNear(leftNode->getWidth(), 296, "the resizable item takes the extra width");
}

void TestThreePanesAndRemoval() {
    auto split = jadefx::make<jadefx::SplitPane>();
    auto first = Box();
    auto second = Box();
    auto third = Box();
    jadefx::Node* firstNode = first.get();
    jadefx::Node* secondNode = second.get();
    jadefx::Node* thirdNode = third.get();
    split->setDividerPositions({0.25, 0.6});
    split->getItems().add(first);
    split->getItems().add(second);
    split->getItems().add(third);
    const std::vector<double> placed = split->getDividerPositions();
    Expect(placed.size() == 2, "three items make two dividers");
    if (placed.size() == 2) {
        ExpectNear(placed[0], 0.25, "the first cached position is applied");
        ExpectNear(placed[1], 0.6, "the second cached position is applied");
    }
    split->setPrefSize(400, 90);
    Show(split, 400, 90);
    Expect(firstNode->getAbsoluteX() + firstNode->getWidth() <= secondNode->getAbsoluteX() + 0.5,
           "the first divider does not cross into the next item");
    ExpectNear(firstNode->getWidth() + secondNode->getWidth() + thirdNode->getWidth() + 16, 400,
               "three items and two dividers fill the pane");

    const double kept = split->getDividerPositions()[0];
    split->getItems().removeAt(2);
    Expect(split->getDividers().size() == 1, "removing the last item drops one divider");
    ExpectNear(split->getDividerPositions()[0], kept, "the remaining divider keeps its fraction");

    auto extra = Box();
    split->getItems().add(extra);
    Expect(split->getDividers().size() == 2, "adding an item grows the divider list");
    ExpectNear(split->getDividers()[0].getPosition(), kept, "the existing divider keeps its fraction");
    ExpectNear(split->getDividers()[1].getPosition(), 0.5, "the new divider starts in the middle");
}

void TestVerticalAndCss() {
    auto split = jadefx::make<jadefx::SplitPane>();
    auto top = Box();
    auto bottom = Box();
    top->setMaxSize(10000, 120);
    jadefx::Node* topNode = top.get();
    jadefx::Node* bottomNode = bottom.get();
    split->setOrientation(jadefx::Orientation::Vertical);
    Expect(split->pseudoState("vertical") && !split->pseudoState("horizontal"), "vertical turns on its own pseudo");
    split->getItems().add(top);
    split->getItems().add(bottom);
    split->setDividerPosition(0, 0.25);
    split->setPrefSize(180, 400);
    auto scene = jadefx::make<jadefx::Scene>(split, 180, 400);
    scene->setStylesheet(
        "split-pane:vertical > .split-pane-divider { padding: 0 9px; cursor: crosshair; }");
    scene->layout(180, 400, 0);
    ExpectNear(topNode->getHeight(), 91, "a quarter of the height leaves half the divider");
    ExpectNear(bottomNode->getHeight(), 291, "the lower item takes the rest");
    const std::vector<jadefx::Node*> dividers = Dividers(*split);
    Expect(!dividers.empty(), "the vertical divider is present");
    if (!dividers.empty()) {
        ExpectNear(dividers[0]->getHeight(), 18, "vertical thickness still follows left and right padding");
        Expect(!dividers[0]->getElementsByClassName("vertical-grabber").empty(), "a vertical split uses the vertical grip");
    }
    if (!dividers.empty()) {
        scene->noteMove(40, dividers[0]->getAbsoluteY() + 4);
        Expect(scene->hoverCursor() == jadefx::Cursor::Crosshair, "a divider rule can replace the resize cursor");
    }

    split->setDividerPosition(0, 0.8);
    scene->layout(180, 400, 0);
    ExpectNear(topNode->getHeight(), 120, "the vertical maximum stops the divider");

    auto styled = jadefx::make<jadefx::SplitPane>();
    styled->setOrientation(jadefx::Orientation::Horizontal);
    styled->getItems().add(Box());
    styled->getItems().add(Box());
    styled->setPrefSize(120, 80);
    auto verticalScene = jadefx::make<jadefx::Scene>(styled, 120, 80);
    verticalScene->setStylesheet("split-pane { orientation: vertical; }");
    verticalScene->layout(120, 80, 0);
    Expect(styled->getOrientation() == jadefx::Orientation::Vertical, "orientation in CSS stacks the items");
    Expect(styled->pseudoState("vertical"), "the CSS orientation turns on :vertical");
}

void TestPercentMinimum() {
    auto split = jadefx::make<jadefx::SplitPane>();
    auto left = Box();
    left->setStyle("min-width: 50%;");
    jadefx::Node* leftNode = left.get();
    split->getItems().add(left);
    split->getItems().add(Box());
    split->setDividerPosition(0, 0.1);
    split->setPrefSize(400, 80);
    Show(split, 400, 80);
    ExpectNear(leftNode->getWidth(), 200, "a percent minimum is a fraction of the pane");
}

void TestDividerOrder() {
    auto split = jadefx::make<jadefx::SplitPane>();
    auto first = Box();
    auto second = Box();
    auto third = Box();
    split->getItems().add(first);
    split->getItems().add(second);
    split->getItems().add(third);
    split->setDividerPositions({0.8, 0.2});
    split->setPrefSize(400, 80);
    Show(split, 400, 80);
    const std::vector<jadefx::Node*> dividers = Dividers(*split);
    Expect(dividers.size() == 2, "both dividers are laid out");
    if (dividers.size() == 2) {
        Expect(dividers[1]->getX() + 0.5 >= dividers[0]->getX() + dividers[0]->getWidth(),
               "a later divider is pushed past the earlier one");
    }
    split->setDisable(true);
    auto scene = Show(split, 400, 80);
    if (dividers.size() == 2) {
        const double before = split->getDividerPositions()[0];
        scene->noteButton(0, true, dividers[0]->getAbsoluteX() + 2, 10);
        scene->noteMove(dividers[0]->getAbsoluteX() + 40, 10);
        scene->layout(400, 80, 0);
        ExpectNear(split->getDividerPositions()[0], before, "a disabled split pane does not drag");
    }
}

}  // namespace

int RunSplitPaneTests() {
    const int before = gFailures;
    TestEmptyAndSingle();
    TestEqualSplit();
    TestDividerPositionAndDrag();
    TestMinAndMax();
    TestResizeKeepsFixedItem();
    TestThreePanesAndRemoval();
    TestVerticalAndCss();
    TestPercentMinimum();
    TestDividerOrder();
    return gFailures - before;
}

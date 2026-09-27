#include "jadefx/jadefx.hpp"

#include <cmath>
#include <cstdio>
#include <memory>

// GridPane and FlowPane.
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

// A pane of a fixed size at the scene's top left.
template <typename P>
struct Rig {
    std::shared_ptr<P> pane;
    std::shared_ptr<jadefx::Scene> scene;

    explicit Rig(std::shared_ptr<P> made, double width, double height) : pane(std::move(made)) {
        pane->setPrefSize(width, height);
        auto root = jadefx::make<jadefx::Pane>();
        root->getChildren().add(pane);
        scene = jadefx::make<jadefx::Scene>(root, 600, 600);
        frame();
    }

    void frame() { scene->layout(600, 600, 0); }
    double x(const jadefx::Node& node) const { return node.getAbsoluteX() - pane->getAbsoluteX(); }
    double y(const jadefx::Node& node) const { return node.getAbsoluteY() - pane->getAbsoluteY(); }
};

void TestGridBasics() {
    Rig<jadefx::GridPane> rig(jadefx::make<jadefx::GridPane>(), 400, 300);
    auto a = Block(50, 20);
    auto b = Block(80, 30);
    auto c = Block(60, 40);
    auto d = Block(30, 10);
    rig.pane->add(a, 0, 0);
    rig.pane->add(b, 1, 0);
    rig.pane->add(c, 0, 1);
    rig.pane->add(d, 1, 1);
    rig.pane->setHgap(10);
    rig.pane->setVgap(5);
    rig.frame();
    Expect(rig.pane->getColumnCount() == 2 && rig.pane->getRowCount() == 2, "two columns and two rows");
    Expect(Near(rig.pane->getColumnWidths()[0], 60) && Near(rig.pane->getColumnWidths()[1], 80),
           "a column is as wide as its widest child");
    Expect(Near(rig.pane->getRowHeights()[0], 30) && Near(rig.pane->getRowHeights()[1], 40),
           "a row is as tall as its tallest child");
    Expect(Near(rig.x(*b), 70) && Near(rig.y(*c), 35), "gaps separate columns and rows");
    Expect(Near(rig.y(*a), 5) && Near(a->getWidth(), 50), "a child keeps its size, centered in its row by default");

    jadefx::GridPane::setHalignment(*a, jadefx::HPos::Right);
    jadefx::GridPane::setValignment(*a, jadefx::VPos::Top);
    rig.frame();
    Expect(Near(rig.x(*a), 10) && Near(rig.y(*a), 0), "halignment and valignment place a child in its cell");
    jadefx::GridPane::setFillWidth(*d, true);
    jadefx::GridPane::setMargin(*d, jadefx::Insets{2, 4, 2, 4});
    rig.frame();
    Expect(Near(d->getWidth(), 72) && Near(rig.x(*d), 74), "fillWidth stretches a child to its cell, inside its margin");

    auto e = Block(20, 20);
    auto f = Block(20, 20);
    rig.pane->addRow(0, {e, f});
    Expect(jadefx::GridPane::getColumnIndex(*e) == 2 && jadefx::GridPane::getColumnIndex(*f) == 3,
           "addRow continues after the row's last child");
    auto wide = Block(300, 10);
    rig.pane->add(wide, 0, 2, jadefx::GridPane::REMAINING, 1);
    rig.frame();
    Expect(rig.pane->getColumnCount() == 4 && Near(wide->getWidth(), 300), "REMAINING spans every column");
    double spanned = 30;  // three gaps
    for (const double width : rig.pane->getColumnWidths()) {
        spanned += width;
    }
    Expect(spanned >= 300 - 0.5, "the spanned columns widen to fit the child");

    rig.pane->setStyle("column-gap: 20px; row-gap: 0px;");
    rig.frame();
    Expect(Near(rig.x(*b) - rig.x(*c), rig.pane->getColumnWidths()[0] + 20), "CSS column-gap wins over setHgap");
}

void TestGridConstraints() {
    Rig<jadefx::GridPane> rig(jadefx::make<jadefx::GridPane>(), 400, 100);
    auto left = Block(50, 20);
    auto middle = Block(50, 20);
    auto right = Block(50, 20);
    rig.pane->add(left, 0, 0);
    rig.pane->add(middle, 1, 0);
    rig.pane->add(right, 2, 0);
    rig.pane->getColumnConstraints() = {jadefx::ColumnConstraints(100), jadefx::ColumnConstraints(),
                                        jadefx::ColumnConstraints()};
    rig.pane->getColumnConstraints()[2].hgrow = jadefx::Priority::Always;
    rig.frame();
    const std::vector<double>& widths = rig.pane->getColumnWidths();
    Expect(Near(widths[0], 100) && Near(widths[1], 50) && Near(widths[2], 250),
           "a fixed column keeps its width and an Always column takes the rest");

    rig.pane->getColumnConstraints()[2].maxWidth = 120;
    jadefx::GridPane::setHgrow(*middle, jadefx::Priority::Sometimes);
    rig.frame();
    Expect(Near(rig.pane->getColumnWidths()[2], 120) && Near(rig.pane->getColumnWidths()[1], 180),
           "a grower stops at its maximum and the rest goes to Sometimes columns");
    rig.pane->getColumnConstraints()[2].hgrow = jadefx::Priority::Never;
    rig.frame();
    Expect(Near(rig.pane->getColumnWidths()[1], 250), "with no Always column, a Sometimes one grows");

    rig.pane->getColumnConstraints() = {jadefx::ColumnConstraints(), jadefx::ColumnConstraints(),
                                        jadefx::ColumnConstraints()};
    rig.pane->getColumnConstraints()[0].percentWidth = 50;
    jadefx::GridPane::setHgrow(*middle, jadefx::Priority::Never);
    rig.frame();
    Expect(Near(rig.pane->getColumnWidths()[0], 200), "percentWidth takes a share of the pane");

    rig.pane->getColumnConstraints().clear();
    rig.pane->setPrefSize(90, 100);
    rig.frame();
    double total = 0;
    for (const double width : rig.pane->getColumnWidths()) {
        total += width;
    }
    Expect(Near(total, 90), "columns shrink to fit a narrow pane");

    rig.pane->setPrefSize(400, 100);
    rig.pane->setAlignment(jadefx::Pos::Center);
    rig.frame();
    Expect(Near(rig.x(*left), 125), "the grid is centered in the pane");

    auto sized = jadefx::make<jadefx::GridPane>();
    sized->add(Block(40, 10), 0, 0);
    sized->add(Block(60, 30), 1, 1);
    sized->setHgap(5);
    auto holder = jadefx::make<jadefx::Scene>(sized, 400, 400);
    holder->layout(400, 400, 0);
    Expect(Near(sized->measuredWidth(400), 105) && Near(sized->measuredHeight(105, -1), 40),
           "the grid's preferred size is its columns and rows");
}

void TestFlow() {
    Rig<jadefx::FlowPane> rig(jadefx::make<jadefx::FlowPane>(10, 5), 200, 200);
    std::vector<std::shared_ptr<jadefx::Pane>> items;
    for (int i = 0; i < 5; ++i) {
        items.push_back(Block(60, i == 1 ? 30 : 20));
        rig.pane->getChildren().add(items.back());
    }
    rig.frame();
    Expect(Near(rig.x(*items[2]), 140) && Near(rig.y(*items[2]), 5), "three fit in a row, centered on the tallest");
    Expect(Near(rig.x(*items[3]), 0) && Near(rig.y(*items[3]), 35), "the fourth wraps to the next row");

    rig.pane->setAlignment(jadefx::Pos::TopCenter);
    rig.pane->setRowValignment(jadefx::VPos::Top);
    rig.frame();
    Expect(Near(rig.x(*items[3]), 35) && Near(rig.y(*items[0]), 0), "rows center in the pane, and rowValignment Top");

    jadefx::FlowPane::setMargin(*items[0], jadefx::Insets{0, 20, 0, 0});
    rig.frame();
    Expect(Near(rig.y(*items[2]), 35), "a margin makes room, pushing the third child to the next row");

    rig.pane->setOrientation(jadefx::Orientation::Vertical);
    rig.pane->setAlignment(jadefx::Pos::TopLeft);
    jadefx::FlowPane::clearConstraints(*items[0]);
    rig.pane->setPrefSize(200, 60);
    rig.frame();
    Expect(Near(rig.x(*items[1]), 0) && Near(rig.y(*items[1]), 25) && Near(rig.x(*items[2]), 70),
           "a vertical flow fills columns and wraps to the right");

    auto flow = jadefx::make<jadefx::FlowPane>(10, 10);
    for (int i = 0; i < 6; ++i) {
        flow->getChildren().add(Block(100, 20));
    }
    flow->setPrefWrapLength(250);
    auto holder = jadefx::make<jadefx::Scene>(flow, 600, 600);
    holder->layout(600, 600, 0);
    Expect(Near(flow->measuredWidth(600), 210), "the preferred width wraps at prefWrapLength");
    Expect(Near(flow->measuredHeight(320, -1), 50), "the preferred height wraps at the width it is given");
    flow->setStyle("gap: 0px;");
    holder->layout(600, 600, 0);
    Expect(Near(flow->measuredWidth(600), 200), "CSS gap wins over the pane's gaps");
}

}  // namespace

int RunLayoutPaneTests() {
    TestGridBasics();
    TestGridConstraints();
    TestFlow();
    if (gFailures == 0) {
        std::printf("layout pane tests passed\n");
    }
    return gFailures;
}

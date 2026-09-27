#include "jadefx/jadefx.hpp"

#include <cmath>
#include <cstdio>
#include <memory>

namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

bool Near(double a, double b, double tolerance = 0.5) { return std::fabs(a - b) <= tolerance; }

std::shared_ptr<jadefx::Pane> Block(double width, double height) {
    auto block = jadefx::make<jadefx::Pane>();
    block->setPrefSize(width, height);
    return block;
}

void Press(jadefx::Scene& scene, double x, double y) {
    scene.noteButton(0, true, x, y, 0);
}

void Release(jadefx::Scene& scene, double x, double y) {
    scene.noteButton(0, false, x, y, 0);
}

void TestScrollBarValues() {
    jadefx::ScrollBar bar;
    int changes = 0;
    bar.setOnValueChanged([&] { ++changes; });
    Expect(bar.getMin() == 0 && bar.getMax() == 100 && bar.getValue() == 0, "a bar starts at 0 of 0 to 100");
    bar.setValue(150);
    Expect(bar.getValue() == 100 && changes == 1, "value is clamped to max");
    bar.setValue(100);
    Expect(changes == 1, "setting the same value does not report a change");
    bar.setMax(40);
    Expect(bar.getValue() == 40 && changes == 2, "lowering max pulls value down");
    bar.setMin(60);
    Expect(bar.getMax() == 60 && bar.getValue() == 60, "a min above max raises max");
    bar.setMin(0);
    bar.setMax(100);
    bar.setValue(50);
    bar.setUnitIncrement(5);
    bar.increment();
    Expect(bar.getValue() == 55, "increment moves by the unit increment");
    bar.decrement();
    bar.decrement();
    Expect(bar.getValue() == 45, "decrement moves back by the unit increment");
    bar.setVisiblePortion(200, 800);
    Expect(Near(bar.getVisibleAmount(), 25), "a quarter of the content on screen is a quarter of the range");
    Expect(Near(bar.getBlockIncrement(), 100.0 / 3.0), "a block is one viewport of the 600 points of travel");
    bar.setVisiblePortion(900, 800);
    Expect(Near(bar.getVisibleAmount(), 100), "a viewport past the content shows the whole range");
}

void TestScrollBarInput() {
    auto bar = jadefx::make<jadefx::ScrollBar>(jadefx::Orientation::Vertical);
    auto root = jadefx::make<jadefx::Pane>();
    root->getChildren().add(bar);
    bar->setPrefSize(10, 200);
    auto scene = jadefx::make<jadefx::Scene>(root, 100, 300);
    scene->layout(100, 300, 0);
    bar->setMax(300);
    bar->setVisiblePortion(100, 400);
    bar->setBlockIncrement(75);
    const double x = bar->getAbsoluteX() + bar->getWidth() - 2;
    const double top = bar->getAbsoluteY();
    // The range is three visible amounts, so the thumb is a quarter of the track: 50 points.
    Press(*scene, x, top + 150);
    Release(*scene, x, top + 150);
    Expect(Near(bar->getValue(), 75), "a press below the thumb pages by the block increment");
    bar->setValue(0);
    Press(*scene, x, top + 10);
    Expect(bar->isValueChanging(), "a press on the thumb starts a drag");
    scene->noteMove(x, top + 160);
    Expect(Near(bar->getValue(), 300), "dragging the thumb to the end reaches max");
    scene->noteMove(x, top + 85);
    Expect(Near(bar->getValue(), 150), "dragging the thumb halfway is half the range");
    Release(*scene, x, top + 85);
    Expect(!bar->isValueChanging(), "the release ends the drag");

    bar->requestFocus();
    bar->setUnitIncrement(10);
    scene->noteKey(jadefx::Key::Down, true, false, 0);
    Expect(Near(bar->getValue(), 160), "Down moves a vertical bar forward");
    scene->noteKey(jadefx::Key::Left, true, false, 0);
    Expect(Near(bar->getValue(), 160), "Left does not move a vertical bar");
    scene->noteKey(jadefx::Key::End, true, false, 0);
    Expect(Near(bar->getValue(), 300), "End moves to max");
    scene->noteKey(jadefx::Key::PageUp, true, false, 0);
    Expect(Near(bar->getValue(), 225), "Page Up moves back by the block increment");
}

struct PaneRig {
    std::shared_ptr<jadefx::Pane> content = Block(400, 1000);
    std::shared_ptr<jadefx::ScrollPane> pane = jadefx::make<jadefx::ScrollPane>(content);
    std::shared_ptr<jadefx::Scene> scene;

    PaneRig() {
        pane->setPrefSize(200, 300);
        auto root = jadefx::make<jadefx::Pane>();
        root->getChildren().add(pane);
        scene = jadefx::make<jadefx::Scene>(root, 400, 400);
        frame();
    }

    void frame() { scene->layout(400, 400, 0); }
};

jadefx::Node* FindBar(jadefx::Node& root, jadefx::Orientation orientation) {
    for (jadefx::Node* node : root.getElementsByClassName("scroll-bar")) {
        auto* bar = dynamic_cast<jadefx::ScrollBar*>(node);
        if (bar != nullptr && bar->getOrientation() == orientation) {
            return bar;
        }
    }
    return nullptr;
}

void TestScrollPaneLayout() {
    PaneRig rig;
    const jadefx::Size view = rig.pane->getViewportBounds();
    Expect(Near(view.width, 200 - jadefx::ScrollBar::kThickness) && Near(view.height, 300 - jadefx::ScrollBar::kThickness),
           "both bars take their thickness from the viewport");
    jadefx::Node* vbar = FindBar(*rig.pane, jadefx::Orientation::Vertical);
    jadefx::Node* hbar = FindBar(*rig.pane, jadefx::Orientation::Horizontal);
    Expect(vbar != nullptr && vbar->isVisible() && hbar != nullptr && hbar->isVisible(),
           "content larger on both axes shows both bars");
    Expect(Near(rig.content->getWidth(), 400) && Near(rig.content->getHeight(), 1000), "content keeps its preferred size");
    Expect(Near(rig.content->getAbsoluteY(), rig.pane->getAbsoluteY()), "vvalue 0 shows the top");

    rig.pane->setVvalue(1);
    rig.frame();
    Expect(Near(rig.content->getAbsoluteY() + rig.content->getHeight(), rig.pane->getAbsoluteY() + view.height),
           "vvalue 1 shows the bottom");
    rig.pane->setVvalue(0.5);
    rig.pane->setPrefSize(200, 500);
    rig.frame();
    const double range = 1000 - rig.pane->getViewportBounds().height;
    Expect(Near(rig.pane->getAbsoluteY() - rig.content->getAbsoluteY(), range * 0.5),
           "a resize keeps vvalue, so the view stays halfway");

    rig.pane->setFitToWidth(true);
    rig.frame();
    Expect(Near(rig.content->getWidth(), rig.pane->getViewportBounds().width), "fitToWidth sizes the content to the viewport");
    Expect(!hbar->isVisible(), "fitToWidth needs no horizontal bar");

    rig.pane->setVbarPolicy(jadefx::ScrollBarPolicy::Never);
    rig.frame();
    Expect(!vbar->isVisible() && Near(rig.pane->getViewportBounds().width, 200), "Never hides the bar and frees its room");

    rig.pane->setVbarPolicy(jadefx::ScrollBarPolicy::Always);
    rig.content->setPrefSize(10, 10);
    rig.frame();
    Expect(vbar->isVisible(), "Always shows the bar with nothing to scroll");

    auto sized = jadefx::make<jadefx::ScrollPane>(Block(120, 80));
    // Sizes are resolved by styling, which runs when a scene lays out.
    auto holder = jadefx::make<jadefx::Scene>(sized, 300, 300);
    holder->layout(300, 300, 0);
    Expect(Near(sized->measuredWidth(1000), 120) && Near(sized->measuredHeight(120, -1), 80),
           "the pane's preferred size is its content's");
    sized->setPrefViewportWidth(50);
    sized->setPrefViewportHeight(40);
    holder->layout(300, 300, 0);
    Expect(Near(sized->measuredWidth(1000), 50) && Near(sized->measuredHeight(50, -1), 40),
           "prefViewportWidth and prefViewportHeight replace it");
}

void TestScrollPaneInput() {
    PaneRig rig;
    int vchanges = 0;
    rig.pane->setOnVvalueChanged([&] { ++vchanges; });
    const double x = rig.pane->getAbsoluteX() + 50;
    const double y = rig.pane->getAbsoluteY() + 50;
    rig.scene->noteScroll(x, y, 0, -1);
    rig.frame();
    Expect(Near(rig.pane->getAbsoluteY() - rig.content->getAbsoluteY(), 40), "a wheel notch scrolls 40 points");
    Expect(vchanges == 1, "the wheel reports a vvalue change");

    rig.scene->noteScroll(x, y, -1, 0);
    rig.frame();
    Expect(Near(rig.pane->getAbsoluteX() - rig.content->getAbsoluteX(), 40), "a sideways swipe scrolls horizontally");

    rig.pane->requestFocus();
    rig.scene->noteKey(jadefx::Key::PageDown, true, false, 0);
    rig.frame();
    Expect(Near(rig.pane->getAbsoluteY() - rig.content->getAbsoluteY(), 40 + rig.pane->getViewportBounds().height),
           "Page Down scrolls a viewport");
    rig.scene->noteKey(jadefx::Key::End, true, false, 0);
    Expect(rig.pane->getVvalue() == 1, "End scrolls to the bottom");
    rig.scene->noteKey(jadefx::Key::Home, true, false, 0);
    Expect(rig.pane->getVvalue() == 0, "Home scrolls to the top");

    // A press on the vertical bar's track pages, and does not take focus from the pane.
    jadefx::Node* vbar = FindBar(*rig.pane, jadefx::Orientation::Vertical);
    const double barX = vbar->getAbsoluteX() + vbar->getWidth() - 2;
    Press(*rig.scene, barX, vbar->getAbsoluteY() + vbar->getHeight() - 20);
    Release(*rig.scene, barX, vbar->getAbsoluteY() + vbar->getHeight() - 20);
    rig.frame();
    Expect(rig.pane->getVvalue() > 0, "a press on the bar's track scrolls the pane");
    Expect(!vbar->isFocused() && rig.pane->isFocused(), "the bar hands focus to the pane");
}

void TestScrollPaneClipping() {
    PaneRig rig;
    auto button = jadefx::make<jadefx::Button>("Hit");
    button->setPrefSize(100, 30);
    auto column = jadefx::make<jadefx::VBox>();
    column->getChildren().add(Block(10, 280));
    column->getChildren().add(button);
    column->getChildren().add(Block(10, 400));
    rig.pane->setContent(column);
    Expect(rig.content->getParent() == nullptr, "replacing the content releases the old node");
    rig.frame();
    // The button starts just below the viewport's bottom edge, under the horizontal bar area.
    const double belowX = button->getAbsoluteX() + 20;
    const double belowY = rig.pane->getAbsoluteY() + rig.pane->getHeight() + 5;
    Expect(button->getAbsoluteY() < belowY, "the button reaches past the pane");
    Expect(rig.scene->pick(belowX, belowY) != button.get(), "content outside the viewport cannot be picked");
    rig.pane->setVvalue(0.5);
    rig.frame();
    const double midX = button->getAbsoluteX() + 20;
    const double midY = button->getAbsoluteY() + 10;
    Expect(rig.scene->pick(midX, midY) == button.get(), "scrolled into view, the button takes the pick");

    auto other = jadefx::make<jadefx::Pane>();
    other->getChildren().add(column);
    Expect(rig.pane->getContent() == nullptr, "content taken by another parent leaves the pane");
}

void TestNestedScrollHandsOver() {
    auto inner = jadefx::make<jadefx::ScrollPane>(Block(100, 400));
    inner->setPrefSize(120, 200);
    auto column = jadefx::make<jadefx::VBox>();
    column->getChildren().add(inner);
    column->getChildren().add(Block(100, 800));
    auto outer = jadefx::make<jadefx::ScrollPane>(column);
    outer->setPrefSize(200, 300);
    auto root = jadefx::make<jadefx::Pane>();
    root->getChildren().add(outer);
    auto scene = jadefx::make<jadefx::Scene>(root, 400, 400);
    scene->layout(400, 400, 0);
    const double x = inner->getAbsoluteX() + 20;
    const double y = inner->getAbsoluteY() + 20;
    scene->noteScroll(x, y, 0, -1);
    Expect(inner->getVvalue() > 0 && outer->getVvalue() == 0, "the inner pane takes the wheel first");
    inner->setVvalue(1);
    scene->layout(400, 400, 0);
    scene->noteScroll(x, y, 0, -1);
    Expect(outer->getVvalue() > 0, "at its end the inner pane lets the outer one scroll");
}

}  // namespace

int RunScrollPaneTests() {
    TestScrollBarValues();
    TestScrollBarInput();
    TestScrollPaneLayout();
    TestScrollPaneInput();
    TestScrollPaneClipping();
    TestNestedScrollHandsOver();
    if (gFailures == 0) {
        std::printf("scroll pane tests passed\n");
    }
    return gFailures;
}

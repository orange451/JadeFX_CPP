#include "jadefx/jadefx.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Incremental passes: a frame restyles and lays out only what changed.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

bool Is(const jadefx::Color& color, int r, int g, int b) {
    auto close = [](float channel, int value) { return std::fabs(channel * 255.f - static_cast<float>(value)) < 1.5f; };
    return close(color.r, r) && close(color.g, g) && close(color.b, b);
}

const char* const kFixtureCss = R"css(
.item { padding: 2px; }
.item.on { background-color: #ff0000; }
.item.wide { padding: 12px; }
#special { background-color: #00ff00; }
.item:hover .inner { color: #0000ff; }
.item:active { background-color: #111111; }
.item:selected { background-color: #222222; }
.item:disabled .inner { color: #333333; }
.item:open { background-color: #444444; }
.panel:focus-within { background-color: #00ffff; }
.item:focus { background-color: #ff00ff; }
.fade { background-color: #000000; transition: background-color 0.5s; }
.fade.on { background-color: #ffffff; }
)css";

// Two panels in a column, each holding an item with a label inside. A test
// changes panel A's side and checks that panel B's side was not touched.
struct Fixture {
    std::shared_ptr<jadefx::Scene> scene;
    std::shared_ptr<jadefx::VBox> root;
    std::shared_ptr<jadefx::StackPane> panelA;
    std::shared_ptr<jadefx::StackPane> target;
    std::shared_ptr<jadefx::Label> targetLabel;
    std::shared_ptr<jadefx::StackPane> panelB;
    std::shared_ptr<jadefx::StackPane> sibling;
    std::shared_ptr<jadefx::Label> siblingLabel;
    double time = 0;

    void frame() {
        time += 1.0 / 60.0;
        scene->layout(300, 200, time);
    }
};

std::shared_ptr<jadefx::StackPane> Item(const char* text, std::shared_ptr<jadefx::Label>& label) {
    auto item = jadefx::make<jadefx::StackPane>();
    item->getClassList().add("item");
    item->setPrefSize(120, 40);
    label = jadefx::make<jadefx::Label>(text);
    label->getClassList().add("inner");
    item->getChildren().add(label);
    return item;
}

std::shared_ptr<jadefx::StackPane> Panel(const std::shared_ptr<jadefx::StackPane>& item) {
    auto panel = jadefx::make<jadefx::StackPane>();
    panel->getClassList().add("panel");
    panel->setPrefSize(300, 90);
    panel->getChildren().add(item);
    return panel;
}

Fixture MakeFixture() {
    Fixture f;
    f.root = jadefx::make<jadefx::VBox>();
    f.target = Item("target", f.targetLabel);
    f.sibling = Item("sibling", f.siblingLabel);
    f.panelA = Panel(f.target);
    f.panelB = Panel(f.sibling);
    f.root->getChildren().add(f.panelA);
    f.root->getChildren().add(f.panelB);
    f.scene = jadefx::make<jadefx::Scene>(f.root, 300, 200);
    f.scene->setStylesheet(kFixtureCss);
    f.scene->setIncrementalUpdates(true);
    f.scene->layout(300, 200, 0);
    return f;
}

struct Seen {
    std::uint32_t restyles = 0;
    std::uint32_t layouts = 0;
};

Seen Of(const jadefx::Node& node) { return {node.debugRestyleCount(), node.debugLayoutCount()}; }
bool Restyled(const jadefx::Node& node, const Seen& before) { return node.debugRestyleCount() > before.restyles; }
bool LaidOut(const jadefx::Node& node, const Seen& before) { return node.debugLayoutCount() > before.layouts; }

void TestIdleFrameDoesNothing() {
    Fixture f = MakeFixture();
    f.frame();
    std::vector<std::pair<const jadefx::Node*, Seen>> before;
    for (const jadefx::Node* node : std::vector<const jadefx::Node*>{
             f.scene.get(), f.root.get(), f.panelA.get(), f.target.get(), f.targetLabel.get(), f.panelB.get(),
             f.sibling.get(), f.siblingLabel.get()}) {
        before.emplace_back(node, Of(*node));
    }
    f.frame();
    f.frame();
    bool idle = true;
    for (const auto& entry : before) {
        idle = idle && !Restyled(*entry.first, entry.second) && !LaidOut(*entry.first, entry.second);
    }
    Expect(idle, "an idle frame restyles and lays out nothing");
}

void TestNewNodesStyleAndLayout() {
    Fixture f = MakeFixture();
    Expect(f.target->debugRestyleCount() == 1 && f.target->debugLayoutCount() == 1,
           "the first frame styles and lays out every new node once");
}

void TestChildAddTouchesOnlyItsParent() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen target = Of(*f.target);
    const Seen targetLabel = Of(*f.targetLabel);
    const Seen panelA = Of(*f.panelA);
    const Seen panelB = Of(*f.panelB);
    const Seen sibling = Of(*f.sibling);
    auto added = jadefx::make<jadefx::StackPane>();
    f.panelA->getChildren().add(added);
    f.frame();
    Expect(added->debugRestyleCount() == 1 && added->debugLayoutCount() == 1, "an added child is styled and laid out");
    Expect(Restyled(*f.target, target) && Restyled(*f.targetLabel, targetLabel),
           "an added child restyles its siblings' subtrees, since :nth-child may change");
    Expect(LaidOut(*f.panelA, panelA), "an added child lays out its parent");
    Expect(!Restyled(*f.sibling, sibling) && !LaidOut(*f.panelB, panelB), "an added child leaves the other panel alone");
}

void TestChildRemoveTouchesOnlyItsParent() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen target = Of(*f.target);
    const Seen sibling = Of(*f.sibling);
    f.target->getChildren().clear();
    f.frame();
    Expect(LaidOut(*f.target, target), "removing a child lays out its parent");
    Expect(!LaidOut(*f.sibling, sibling) && !Restyled(*f.sibling, sibling), "removing a child leaves the other panel alone");
}

void TestFullPassWhenIncrementalOff() {
    Fixture f = MakeFixture();
    f.scene->setIncrementalUpdates(false);
    f.target->setPrefSize(150, 40);
    f.frame();
    Expect(f.target->computedStyle().width.pixels == 150, "with incremental passes off every node restyles each frame");
}

}  // namespace

int RunIncrementalTests() {
    TestIdleFrameDoesNothing();
    TestNewNodesStyleAndLayout();
    TestChildAddTouchesOnlyItsParent();
    TestChildRemoveTouchesOnlyItsParent();
    TestFullPassWhenIncrementalOff();
    return gFailures;
}

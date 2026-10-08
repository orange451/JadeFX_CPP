#include "jadefx/jadefx.hpp"
#include "scene/IncrementalCheck.hpp"

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

void TestVerifyAgreesOnCleanScene() {
    Fixture f = MakeFixture();
    f.frame();
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "a full pass agrees with an incremental one");
}

void TestCompareNamesPathAndField() {
    Fixture f = MakeFixture();
    const std::vector<jadefx::NodeSnapshot> shots = jadefx::IncrementalCheck::snapshot(*f.scene);
    std::vector<jadefx::NodeSnapshot> changed = shots;
    for (jadefx::NodeSnapshot& shot : changed) {
        if (shot.node == f.targetLabel.get()) {
            shot.style.padding.left += 3;
        }
    }
    const std::string padding = jadefx::IncrementalCheck::compare(shots, changed);
    Expect(padding.find("padding") != std::string::npos, "a style difference names the field");
    Expect(padding.find("label.inner") != std::string::npos, "a style difference names the node's path");
    changed = shots;
    for (jadefx::NodeSnapshot& shot : changed) {
        if (shot.node == f.target.get()) {
            shot.width += 1;
        }
    }
    Expect(jadefx::IncrementalCheck::compare(shots, changed).find("bounds") != std::string::npos,
           "a bounds difference says so");
}

// Runs mutate between two frames and checks that only panel A's side restyled.
void ExpectRestyle(const char* what, const std::function<void(Fixture&)>& mutate, bool labelToo) {
    Fixture f = MakeFixture();
    f.frame();
    const Seen target = Of(*f.target);
    const Seen label = Of(*f.targetLabel);
    const Seen sibling = Of(*f.sibling);
    const Seen siblingLabel = Of(*f.siblingLabel);
    mutate(f);
    f.frame();
    const std::string name(what);
    Expect(Restyled(*f.target, target), (name + " restyles the node").c_str());
    if (labelToo) {
        Expect(Restyled(*f.targetLabel, label), (name + " restyles the node's subtree").c_str());
    }
    Expect(!Restyled(*f.sibling, sibling) && !Restyled(*f.siblingLabel, siblingLabel),
           (name + " leaves the other panel alone").c_str());
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), (name + " matches a full pass").c_str());
}

void TestStyleSources() {
    ExpectRestyle("adding a class", [](Fixture& f) { f.target->getClassList().add("on"); }, true);
    ExpectRestyle("removing a class",
                  [](Fixture& f) { f.target->getClassList().removeIf([](const std::string& n) { return n == "item"; }); },
                  true);
    ExpectRestyle("changing the id", [](Fixture& f) { f.target->setElementId("special"); }, true);
    ExpectRestyle("an inline style", [](Fixture& f) { f.target->setStyle("background-color: #123456;"); }, false);
    ExpectRestyle("a node's stylesheet", [](Fixture& f) { f.target->setStylesheet(".inner { color: #654321; }"); }, true);
    ExpectRestyle("pressing", [](Fixture& f) { f.target->setPressed(true); }, true);
    ExpectRestyle("selecting", [](Fixture& f) { f.target->setSelected(true); }, true);
    ExpectRestyle("disabling", [](Fixture& f) { f.target->setDisable(true); }, true);
    ExpectRestyle("a pseudo-class state", [](Fixture& f) { f.target->setPseudoState("open", true); }, true);
    ExpectRestyle("a size set from code", [](Fixture& f) { f.target->setPrefSize(150, 40); }, false);
    ExpectRestyle("a background set from code", [](Fixture& f) { f.target->setBackground(jadefx::Color::black()); }, false);
}

void TestStyleSourceResults() {
    Fixture f = MakeFixture();
    f.target->setDisable(true);
    f.frame();
    Expect(Is(f.targetLabel->computedStyle().color, 51, 51, 51), "a descendant of a disabled node matches :disabled");
    f.target->setPressed(true);
    f.frame();
    Expect(Is(f.target->computedStyle().background.color, 17, 17, 17), "a pressed node matches :active");
}

void TestHoverRestylesDescendants() {
    Fixture f = MakeFixture();
    // Inside panel A but outside the item, so the root and the panel are already hovered.
    f.scene->noteMove(f.panelA->getAbsoluteX() + 2, f.panelA->getAbsoluteY() + 2);
    f.frame();
    const Seen label = Of(*f.targetLabel);
    const Seen sibling = Of(*f.sibling);
    f.scene->noteMove(f.target->getAbsoluteX() + 4, f.target->getAbsoluteY() + 4);
    f.frame();
    Expect(Restyled(*f.targetLabel, label), "hovering an item restyles the label inside it");
    Expect(Is(f.targetLabel->computedStyle().color, 0, 0, 255), "the label matches .item:hover .inner");
    Expect(!Restyled(*f.sibling, sibling), "hovering one item leaves the other alone");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "hover matches a full pass");
}

void TestInheritedChangePropagates() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen label = Of(*f.targetLabel);
    const Seen siblingLabel = Of(*f.siblingLabel);
    f.panelA->setStyle("color: #00aa00;");
    f.frame();
    Expect(Restyled(*f.targetLabel, label) && Is(f.targetLabel->computedStyle().color, 0, 170, 0),
           "a changed inherited color restyles the descendants that inherit it");
    Expect(!Restyled(*f.siblingLabel, siblingLabel), "and leaves the other panel alone");
}

void TestUserAgentSheetRestylesEverything() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen sibling = Of(*f.sibling);
    const jadefx::Color before = f.scene->themeColor(jadefx::ThemeColor::Background);
    f.scene->setUserAgentStylesheet(jadefx::Theme::DARK);
    f.frame();
    Expect(Restyled(*f.sibling, sibling), "a scene's user-agent stylesheet restyles every node");
    Expect(f.scene->themeColor(jadefx::ThemeColor::Background).r != before.r, "the dark theme applies");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "a user-agent change matches a full pass");
}

void TestThemeSwitchRestylesEverything() {
    const std::string previous = jadefx::Theme::getUserAgentStylesheet();
    Fixture f = MakeFixture();
    f.frame();
    const Seen sibling = Of(*f.sibling);
    jadefx::Theme::setUserAgentStylesheet(jadefx::Theme::DARK);
    f.frame();
    Expect(Restyled(*f.sibling, sibling), "switching the application theme restyles every node");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "a theme switch matches a full pass");
    jadefx::Theme::setUserAgentStylesheet(previous);
}

void TestFocusWithinFollowsFocus() {
    Fixture f = MakeFixture();
    std::shared_ptr<jadefx::Label> secondLabel;
    auto second = Item("second", secondLabel);
    f.panelA->getChildren().add(second);
    f.frame();
    const Seen panelA = Of(*f.panelA);
    const Seen panelB = Of(*f.panelB);
    f.scene->requestFocus(f.target.get());
    f.frame();
    Expect(Restyled(*f.panelA, panelA), "focusing inside a panel restyles the panel");
    Expect(Is(f.panelA->computedStyle().background.color, 0, 255, 255), "the panel matches :focus-within");
    Expect(Is(f.target->computedStyle().background.color, 255, 0, 255), "the focused item matches :focus");
    Expect(!Restyled(*f.panelB, panelB), "focus leaves the other panel alone");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "focus matches a full pass");

    const Seen panelAgain = Of(*f.panelA);
    const Seen secondSeen = Of(*second);
    f.scene->requestFocus(second.get());
    f.frame();
    Expect(Restyled(*second, secondSeen), "the newly focused item restyles");
    Expect(!Restyled(*f.panelA, panelAgain), "focus moving within a panel leaves the panel's :focus-within as it was");

    f.panelA->getChildren().removeIf([&](const std::shared_ptr<jadefx::Node>& n) { return n == second; });
    f.frame();
    Expect(!f.panelA->isFocusWithin(), "removing the focused node clears :focus-within above it");
    Expect(!Is(f.panelA->computedStyle().background.color, 0, 255, 255), "and the panel's :focus-within style goes");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "removing the focused node matches a full pass");
}

void TestFocusWithinWithoutWindowFocus() {
    Fixture f = MakeFixture();
    f.scene->requestFocus(f.target.get());
    f.frame();
    f.scene->noteWindowFocus(false);
    f.frame();
    Expect(!f.panelA->isFocusWithin(), "a window without the system focus has nothing focused within");
    f.scene->noteWindowFocus(true);
    f.frame();
    Expect(f.panelA->isFocusWithin(), "the focus returns with the window");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "window focus matches a full pass");
}

void TestFocusWithinReachesDescendants() {
    Fixture f = MakeFixture();
    std::shared_ptr<jadefx::Label> otherLabel;
    auto other = Item("other", otherLabel);
    f.panelA->getChildren().add(other);
    f.panelA->setStylesheet(".panel:focus-within .inner { color: #00aa00; }");
    f.frame();
    f.scene->requestFocus(f.target.get());
    f.frame();
    Expect(Is(otherLabel->computedStyle().color, 0, 170, 0),
           "a label under a panel with the focus within matches .panel:focus-within .inner");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "a descendant :focus-within matches a full pass");
}

void TestPaintOnlyChangeKeepsLayout() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen target = Of(*f.target);
    const Seen root = Of(*f.root);
    f.target->getClassList().add("on");
    f.frame();
    Expect(Restyled(*f.target, target), "a color class restyles");
    Expect(!LaidOut(*f.target, target) && !LaidOut(*f.root, root), "a color change lays nothing out");
}

void TestPaddingChangeLaysOutUpward() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen target = Of(*f.target);
    const Seen panelA = Of(*f.panelA);
    const Seen root = Of(*f.root);
    const Seen panelB = Of(*f.panelB);
    f.target->getClassList().add("wide");
    f.frame();
    Expect(LaidOut(*f.target, target) && LaidOut(*f.panelA, panelA) && LaidOut(*f.root, root),
           "a padding change lays out the node and its ancestors");
    Expect(!LaidOut(*f.panelB, panelB), "a padding change leaves the other panel's layout alone");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "a padding change matches a full pass");
}

void TestResizeLaysOut() {
    Fixture f = MakeFixture();
    // The root fills the scene's width, so a wider scene resizes it.
    f.root->setPrefWidthRatio(1);
    f.frame();
    const Seen root = Of(*f.root);
    f.scene->layout(400, 200, f.time + 0.1);
    Expect(LaidOut(*f.root, root), "resizing the scene lays out the root");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "a resize matches a full pass");
}

void TestAlignmentReachesDescendants() {
    Fixture f = MakeFixture();
    f.frame();
    f.root->setAlignment(jadefx::Pos::BottomRight);
    f.frame();
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(),
           "an ancestor's alignment change places descendants that inherit it");
}

void TestVisibilityLaysOut() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen panelA = Of(*f.panelA);
    f.target->setVisible(false);
    f.frame();
    Expect(LaidOut(*f.panelA, panelA), "hiding a node lays out its parent");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "hiding matches a full pass");
}

void TestPopupFollowsItsContent() {
    Fixture f = MakeFixture();
    auto popup = jadefx::make<jadefx::StackPane>();
    popup->getClassList().add("item");
    auto text = jadefx::make<jadefx::Label>("popup");
    popup->getChildren().add(text);
    f.scene->showPopup(popup, 10, 10, -1, -1);
    f.frame();
    const double before = popup->getWidth();
    text->setText("a popup with much longer text in it");
    f.frame();
    Expect(popup->getWidth() > before + 20, "a measured popup grows with its content");
    f.scene->movePopup(popup.get(), 30, 40, -1, -1);
    f.frame();
    Expect(popup->getX() == 30 && popup->getY() == 40, "a moved popup is placed where it was moved");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "popups match a full pass");
}

void TestOnlyTheAnimatingNodeRestyles() {
    Fixture f = MakeFixture();
    f.target->getClassList().add("fade");
    f.frame();
    const Seen sibling = Of(*f.sibling);
    const Seen panelA = Of(*f.panelA);
    f.target->getClassList().add("on");
    f.frame();
    std::uint32_t restyles = f.target->debugRestyleCount();
    bool everyFrame = true;
    for (int i = 0; i < 10; ++i) {
        f.frame();
        everyFrame = everyFrame && f.target->debugRestyleCount() == restyles + 1;
        restyles = f.target->debugRestyleCount();
    }
    Expect(everyFrame, "a node in a transition restyles every frame");
    const float mid = f.target->computedStyle().background.color.r;
    Expect(mid > 0.05f && mid < 0.95f, "the background is part way through its transition");
    Expect(!Restyled(*f.sibling, sibling) && !Restyled(*f.panelA, panelA), "only the animating node restyles");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "a transition matches a full pass");
    for (int i = 0; i < 40; ++i) {
        f.frame();
    }
    Expect(Is(f.target->computedStyle().background.color, 255, 255, 255), "the transition ends on its target");
    const Seen settled = Of(*f.target);
    f.frame();
    Expect(!Restyled(*f.target, settled), "a finished transition stops restyling");
}

void TestLabelTextLaysOut() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen target = Of(*f.target);
    f.targetLabel->setText("a much longer label than before");
    f.frame();
    Expect(LaidOut(*f.target, target), "new label text lays out the label's parent");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "label text matches a full pass");
}

void TestIndeterminateBarKeepsMoving() {
    auto root = jadefx::make<jadefx::VBox>();
    auto bar = jadefx::make<jadefx::ProgressBar>(-1.0);
    bar->setPrefSize(200, 12);
    root->getChildren().add(bar);
    auto scene = jadefx::make<jadefx::Scene>(root, 300, 100);
    scene->setIncrementalUpdates(true);
    scene->layout(300, 100, 0);
    const std::uint32_t before = bar->debugLayoutCount();
    for (int i = 1; i <= 5; ++i) {
        scene->layout(300, 100, i / 60.0);
    }
    Expect(bar->debugLayoutCount() >= before + 5, "an indeterminate bar lays out every frame with nothing else changing");
    Expect(jadefx::IncrementalCheck::verify(*scene).empty(), "an indeterminate bar matches a full pass");
}

void TestListViewFollowsItemChanges() {
    auto list = jadefx::make<jadefx::ListView<std::string>>();
    list->getItems().add("one");
    list->getItems().add("two");
    list->setPrefSize(200, 120);
    auto scene = jadefx::make<jadefx::Scene>(list, 200, 120);
    scene->setIncrementalUpdates(true);
    scene->layout(200, 120, 0);
    list->getItems().set(0, "a much longer first item");
    list->getItems().add("three");
    scene->layout(200, 120, 0.1);
    Expect(jadefx::IncrementalCheck::verify(*scene).empty(), "a list view follows its items");
    list->getSelectionModel().select(1);
    scene->layout(200, 120, 0.2);
    Expect(jadefx::IncrementalCheck::verify(*scene).empty(), "a list view follows its selection");
}

}  // namespace

int RunIncrementalTests() {
    TestIdleFrameDoesNothing();
    TestNewNodesStyleAndLayout();
    TestChildAddTouchesOnlyItsParent();
    TestChildRemoveTouchesOnlyItsParent();
    TestFullPassWhenIncrementalOff();
    TestVerifyAgreesOnCleanScene();
    TestCompareNamesPathAndField();
    TestStyleSources();
    TestStyleSourceResults();
    TestHoverRestylesDescendants();
    TestInheritedChangePropagates();
    TestUserAgentSheetRestylesEverything();
    TestThemeSwitchRestylesEverything();
    TestFocusWithinFollowsFocus();
    TestFocusWithinWithoutWindowFocus();
    TestFocusWithinReachesDescendants();
    TestPaintOnlyChangeKeepsLayout();
    TestPaddingChangeLaysOutUpward();
    TestResizeLaysOut();
    TestAlignmentReachesDescendants();
    TestVisibilityLaysOut();
    TestPopupFollowsItsContent();
    TestOnlyTheAnimatingNodeRestyles();
    TestLabelTextLaysOut();
    TestIndeterminateBarKeepsMoving();
    TestListViewFollowsItemChanges();
    return gFailures;
}

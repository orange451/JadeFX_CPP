#include "jadefx/jadefx.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

// The full style and layout pass, made cheaper without changing what it computes.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

using jadefx::LayoutPass;
using PassNote = std::pair<LayoutPass, bool>;

void TestScenePassesReported() {
    auto scene = jadefx::make<jadefx::Scene>(jadefx::make<jadefx::VBox>(), 200, 100);
    std::vector<PassNote> notes;
    scene->setLayoutPassHook([&](LayoutPass pass, bool begin) { notes.emplace_back(pass, begin); });
    scene->layout(200, 100, 0);
    const std::vector<PassNote> expected{{LayoutPass::Styles, true}, {LayoutPass::Styles, false},
                                         {LayoutPass::Layout, true}, {LayoutPass::Layout, false},
                                         {LayoutPass::Popups, true}, {LayoutPass::Popups, false}};
    Expect(notes == expected, "a scene layout reports styles, layout, and popups, each begun then ended");
}

void TestStageForwardsPasses() {
    jadefx::Stage stage;
    std::vector<PassNote> notes;
    stage.setLayoutPassHook([&](LayoutPass pass, bool begin) { notes.emplace_back(pass, begin); });
    stage.noteLayoutPass(LayoutPass::Popups, true);
    Expect(notes.size() == 1 && notes[0] == PassNote{LayoutPass::Popups, true}, "the stage hook hears a noted pass");

    auto scene = jadefx::make<jadefx::Scene>(jadefx::make<jadefx::Pane>(), 100, 100);
    stage.setScene(scene);
    notes.clear();
    scene->layout(100, 100, 0);
    Expect(notes.size() == 6, "a scene on the stage reports its passes to the stage hook");

    stage.setScene(jadefx::make<jadefx::Scene>(jadefx::make<jadefx::Pane>(), 100, 100));
    notes.clear();
    scene->layout(100, 100, 0);
    Expect(notes.empty(), "a scene the stage replaced stops reporting");
}

// A computed color against 0-255 channels.
bool Is(const jadefx::Color& color, int r, int g, int b) {
    auto close = [](float channel, int value) { return std::fabs(channel * 255.f - static_cast<float>(value)) < 1.5f; };
    return close(color.r, r) && close(color.g, g) && close(color.b, b);
}

void TestIndexedSelectorsMatch() {
    auto root = jadefx::make<jadefx::VBox>();
    auto byId = jadefx::make<jadefx::StackPane>();
    byId->setElementId("only");
    auto twoClasses = jadefx::make<jadefx::StackPane>();
    twoClasses->getClassList().add("b");
    twoClasses->getClassList().add("a");
    auto typed = jadefx::make<jadefx::Label>("typed");
    typed->getClassList().add("primary");
    auto outer = jadefx::make<jadefx::StackPane>();
    outer->getClassList().add("outer");
    auto inner = jadefx::make<jadefx::StackPane>();
    inner->getClassList().add("inner");
    outer->getChildren().add(inner);
    auto plain = jadefx::make<jadefx::StackPane>();
    for (const std::shared_ptr<jadefx::Node>& node :
         std::vector<std::shared_ptr<jadefx::Node>>{byId, twoClasses, typed, outer, plain}) {
        root->getChildren().add(node);
    }
    auto scene = jadefx::make<jadefx::Scene>(root, 200, 300);
    scene->setStylesheet("* { border-color: #010203; }"
                         "#only { background-color: #ff0000; }"
                         ".a.b { background-color: #00ff00; }"
                         "label.primary { color: #0000ff; }"
                         "label { color: #123456; }"
                         ".outer .inner { background-color: #abcdef; }"
                         "stackpane, .never { background-color: #fedcba; }"
                         ".never, #only { color: #102030; }");
    scene->layout(200, 300, 0);
    Expect(Is(byId->computedStyle().background.color, 255, 0, 0), "an id rule beats a type rule");
    Expect(Is(byId->computedStyle().color, 16, 32, 48), "a rule matched through its second selector applies");
    Expect(Is(twoClasses->computedStyle().background.color, 0, 255, 0),
           "a two-class selector matches classes listed in either order");
    Expect(Is(typed->computedStyle().color, 0, 0, 255), "a type-and-class selector beats a bare type selector");
    Expect(Is(inner->computedStyle().background.color, 171, 205, 239), "a descendant selector matches");
    Expect(Is(plain->computedStyle().background.color, 254, 220, 186), "a type selector in a selector list matches");
    Expect(Is(plain->computedStyle().borderColor, 1, 2, 3), "the universal selector matches every node");
}

void TestPropertyIds() {
    Expect(jadefx::propertyIdOf("background-color") == jadefx::PropertyId::BackgroundColor, "background-color has its id");
    Expect(jadefx::propertyIdOf("--panel-bg") == jadefx::PropertyId::Custom, "a custom property is custom");
    Expect(jadefx::propertyIdOf("accent-color") == jadefx::PropertyId::Custom, "accent-color is stored as a custom property");
    Expect(jadefx::propertyIdOf("all") == jadefx::PropertyId::All, "all has its id, for transitions");
    Expect(jadefx::propertyIdOf("not-a-property") == jadefx::PropertyId::Unknown, "an unknown property is unknown");
}

void TestDeclarationsParsedOnce() {
    using Kind = jadefx::DeclarationValue::Kind;
    const std::vector<jadefx::Declaration> parsed = jadefx::parseInlineDeclarations(
        "color: #ff0000; width: 50%; padding: 1em 2px; border-color: var(--x); background-color: nonsense");
    Expect(parsed.size() == 5, "five declarations parse");
    if (parsed.size() != 5) {
        return;
    }
    Expect(parsed[0].parsed.kind == Kind::Color && Is(parsed[0].parsed.color, 255, 0, 0), "a color is parsed at load");
    Expect(parsed[1].parsed.kind == Kind::Size && parsed[1].parsed.size.kind == jadefx::SizeKind::Percent &&
               parsed[1].parsed.size.percent == 0.5,
           "a length is parsed at load");
    Expect(parsed[2].parsed.kind == Kind::Lengths && parsed[2].parsed.lengthCount == 2 &&
               parsed[2].parsed.lengths[0].em == 1 && parsed[2].parsed.lengths[1].pixels == 2,
           "a length list keeps em for the node's font size");
    Expect(parsed[3].hasVar && parsed[3].parsed.kind == Kind::Raw, "a value with var() stays raw");
    Expect(parsed[4].parsed.kind == Kind::Invalid, "a value that does not parse is marked invalid");
}

void TestVarResolvedPerNode() {
    auto root = jadefx::make<jadefx::VBox>();
    auto first = jadefx::make<jadefx::StackPane>();
    auto second = jadefx::make<jadefx::StackPane>();
    first->getClassList().add("t");
    second->getClassList().add("t");
    first->setStyle("--tone: #ff0000;");
    second->setStyle("--tone: #00ff00;");
    root->getChildren().add(first);
    root->getChildren().add(second);
    auto scene = jadefx::make<jadefx::Scene>(root, 200, 200);
    scene->setStylesheet(".t { font-size: 20px; padding: 1em; background-color: var(--tone); }"
                         ".t { background-color: nonsense; }");
    scene->layout(200, 200, 0);
    Expect(Is(first->computedStyle().background.color, 255, 0, 0), "var() resolves against the first node's value");
    Expect(Is(second->computedStyle().background.color, 0, 255, 0), "var() resolves against the second node's value");
    Expect(first->computedStyle().padding.left == 20, "an em length uses the node's own font size");
}

void TestTransitionTimingsByProperty() {
    auto root = jadefx::make<jadefx::VBox>();
    auto box = jadefx::make<jadefx::StackPane>();
    box->getClassList().add("t");
    auto edged = jadefx::make<jadefx::StackPane>();
    edged->getClassList().add("u");
    root->getChildren().add(box);
    root->getChildren().add(edged);
    auto scene = jadefx::make<jadefx::Scene>(root, 200, 200);
    scene->setStylesheet(".t { background-color: #000000; color: #000000; transition: background-color 1s, color 0s; }"
                         ".t.on { background-color: #ffffff; color: #ffffff; }"
                         ".u { border-color: #000000; transition: all 1s; }"
                         ".u.on { border-color: #ffffff; }");
    scene->layout(200, 200, 0);
    box->getClassList().add("on");
    edged->getClassList().add("on");
    scene->layout(200, 200, 0);
    scene->layout(200, 200, 0.5);
    const float background = box->computedStyle().background.color.r;
    Expect(background > 0.35f && background < 0.65f, "a timed property is halfway through its transition");
    Expect(Is(box->computedStyle().color, 255, 255, 255), "a property with a zero-second transition jumps");
    const float border = edged->computedStyle().borderColor.r;
    Expect(border > 0.35f && border < 0.65f, "all covers a property it does not name");
}

void TestDeepTreeInherits() {
    auto top = jadefx::make<jadefx::StackPane>();
    top->setStyle("color: #405060;");
    std::shared_ptr<jadefx::StackPane> parent = top;
    for (int i = 0; i < 40; ++i) {
        auto child = jadefx::make<jadefx::StackPane>();
        parent->getChildren().add(child);
        parent = child;
    }
    auto leaf = jadefx::make<jadefx::Label>("leaf");
    parent->getChildren().add(leaf);
    auto scene = jadefx::make<jadefx::Scene>(top, 200, 200);
    scene->layout(200, 200, 0);
    scene->layout(200, 200, 0.1);
    Expect(Is(leaf->computedStyle().color, 64, 80, 96), "a color inherits through forty levels");
}

void TestMeasureFollowsTextBetweenFrames() {
    auto root = jadefx::make<jadefx::HBox>();
    auto label = jadefx::make<jadefx::Label>("short");
    root->getChildren().add(label);
    auto scene = jadefx::make<jadefx::Scene>(root, 600, 100);
    scene->layout(600, 100, 0);
    const double before = label->getWidth();
    label->setText("a much longer piece of text than before");
    scene->layout(600, 100, 0.1);
    Expect(label->getWidth() > before + 20, "a label laid out again after new text is wider");
}

void TestMeasureOutsideLayoutIsFresh() {
    auto root = jadefx::make<jadefx::HBox>();
    auto label = jadefx::make<jadefx::Label>("short");
    root->getChildren().add(label);
    auto scene = jadefx::make<jadefx::Scene>(root, 600, 100);
    scene->layout(600, 100, 0);
    const double before = label->measuredWidth(600);
    label->setText("a much longer piece of text than before");
    Expect(label->measuredWidth(600) > before + 20, "a measure between frames sees the new text");
}

void TestThemeColorFollowsStylesheet() {
    auto box = jadefx::make<jadefx::StackPane>();
    box->getClassList().add("x");
    auto scene = jadefx::make<jadefx::Scene>(box, 100, 100);
    scene->setStylesheet(".x { --accent-color: #ff0000; }");
    scene->layout(100, 100, 0);
    Expect(Is(box->themeColor(jadefx::ThemeColor::Accent), 255, 0, 0), "a theme color reads the custom property");
    Expect(Is(box->themeColor(jadefx::ThemeColor::Accent), 255, 0, 0), "and reads it again the same");
    scene->setStylesheet(".x { --accent-color: #00ff00; }");
    scene->layout(100, 100, 0.1);
    Expect(Is(box->themeColor(jadefx::ThemeColor::Accent), 0, 255, 0), "a changed custom property changes the color");
    scene->setStylesheet(".x { --accent-color: currentColor; color: #0000ff; }");
    scene->layout(100, 100, 0.2);
    Expect(Is(box->themeColor(jadefx::ThemeColor::Accent), 0, 0, 255), "currentColor is the node's text color");
}

// A page whose height is known only once its rows are placed, like a panel that
// lays itself out to learn its height.
class GrowingPage : public jadefx::Pane {
public:
    double rows = 1;

protected:
    void layoutChildren() override { rows = 10; }
    double preferredContentHeight(double) const override { return rows * 20; }
};

// Lays its page out, then measures it again, as a scroll pane does when the
// content changed height.
class TwiceLayer : public jadefx::Pane {
public:
    std::shared_ptr<GrowingPage> page = jadefx::make<GrowingPage>();
    double second = 0;
    TwiceLayer() { getChildren().add(page); }

protected:
    void layoutChildren() override {
        page->performLayout(0, 0, getWidth(), page->measuredHeight(getWidth(), -1));
        second = page->measuredHeight(getWidth(), -1);
        page->performLayout(0, 0, getWidth(), second);
    }
};

void TestMeasureAfterOwnLayoutIsFresh() {
    auto layer = jadefx::make<TwiceLayer>();
    auto scene = jadefx::make<jadefx::Scene>(layer, 200, 400);
    scene->layout(200, 400, 0);
    Expect(layer->second == 200, "a measure after a node's own layout sees the height that layout found");
}

}  // namespace

int RunStylePerfTests() {
    TestScenePassesReported();
    TestStageForwardsPasses();
    TestIndexedSelectorsMatch();
    TestPropertyIds();
    TestDeclarationsParsedOnce();
    TestVarResolvedPerNode();
    TestTransitionTimingsByProperty();
    TestDeepTreeInherits();
    TestMeasureFollowsTextBetweenFrames();
    TestMeasureOutsideLayoutIsFresh();
    TestThemeColorFollowsStylesheet();
    TestMeasureAfterOwnLayoutIsFresh();
    return gFailures;
}

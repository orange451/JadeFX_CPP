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

}  // namespace

int RunStylePerfTests() {
    TestScenePassesReported();
    TestStageForwardsPasses();
    TestIndexedSelectorsMatch();
    return gFailures;
}

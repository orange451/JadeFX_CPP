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

}  // namespace

int RunStylePerfTests() {
    TestScenePassesReported();
    TestStageForwardsPasses();
    return gFailures;
}

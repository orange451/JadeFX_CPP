#include "jadefx/jadefx.hpp"

#include "platform/GlfwHost.hpp"

#include <cstdio>
#include <utility>
#include <vector>

// The frame phase hook: what a host that times the window's frame is told.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

using jadefx::FramePhase;
using Note = std::pair<FramePhase, bool>;

void TestHookHearsEachNote() {
    jadefx::Stage stage;
    // No hook: a note goes nowhere.
    stage.notePhase(FramePhase::Layout, true);
    std::vector<Note> notes;
    stage.setFramePhaseHook([&](FramePhase phase, bool begin) { notes.emplace_back(phase, begin); });
    stage.notePhase(FramePhase::Wait, true);
    stage.notePhase(FramePhase::Wait, false);
    Expect(notes == std::vector<Note>{{FramePhase::Wait, true}, {FramePhase::Wait, false}},
           "the hook hears a phase begin and end");
}

void TestFrameReportsItsPhases() {
    jadefx::GlfwHost host;
    if (!host.create(240, 160, "phases")) {
        Expect(false, "phase window opens");
        return;
    }
    jadefx::Stage stage;
    if (!stage.initializeGraphics(&jadefx::GlfwHost::proc)) {
        Expect(false, "phase context");
        host.destroy();
        return;
    }
    stage.setScene(jadefx::make<jadefx::Scene>(jadefx::make<jadefx::Pane>(), 240, 160));
    std::vector<Note> notes;
    stage.setFramePhaseHook([&](FramePhase phase, bool begin) { notes.emplace_back(phase, begin); });
    int pointWidth = 0;
    int pointHeight = 0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    host.windowSize(pointWidth, pointHeight);
    host.framebufferSize(framebufferWidth, framebufferHeight);

    // Without a tail, a frame is its events, its layout, and its paint, in that order.
    Expect(stage.frame(pointWidth, pointHeight, framebufferWidth, framebufferHeight), "the first frame draws");
    const std::vector<Note> plain{{FramePhase::Events, true},  {FramePhase::Events, false}, {FramePhase::Layout, true},
                                  {FramePhase::Layout, false}, {FramePhase::Render, true},  {FramePhase::Render, false}};
    Expect(notes == plain, "a frame reports events, layout, and render, each begun then ended");

    // A tail is reported after the paint, and only when there is one.
    bool tailRan = false;
    bool renderOpenInTail = true;
    stage.setFrameTail([&] {
        tailRan = true;
        renderOpenInTail = !notes.empty() && notes.back() != Note{FramePhase::Tail, true};
    });
    notes.clear();
    Expect(stage.frame(pointWidth, pointHeight, framebufferWidth, framebufferHeight), "the second frame draws");
    std::vector<Note> tailed = plain;
    tailed.emplace_back(FramePhase::Tail, true);
    tailed.emplace_back(FramePhase::Tail, false);
    Expect(tailRan && notes == tailed, "a frame with a tail reports it last");
    Expect(!renderOpenInTail, "the tail runs inside its own phase, after the paint has ended");

    host.destroy();
}

void TestNoGraphicsNoPhases() {
    jadefx::Stage stage;
    std::vector<Note> notes;
    stage.setFramePhaseHook([&](FramePhase phase, bool begin) { notes.emplace_back(phase, begin); });
    stage.frame(240, 160, 240, 160);
    Expect(notes.empty(), "a frame that draws nothing reports no phase");
}

}  // namespace

int RunFramePhaseTests() {
    TestHookHearsEachNote();
    TestFrameReportsItsPhases();
    TestNoGraphicsNoPhases();
    return gFailures;
}

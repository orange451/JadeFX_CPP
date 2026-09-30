#include "jadefx/jadefx.hpp"

#include <cstdio>
#include <vector>

namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

// Without a host the lock is only recorded, and the motion still adds up.
void TestLockWithoutHost() {
    jadefx::Scene scene;
    Expect(!scene.isPointerLocked(), "a scene starts unlocked");
    scene.notePointerDelta(5, 5);
    double dx = -1;
    double dy = -1;
    scene.takePointerDelta(dx, dy);
    Expect(dx == 0 && dy == 0, "motion while unlocked is not kept");

    scene.setPointerLocked(true);
    Expect(scene.isPointerLocked(), "the lock is recorded");
    scene.notePointerDelta(3, -1);
    scene.notePointerDelta(2, 4);
    scene.takePointerDelta(dx, dy);
    Expect(dx == 5 && dy == 3, "motion while locked adds up");
    scene.takePointerDelta(dx, dy);
    Expect(dx == 0 && dy == 0, "a take starts the sum again");
}

// The bridge hears each change once, and a window focus loss ends the lock.
void TestBridgeAndFocus() {
    jadefx::Scene scene;
    std::vector<bool> calls;
    scene.setPointerLockBridge([&calls](bool locked) { calls.push_back(locked); });
    scene.setPointerLocked(true);
    scene.setPointerLocked(true);
    Expect(calls == std::vector<bool>{true}, "the bridge hears a lock once");
    scene.notePointerDelta(7, 0);
    scene.noteWindowFocus(false);
    Expect(!scene.isPointerLocked(), "losing the window's focus ends the lock");
    Expect(calls == std::vector<bool>{true, false}, "the bridge hears the unlock");
    double dx = -1;
    double dy = -1;
    scene.takePointerDelta(dx, dy);
    Expect(dx == 0 && dy == 0, "an unlock drops the motion not yet taken");
}

}  // namespace

int RunPointerLockTests() {
    TestLockWithoutHost();
    TestBridgeAndFocus();
    return gFailures;
}

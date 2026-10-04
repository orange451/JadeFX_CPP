#include "gl/Occluder.hpp"
#include "gl/UiRenderer.hpp"

#include <cstdio>

// The occluder UiRenderer hides UI fragments behind: its state, which a draw
// reads, kept without a GL context.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

void TestStartsInactive() {
    jadefx::UiRenderer renderer;
    Expect(!renderer.occluder().active(), "a renderer starts with no occluder");
}

void TestSetAndClear() {
    jadefx::UiRenderer renderer;
    const unsigned before = renderer.occluder().revision;
    renderer.setOccluder(7, 10, 20, 300, 200, 0.5f);
    const jadefx::Occluder& set = renderer.occluder();
    Expect(set.active(), "a texture and a rectangle make it active");
    Expect(set.texture == 7 && set.rect[0] == 10.f && set.rect[1] == 20.f && set.rect[2] == 300.f &&
               set.rect[3] == 200.f && set.depth == 0.5f,
           "it keeps what it was given");
    Expect(set.revision != before, "setting it moves the revision");
    const unsigned afterSet = set.revision;
    renderer.setOccluder(7, 10, 20, 300, 200, 0.5f);
    Expect(renderer.occluder().revision == afterSet, "setting the same values again does not");
    renderer.clearOccluder();
    Expect(!renderer.occluder().active(), "clearing it makes it inactive");
    Expect(renderer.occluder().revision != afterSet, "and moves the revision");
}

void TestEmptyIsInactive() {
    jadefx::UiRenderer renderer;
    renderer.setOccluder(0, 0, 0, 100, 100, 0.5f);
    Expect(!renderer.occluder().active(), "no texture is no occluder");
    renderer.setOccluder(3, 0, 0, 0, 100, 0.5f);
    Expect(!renderer.occluder().active(), "nor is an empty rectangle");
}

}  // namespace

int RunOccluderTests() {
    TestStartsInactive();
    TestSetAndClear();
    TestEmptyIsInactive();
    return gFailures;
}

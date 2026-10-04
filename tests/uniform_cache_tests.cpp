#include "gl/UniformCache.hpp"

#include <cstdio>

// The values UiRenderer last gave each shader uniform, so a draw that repeats
// one skips the GL call.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

void TestFirstSetAlwaysChanges() {
    jadefx::UniformCache cache(3);
    Expect(cache.changed(0, 1.f, 2.f, 3.f, 4.f), "a slot never set must be set");
    Expect(cache.changed(1, 0.f), "even to zero");
}

void TestRepeatIsSkipped() {
    jadefx::UniformCache cache(2);
    cache.changed(0, 1.f, 2.f, 3.f, 4.f);
    Expect(!cache.changed(0, 1.f, 2.f, 3.f, 4.f), "the same four values again need no call");
    Expect(cache.changed(0, 1.f, 2.f, 3.f, 5.f), "one value different needs a call");
    Expect(!cache.changed(0, 1.f, 2.f, 3.f, 5.f), "and is remembered");
    cache.changed(1, 7.f);
    Expect(!cache.changed(1, 7.f), "a single float repeats too");
    Expect(cache.changed(0, 1.f, 2.f, 3.f, 4.f), "slots are kept apart");
}

void TestResetForgets() {
    jadefx::UniformCache cache(1);
    cache.changed(0, 1.f, 1.f, 1.f, 1.f);
    cache.reset();
    Expect(cache.changed(0, 1.f, 1.f, 1.f, 1.f), "after reset, as when the program is made again, everything is set");
}

void TestOutOfRangeAlwaysSets() {
    jadefx::UniformCache cache(1);
    Expect(cache.changed(5, 1.f), "a slot past the end is never skipped");
    Expect(cache.changed(5, 1.f), "nor remembered");
}

}  // namespace

int RunUniformCacheTests() {
    gFailures = 0;
    TestFirstSetAlwaysChanges();
    TestRepeatIsSkipped();
    TestResetForgets();
    TestOutOfRangeAlwaysSets();
    return gFailures;
}

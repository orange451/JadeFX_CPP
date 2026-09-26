#include "jadefx/application/RunLater.hpp"
#include "jadefx/stage/FolderDialog.hpp"

#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

void TestOrderAndNesting() {
    jadefx::drainRunLater();
    std::vector<int> ran;
    jadefx::runLater([&] { ran.push_back(1); });
    jadefx::runLater([&] {
        ran.push_back(2);
        jadefx::runLater([&] { ran.push_back(4); });
    });
    jadefx::runLater([&] { ran.push_back(3); });
    Expect(ran.empty(), "runLater does not run the task in the call");
    jadefx::drainRunLater();
    Expect(ran == std::vector<int>({1, 2, 3}), "tasks run in the order they were queued");
    jadefx::drainRunLater();
    Expect(ran == std::vector<int>({1, 2, 3, 4}), "a task queued while draining waits for the next drain");
}

void TestOtherThread() {
    jadefx::drainRunLater();
    bool ran = false;
    std::thread worker([&] { jadefx::runLater([&] { ran = true; }); });
    worker.join();
    Expect(!ran, "a task from another thread waits for the UI thread");
    jadefx::drainRunLater();
    Expect(ran, "the UI thread runs a task queued on another thread");
}

void TestEmptyHandlerIsIgnored() {
    jadefx::runLater(nullptr);
    jadefx::showFolderDialog(jadefx::FolderDialogOptions{}, nullptr);
    jadefx::drainRunLater();
    Expect(true, "an empty task or handler is ignored");
}

}  // namespace

int RunRunLaterTests() {
    TestOrderAndNesting();
    TestOtherThread();
    TestEmptyHandlerIsIgnored();
    return gFailures;
}

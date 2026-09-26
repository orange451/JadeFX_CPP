#pragma once

#include <functional>

namespace jadefx {

// Queues task for the UI thread. Safe to call from any thread. Tasks run in
// the order they were queued, at the start of the next Stage::frame, before
// that frame's input. A task queued while the queue drains waits for the next frame.
void runLater(std::function<void()> task);

// Runs every task queued so far. Stage::frame calls this. A host that never
// calls frame can call it from its own loop.
void drainRunLater();

}  // namespace jadefx

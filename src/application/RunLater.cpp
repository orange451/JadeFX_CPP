#include "jadefx/application/RunLater.hpp"

#include <mutex>
#include <utility>
#include <vector>

namespace jadefx {
namespace {

std::mutex& QueueMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<std::function<void()>>& Queue() {
    static std::vector<std::function<void()>> queue;
    return queue;
}

}  // namespace

void runLater(std::function<void()> task) {
    if (!task) {
        return;
    }
    std::lock_guard<std::mutex> guard(QueueMutex());
    Queue().push_back(std::move(task));
}

void drainRunLater() {
    std::vector<std::function<void()>> batch;
    {
        std::lock_guard<std::mutex> guard(QueueMutex());
        batch.swap(Queue());
    }
    for (std::function<void()>& task : batch) {
        task();
    }
}

}  // namespace jadefx

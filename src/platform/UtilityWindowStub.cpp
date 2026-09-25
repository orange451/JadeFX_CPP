#include "jadefx/stage/UtilityWindow.hpp"

#include "DesktopWindows.hpp"

namespace jadefx {

void bindDesktopPrimary(GlfwHost&, Stage&) {}
void shutdownDesktopWindows() {}
void closeFlaggedDesktopWindows() {}
bool drawDesktopWindows() { return true; }

struct UtilityWindow::Host {};

UtilityWindow::~UtilityWindow() = default;

std::shared_ptr<UtilityWindow> UtilityWindow::open(std::string, int, int, double, double) { return nullptr; }

Stage& UtilityWindow::stage() {
    static Stage dummy;
    return dummy;
}

const Stage& UtilityWindow::stage() const {
    static Stage dummy;
    return dummy;
}

void UtilityWindow::setTitle(const std::string&) {}
void UtilityWindow::setCanClose(std::function<bool()>) {}
void UtilityWindow::setOnClosed(std::function<void()>) {}
bool UtilityWindow::tryClose() { return false; }
void UtilityWindow::close() {}

}  // namespace jadefx

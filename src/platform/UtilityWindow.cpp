#include "jadefx/stage/UtilityWindow.hpp"

#include "DesktopWindows.hpp"
#include "GlfwHost.hpp"
#include "jadefx/scene/layout/StackPane.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

namespace jadefx {

struct UtilityWindow::Host {
    GlfwHost glfw;
};

namespace {

struct Primary {
    GlfwHost* host = nullptr;
    Stage* stage = nullptr;
};

Primary gPrimary;
std::vector<UtilityWindow*> gWindows;

void restorePrimary() {
    if (gPrimary.host != nullptr) {
        gPrimary.host->makeCurrent();
    }
}

}  // namespace

void bindDesktopPrimary(GlfwHost& host, Stage& stage) {
    gPrimary.host = &host;
    gPrimary.stage = &stage;
}

void shutdownDesktopWindows() {
    const std::vector<UtilityWindow*> open = gWindows;
    for (UtilityWindow* window : open) {
        if (window != nullptr && window->isOpen()) {
            window->close();
        }
    }
    gWindows.clear();
    gPrimary = {};
}

void closeFlaggedDesktopWindows() {
    const std::vector<UtilityWindow*> open = gWindows;
    for (UtilityWindow* window : open) {
        if (window != nullptr && window->isOpen() && window->host_ && window->host_->glfw.shouldClose()) {
            window->close();
        }
    }
}

bool drawDesktopWindows() {
    const std::vector<UtilityWindow*> open = gWindows;
    for (UtilityWindow* window : open) {
        if (window == nullptr || !window->isOpen()) {
            continue;
        }
        window->draw();
        // A utility window's GL error stays in that window. The primary window keeps running.
        if (window->isOpen() && !window->stage_.graphicsOk()) {
            std::fprintf(stderr, "Closing a utility window after an OpenGL error.\n");
            window->close();
        }
    }
    restorePrimary();
    return true;
}

UtilityWindow::~UtilityWindow() { close(); }

GLFWwindow* UtilityWindow::native() const { return host_ ? host_->glfw.handle() : nullptr; }

std::shared_ptr<UtilityWindow> UtilityWindow::open(std::string title, int width, int height, double screenX,
                                                   double screenY) {
    if (gPrimary.host == nullptr || gPrimary.host->handle() == nullptr) {
        return nullptr;
    }
    width = std::max(width, 160);
    height = std::max(height, 120);
    auto window = std::shared_ptr<UtilityWindow>(new UtilityWindow());
    window->host_ = std::make_unique<Host>();
    if (!window->host_->glfw.openChild(width, height, title.c_str(), static_cast<int>(std::lround(screenX)),
                                        static_cast<int>(std::lround(screenY)))) {
        window->host_.reset();
        return nullptr;
    }
    window->open_ = true;
    gWindows.push_back(window.get());

    UtilityWindow* raw = window.get();
    raw->host_->glfw.bind(&raw->stage_);
    raw->host_->glfw.setCloseHook([raw]() { return raw->allowed(); });
    raw->stage_.setHostHandlers(
        [raw](int w, int h) {
            if (raw->host_) {
                raw->host_->glfw.setSize(w, h);
            }
        },
        [raw]() {
            if (raw->host_) {
                raw->host_->glfw.show();
            }
        },
        [raw](const std::string& next) {
            if (raw->host_) {
                raw->host_->glfw.setTitle(next.c_str());
            }
        });
    raw->stage_.setCursorHandler([raw](Cursor cursor) {
        if (raw->host_) {
            raw->host_->glfw.setCursor(cursor);
        }
    });
    raw->host_->glfw.setRedraw([raw]() {
        if (!raw->isOpen()) {
            return;
        }
        raw->draw();
        restorePrimary();
    });

    raw->host_->glfw.makeCurrent();
    const bool graphics = raw->stage_.initializeGraphics(&GlfwHost::proc);
    raw->host_->glfw.setSwapInterval(gPrimary.host->swapInterval());
    restorePrimary();
    if (!graphics) {
        window->close();
        return nullptr;
    }
    raw->stage_.setTitle(std::move(title));
    raw->stage_.show();
    return window;
}

Stage& UtilityWindow::stage() { return stage_; }

const Stage& UtilityWindow::stage() const { return stage_; }

void UtilityWindow::setTitle(const std::string& title) { stage_.setTitle(title); }

void UtilityWindow::setCanClose(std::function<bool()> canClose) { canClose_ = std::move(canClose); }

void UtilityWindow::setOnClosed(std::function<void()> onClosed) { onClosed_ = std::move(onClosed); }

bool UtilityWindow::allowed() const { return !canClose_ || canClose_(); }

bool UtilityWindow::tryClose() {
    if (!open_) {
        return true;
    }
    if (!allowed()) {
        if (GLFWwindow* native = this->native()) {
            glfwSetWindowShouldClose(native, GLFW_FALSE);
        }
        return false;
    }
    close();
    return true;
}

void UtilityWindow::close() {
    if (!open_ && !host_) {
        return;
    }
    // A close hook may drop the last owner. Keep the window alive until this returns.
    std::shared_ptr<UtilityWindow> keep;
    try {
        keep = shared_from_this();
    } catch (const std::bad_weak_ptr&) {
        keep.reset();
    }
    open_ = false;
    gWindows.erase(std::remove(gWindows.begin(), gWindows.end(), this), gWindows.end());
    std::function<void()> closed = std::move(onClosed_);
    canClose_ = nullptr;
    if (host_) {
        if (host_->glfw.handle() != nullptr) {
            host_->glfw.makeCurrent();
            stage_.getScene().setRoot(std::make_shared<StackPane>());
            stage_.shutdownGraphics();
        }
        host_->glfw.setRedraw(nullptr);
        host_->glfw.setCloseHook(nullptr);
        host_->glfw.destroy();
        host_.reset();
        restorePrimary();
    }
    if (closed) {
        closed();
    }
}

void UtilityWindow::draw() {
    if (!open_ || !host_ || host_->glfw.handle() == nullptr) {
        return;
    }
    if (host_->glfw.shouldClose()) {
        if (allowed()) {
            close();
        } else {
            glfwSetWindowShouldClose(host_->glfw.handle(), GLFW_FALSE);
        }
        return;
    }
    host_->glfw.makeCurrent();
    int width = 0;
    int height = 0;
    int pixelsWide = 0;
    int pixelsHigh = 0;
    host_->glfw.windowSize(width, height);
    host_->glfw.framebufferSize(pixelsWide, pixelsHigh);
    if (stage_.frame(width, height, pixelsWide, pixelsHigh)) {
        host_->glfw.swap();
    }
}

bool stageToScreen(const Stage& stage, double x, double y, double& screenX, double& screenY) {
    GLFWwindow* native = nullptr;
    if (gPrimary.stage == &stage && gPrimary.host != nullptr) {
        native = gPrimary.host->handle();
    } else {
        for (UtilityWindow* window : gWindows) {
            if (window != nullptr && window->isOpen() && &window->stage_ == &stage) {
                native = window->native();
                break;
            }
        }
    }
    if (native == nullptr) {
        return false;
    }
    int originX = 0;
    int originY = 0;
    glfwGetWindowPos(native, &originX, &originY);
    screenX = static_cast<double>(originX) + x;
    screenY = static_cast<double>(originY) + y;
    return true;
}

bool windowUnderScreen(double screenX, double screenY, Stage*& stage, double& localX, double& localY) {
    auto consider = [&](GLFWwindow* native, Stage* candidate) -> bool {
        if (native == nullptr || candidate == nullptr || glfwGetWindowAttrib(native, GLFW_VISIBLE) == GLFW_FALSE) {
            return false;
        }
        int originX = 0;
        int originY = 0;
        int width = 0;
        int height = 0;
        glfwGetWindowPos(native, &originX, &originY);
        glfwGetWindowSize(native, &width, &height);
        const double x = screenX - static_cast<double>(originX);
        const double y = screenY - static_cast<double>(originY);
        if (x < 0.0 || y < 0.0 || x >= static_cast<double>(width) || y >= static_cast<double>(height)) {
            return false;
        }
        stage = candidate;
        localX = x;
        localY = y;
        return true;
    };
    for (auto it = gWindows.rbegin(); it != gWindows.rend(); ++it) {
        UtilityWindow* window = *it;
        if (window != nullptr && window->isOpen() && consider(window->native(), &window->stage_)) {
            return true;
        }
    }
    if (gPrimary.host != nullptr && consider(gPrimary.host->handle(), gPrimary.stage)) {
        return true;
    }
    return false;
}

}  // namespace jadefx

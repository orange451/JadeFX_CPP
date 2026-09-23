#include "GlfwHost.hpp"

#include "jadefx/stage/Stage.hpp"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdio>
#include <string>

namespace jadefx {
namespace {

char gError[512] = {};

void CaptureError(int, const char* description) {
    std::snprintf(gError, sizeof gError, "%s", description != nullptr ? description : "");
}

void SetCoreHints() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#ifdef __APPLE__
    // macOS only creates a 3.2+ core context when this hint is set, and 4.1 is the newest it offers.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
#else
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#ifdef _WIN32
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#endif
}

GlfwHost* HostOf(GLFWwindow* window) {
    return static_cast<GlfwHost*>(glfwGetWindowUserPointer(window));
}

void OnMove(GLFWwindow* window, double x, double y) {
    if (GlfwHost* host = HostOf(window)) {
        if (Stage* stage = host->boundStage()) {
            stage->pushMove(x, y);
        }
    }
}

void OnButton(GLFWwindow* window, int button, int action, int) {
    if (GlfwHost* host = HostOf(window)) {
        if (Stage* stage = host->boundStage()) {
            double x = 0;
            double y = 0;
            glfwGetCursorPos(window, &x, &y);
            stage->pushButton(button, action == GLFW_PRESS, x, y);
        }
    }
}

void OnScroll(GLFWwindow* window, double dx, double dy) {
    if (GlfwHost* host = HostOf(window)) {
        if (Stage* stage = host->boundStage()) {
            double x = 0;
            double y = 0;
            glfwGetCursorPos(window, &x, &y);
            stage->pushScroll(x, y, dx, dy);
        }
    }
}

void OnKey(GLFWwindow* window, int key, int, int action, int mods) {
    if (GlfwHost* host = HostOf(window)) {
        if (Stage* stage = host->boundStage()) {
            const bool repeat = action == GLFW_REPEAT;
            stage->pushKey(key, action != GLFW_RELEASE, mods, repeat);
        }
    }
}

void OnChar(GLFWwindow* window, unsigned int codepoint) {
    GlfwHost* host = HostOf(window);
    Stage* stage = host != nullptr ? host->boundStage() : nullptr;
    if (stage != nullptr) {
        std::string text;
        if (codepoint < 0x80) {
            text.push_back(static_cast<char>(codepoint));
        } else if (codepoint < 0x800) {
            text.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            text.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint < 0x10000) {
            text.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            text.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            text.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            text.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            text.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            text.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            text.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
        stage->pushText(text);
    }
}

void OnContentChange(GLFWwindow* window, int, int) {
    if (GlfwHost* host = HostOf(window)) {
        host->performRedraw();
    }
}

void OnRefresh(GLFWwindow* window) {
    if (GlfwHost* host = HostOf(window)) {
        host->performRedraw();
    }
}

}  // namespace

bool GlfwHost::create(int width, int height, const char* title) {
    glfwSetErrorCallback(CaptureError);
    if (glfwInit() != GLFW_TRUE) {
        std::fprintf(stderr, "glfwInit failed (%s).\n", gError);
        return false;
    }
    gError[0] = '\0';
    SetCoreHints();
    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (window_ == nullptr) {
        std::fprintf(stderr, "Could not create an OpenGL window (%s).\n", gError);
        glfwTerminate();
        return false;
    }
    glfwSetWindowSizeLimits(window_, 240, 160, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    return true;
}

void GlfwHost::destroy() {
    redraw_ = nullptr;
    stage_ = nullptr;
    redrawing_ = false;
    if (window_ != nullptr) {
        glfwSetWindowUserPointer(window_, nullptr);
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

bool GlfwHost::shouldClose() const {
    return window_ == nullptr || glfwWindowShouldClose(window_) == GLFW_TRUE;
}

void GlfwHost::requestClose() {
    if (window_ != nullptr) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
}

void GlfwHost::poll() { glfwPollEvents(); }

void GlfwHost::swap() {
    if (window_ != nullptr) {
        glfwSwapBuffers(window_);
    }
}

void GlfwHost::show() {
    if (window_ != nullptr) {
        glfwShowWindow(window_);
    }
}

void GlfwHost::setSize(int width, int height) {
    if (window_ != nullptr && width > 0 && height > 0) {
        glfwSetWindowSize(window_, width, height);
    }
}

void GlfwHost::setTitle(const char* title) {
    if (window_ != nullptr && title != nullptr) {
        glfwSetWindowTitle(window_, title);
    }
}

void GlfwHost::windowSize(int& width, int& height) const {
    width = 0;
    height = 0;
    if (window_ != nullptr) {
        glfwGetWindowSize(window_, &width, &height);
    }
}

void GlfwHost::framebufferSize(int& width, int& height) const {
    width = 0;
    height = 0;
    if (window_ != nullptr) {
        glfwGetFramebufferSize(window_, &width, &height);
    }
}

void* GlfwHost::proc(const char* name) {
    return reinterpret_cast<void*>(glfwGetProcAddress(name));
}

void GlfwHost::bind(Stage* stage) {
    if (window_ == nullptr) {
        return;
    }
    stage_ = stage;
    glfwSetWindowUserPointer(window_, this);
    glfwSetCursorPosCallback(window_, OnMove);
    glfwSetMouseButtonCallback(window_, OnButton);
    glfwSetScrollCallback(window_, OnScroll);
    glfwSetKeyCallback(window_, OnKey);
    glfwSetCharCallback(window_, OnChar);
    stage->setClipboardHandlers(
        [this](const std::string& text) {
            if (window_ != nullptr) {
                glfwSetClipboardString(window_, text.c_str());
            }
        },
        [this]() {
            if (window_ == nullptr) {
                return std::string();
            }
            const char* text = glfwGetClipboardString(window_);
            return text != nullptr ? std::string(text) : std::string();
        });
    // These run while a resize drag still owns the thread, before poll() returns.
    glfwSetWindowSizeCallback(window_, OnContentChange);
    glfwSetFramebufferSizeCallback(window_, OnContentChange);
    glfwSetWindowRefreshCallback(window_, OnRefresh);
}

void GlfwHost::setRedraw(std::function<void()> redraw) { redraw_ = std::move(redraw); }

void GlfwHost::performRedraw() {
    if (redrawing_ || !redraw_ || window_ == nullptr) {
        return;
    }
    redrawing_ = true;
    // Waiting for vsync here blocks the platform resize loop, so the drag paints late.
    glfwSwapInterval(0);
    redraw_();
    if (window_ != nullptr) {
        glfwSwapInterval(1);
    }
    redrawing_ = false;
}

}  // namespace jadefx

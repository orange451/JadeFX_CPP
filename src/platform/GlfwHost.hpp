#pragma once

#include "jadefx/style/Style.hpp"

#include <functional>

struct GLFWwindow;
struct GLFWcursor;

namespace jadefx {

class Stage;

class GlfwHost {
public:
    bool create(int width, int height, const char* title);
    // A second window. create() must already have initialized GLFW.
    // destroy() closes the window and does not terminate GLFW.
    bool openChild(int width, int height, const char* title, int x, int y);
    void destroy();
    bool shouldClose() const;
    void requestClose();
    void poll();
    void swap();
    void show();
    void setSize(int width, int height);
    void setTitle(const char* title);
    void makeCurrent();
    GLFWwindow* handle() const { return window_; }
    // Return false to keep the window open. The primary window has no hook.
    void setCloseHook(std::function<bool()> hook);
    // True when the hook wants the window to stay. Used by the GLFW close callback.
    bool closeHookRejects() const;
    // 0 presents as soon as the frame is finished. The default waits for the display.
    void setSwapInterval(int interval);
    int swapInterval() const { return swapInterval_; }
    void windowSize(int& width, int& height) const;
    void framebufferSize(int& width, int& height) const;
    static void* proc(const char* name);
    void bind(Stage* stage);
    Stage* boundStage() const { return stage_; }
    void setCursor(Cursor cursor);

    // Cocoa and Win32 do not return from event polling while the user resizes the window.
    // The redraw runs from the callbacks those nested loops already invoke.
    void setRedraw(std::function<void()> redraw);
    void performRedraw();

private:
    GLFWwindow* window_ = nullptr;
    Stage* stage_ = nullptr;
    bool ownsLibrary_ = false;
    std::function<bool()> closeHook_;
    GLFWcursor* cursors_[static_cast<int>(CursorShape::Hidden) + 1] = {};
    std::function<void()> redraw_;
    bool redrawing_ = false;
    int swapInterval_ = 1;
};

}  // namespace jadefx

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
    // Handles events as they come, for up to seconds, and returns early after the first.
    void waitEvents(double seconds);
    bool isIconified() const;
    // Seconds on a steady clock.
    static double now();
    void swap();
    void show();
    void setSize(int width, int height);
    void setTitle(const char* title);
    void makeCurrent();
    GLFWwindow* handle() const { return window_; }
    // Return false to keep the window open. The primary window asks its Stage.
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
    // Hides the pointer and holds it where it is. Moves become Stage::pushPointerDelta,
    // presses and scrolls are reported where the lock began, and cursor shapes wait
    // for the unlock, which puts the pointer back there.
    void setPointerLocked(bool locked);
    bool pointerLocked() const { return locked_; }
    // The motion since the last move while locked. Updates the last position.
    void lockedMove(double x, double y, double& dx, double& dy);
    // Where presses land: the lock's point while locked, else the cursor.
    void pointerPosition(double& x, double& y) const;

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
    bool locked_ = false;
    double lockX_ = 0;
    double lockY_ = 0;
    double lastX_ = 0;
    double lastY_ = 0;
    // The shape asked for last, applied again at the unlock.
    Cursor cursor_ = Cursor::Default;
};

}  // namespace jadefx

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
    void destroy();
    bool shouldClose() const;
    void requestClose();
    void poll();
    void swap();
    void show();
    void setSize(int width, int height);
    void setTitle(const char* title);
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
    GLFWcursor* cursors_[static_cast<int>(CursorShape::Hidden) + 1] = {};
    std::function<void()> redraw_;
    bool redrawing_ = false;
};

}  // namespace jadefx

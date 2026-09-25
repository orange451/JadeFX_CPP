#pragma once

#include "jadefx/stage/Stage.hpp"

#include <functional>
#include <memory>
#include <string>

struct GLFWwindow;

namespace jadefx {

// A small extra window. The desktop loop draws it beside the main window.
// open() returns null when no primary window exists (tests that never open one,
// or a mobile build). tryClose() is the title-bar close: setCanClose can keep
// the window. close() always destroys it.
class UtilityWindow : public std::enable_shared_from_this<UtilityWindow> {
public:
    static std::shared_ptr<UtilityWindow> open(std::string title, int width, int height, double screenX, double screenY);
    ~UtilityWindow();

    Stage& stage();
    const Stage& stage() const;

    void setTitle(const std::string& title);
    bool isOpen() const { return open_; }

    // False keeps the window. An empty hook allows the close.
    void setCanClose(std::function<bool()> canClose);
    void setOnClosed(std::function<void()> onClosed);

    // Honors setCanClose. True when the window closed.
    bool tryClose();
    void close();

private:
    friend void closeFlaggedDesktopWindows();
    friend void shutdownDesktopWindows();
    friend bool drawDesktopWindows();
    friend bool stageToScreen(const Stage& stage, double x, double y, double& screenX, double& screenY);
    friend bool windowUnderScreen(double screenX, double screenY, Stage*& stage, double& localX, double& localY);

    struct Host;

    UtilityWindow() = default;
    bool allowed() const;
    void draw();
    GLFWwindow* native() const;

    Stage stage_;
    std::unique_ptr<Host> host_;
    std::function<bool()> canClose_;
    std::function<void()> onClosed_;
    bool open_ = false;
};

// Window point to screen pixels. False when the stage has no desktop window.
bool stageToScreen(const Stage& stage, double x, double y, double& screenX, double& screenY);

// The top-most window that contains the screen point. localX/localY are window points.
bool windowUnderScreen(double screenX, double screenY, Stage*& stage, double& localX, double& localY);

}  // namespace jadefx

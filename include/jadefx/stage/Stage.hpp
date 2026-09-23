#pragma once

#include "jadefx/scene/Scene.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace jadefx {

class UiRenderer;

// One window's scene, input, and frame. Application::launch owns the window.
// An existing OpenGL program can own a Stage and call frame() after its own clear.
class Stage {
public:
    Stage();
    ~Stage();

    Stage(const Stage&) = delete;
    Stage& operator=(const Stage&) = delete;

    Scene& getScene();
    const Scene& getScene() const;
    void setScene(std::shared_ptr<Scene> scene);

    int getWidth() const { return pointWidth_; }
    int getHeight() const { return pointHeight_; }
    void setTitle(const std::string& title);
    void setSize(int width, int height);
    void show();

    void setOnKey(std::function<void(int key, bool pressed)> handler) { onKey_ = std::move(handler); }
    void setOnText(std::function<void(const std::string& text)> handler) { onText_ = std::move(handler); }
    void setOnScroll(ScrollHandler handler) { onScroll_ = std::move(handler); }

    // Runs after the UI, with the framebuffer still bound. Width and height are pixels.
    void setRenderingCallback(std::function<void(int framebufferWidth, int framebufferHeight)> callback) {
        afterUi_ = std::move(callback);
    }

    bool initializeGraphics(void* (*proc)(const char*));
    void shutdownGraphics();

    void pushMove(double x, double y);
    void pushButton(int button, bool down, double x, double y);
    void pushScroll(double x, double y, double deltaX, double deltaY);
    void pushKey(int key, bool pressed);
    void pushKey(int key, bool pressed, int mods, bool repeat);
    void pushText(std::string text);

    void setClipboardText(const std::string& text);
    std::string clipboardText() const;
    void setClipboardHandlers(std::function<void(const std::string&)> setText, std::function<std::string()> getText);

    void setSafeInsets(const Insets& insets) { safe_ = insets; }

    // pointWidth/pointHeight are CSS pixels. framebufferWidth/Height are the drawable.
    // Returns false when OpenGL reported an error while drawing the frame.
    bool frame(int pointWidth, int pointHeight, int framebufferWidth, int framebufferHeight);
    bool graphicsOk() const { return graphicsOk_; }

    using ResizeHandler = std::function<void(int width, int height)>;
    using ShowHandler = std::function<void()>;
    using TitleHandler = std::function<void(const std::string&)>;
    void setHostHandlers(ResizeHandler resize, ShowHandler show, TitleHandler title);

private:
    struct Event;
    void processEvents();
    void hookClipboard();

    std::shared_ptr<Scene> scene_;
    std::unique_ptr<UiRenderer> renderer_;
    std::vector<Event> events_;
    Insets safe_;
    ResizeHandler onResize_;
    ShowHandler onShow_;
    TitleHandler onTitle_;
    std::function<void(int, bool)> onKey_;
    std::function<void(const std::string&)> onText_;
    ScrollHandler onScroll_;
    std::function<void(int, int)> afterUi_;
    std::function<void(const std::string&)> clipboardSet_;
    std::function<std::string()> clipboardGet_;
    std::string clipboard_;
    int pointWidth_ = 0;
    int pointHeight_ = 0;
    bool graphicsReady_ = false;
    bool graphicsOk_ = true;
    bool shown_ = false;
    int frames_ = 0;
    std::string pendingTitle_;
};

}  // namespace jadefx

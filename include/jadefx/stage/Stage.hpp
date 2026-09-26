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
    void pushPointerExit();
    void pushButton(int button, bool down, double x, double y, int mods = 0);
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
    // The color buffer is cleared unless setClearsColor(false) was called. An existing
    // scene that already cleared can then show through a transparent stage.
    bool frame(int pointWidth, int pointHeight, int framebufferWidth, int framebufferHeight);
    void setClearsColor(bool clear) { clearColor_ = clear; }
    bool graphicsOk() const { return graphicsOk_; }

    using ResizeHandler = std::function<void(int width, int height)>;
    using ShowHandler = std::function<void()>;
    using TitleHandler = std::function<void(const std::string&)>;
    using CursorHandler = std::function<void(Cursor)>;
    void setHostHandlers(ResizeHandler resize, ShowHandler show, TitleHandler title);

    // Asked when the user closes the window: the close button, Alt+F4, or Cmd+Q.
    // Return false to keep it open, for example to ask about unsaved work, then
    // call close() once the answer allows it. No handler lets every close through.
    void setOnCloseRequest(std::function<bool()> handler) { onCloseRequest_ = std::move(handler); }
    // True when the handler lets the window close. The host calls this.
    bool closeRequested();
    // Closes the window without asking onCloseRequest.
    void close();
    // The host's way to close the window. Application::launch sets it.
    void setCloseHandler(std::function<void()> handler) { onClose_ = std::move(handler); }
    // Called when the cursor over the window changes. The GLFW host sets the system cursor.
    void setCursorHandler(CursorHandler handler);

    // Forwarded to the current scene, and to a scene installed later with setScene.
    // The pump returns 1 after one turn, 0 when a frame is already running, and -1 to stop.
    void setEventPump(std::function<int()> pump);

    // Runs after the frame has been drawn. A resize requested here shows up next frame.
    void setFrameTail(std::function<void()> tail);

private:
    struct Event;
    void processEvents();
    void hookClipboard();
    void syncCursor();

    std::shared_ptr<Scene> scene_;
    std::unique_ptr<UiRenderer> renderer_;
    std::vector<Event> events_;
    Insets safe_;
    ResizeHandler onResize_;
    ShowHandler onShow_;
    std::function<bool()> onCloseRequest_;
    std::function<void()> onClose_;
    TitleHandler onTitle_;
    CursorHandler onCursor_;
    Cursor cursor_ = Cursor::Default;
    bool cursorApplied_ = false;
    std::function<void(int, bool)> onKey_;
    std::function<void(const std::string&)> onText_;
    ScrollHandler onScroll_;
    std::function<void(int, int)> afterUi_;
    std::function<void(const std::string&)> clipboardSet_;
    std::function<std::string()> clipboardGet_;
    std::function<int()> eventPump_;
    std::function<void()> frameTail_;
    std::string clipboard_;
    int pointWidth_ = 0;
    int pointHeight_ = 0;
    bool graphicsReady_ = false;
    bool graphicsOk_ = true;
    bool clearColor_ = true;
    bool shown_ = false;
    int frames_ = 0;
    std::string pendingTitle_;
};

}  // namespace jadefx

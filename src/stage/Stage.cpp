#include "jadefx/stage/Stage.hpp"

#include "jadefx/application/RunLater.hpp"

#include "jadefx/scene/layout/StackPane.hpp"
#include "gl/UiRenderer.hpp"
#include "gl/gl.hpp"

#include <cstdlib>
#include <cstdio>
#include <utility>

namespace jadefx {

namespace {
double gZoom = 1.0;
}

void Stage::setZoom(double zoom) { gZoom = zoom > 0.05 ? zoom : 1.0; }

double Stage::getZoom() { return gZoom; }

struct Stage::Event {
    enum class Type { Move, Button, Scroll, Key, Text, Leave, Focus, Drop, PointerDelta };
    Type type = Type::Move;
    double x = 0;
    double y = 0;
    double dx = 0;
    double dy = 0;
    int button = 0;
    int key = 0;
    int mods = 0;
    bool down = false;
    bool repeat = false;
    std::string text;
    std::vector<std::string> paths;
};

Stage::Stage() {
    auto root = std::make_shared<StackPane>();
    scene_ = std::make_shared<Scene>(root);
    hookClipboard();
}

Stage::~Stage() = default;

Scene& Stage::getScene() { return *scene_; }
const Scene& Stage::getScene() const { return *scene_; }

void Stage::hookClipboard() {
    if (!scene_) {
        return;
    }
    scene_->setClipboardBridge([this](const std::string& text) { setClipboardText(text); },
                               [this]() { return clipboardText(); });
    scene_->setPointerLockBridge([this](bool locked) {
        if (onPointerLock_) {
            onPointerLock_(locked);
        }
    });
}

void Stage::setScene(std::shared_ptr<Scene> scene) {
    if (!scene) {
        return;
    }
    // The scene going away gives back the host's pointer if it held it, and
    // is cut off so it cannot lock the pointer again from outside the window.
    if (scene_ && scene_ != scene) {
        scene_->setPointerLocked(false);
        scene_->setPointerLockBridge(nullptr);
    }
    scene_ = std::move(scene);
    if (eventPump_) {
        scene_->setEventPump(eventPump_);
    }
    hookClipboard();
    if (scene_->requestedWidth() > 1.0 && scene_->requestedHeight() > 1.0 && onResize_) {
        onResize_(static_cast<int>(scene_->requestedWidth()), static_cast<int>(scene_->requestedHeight()));
    }
}

void Stage::setTitle(const std::string& title) {
    pendingTitle_ = title;
    if (onTitle_) {
        onTitle_(title);
    }
}

void Stage::setSize(int width, int height) {
    if (onResize_) {
        onResize_(width, height);
    }
}

void Stage::show() {
    shown_ = true;
    if (onShow_) {
        onShow_();
    }
}

void Stage::setEventPump(std::function<int()> pump) {
    eventPump_ = std::move(pump);
    if (scene_ && eventPump_) {
        scene_->setEventPump(eventPump_);
    }
}

void Stage::setFrameTail(std::function<void()> tail) { frameTail_ = std::move(tail); }

void Stage::notePhase(FramePhase phase, bool begin) const {
    if (phaseHook_) {
        phaseHook_(phase, begin);
    }
}

namespace {

// Reports a phase from construction to destruction.
class PhaseScope {
public:
    PhaseScope(const Stage& stage, FramePhase phase) : stage_(stage), phase_(phase) { stage_.notePhase(phase_, true); }
    ~PhaseScope() { stage_.notePhase(phase_, false); }
    PhaseScope(const PhaseScope&) = delete;
    PhaseScope& operator=(const PhaseScope&) = delete;

private:
    const Stage& stage_;
    FramePhase phase_;
};

}  // namespace

void Stage::setHostHandlers(ResizeHandler resize, ShowHandler show, TitleHandler title) {
    onResize_ = std::move(resize);
    onShow_ = std::move(show);
    onTitle_ = std::move(title);
    if (!pendingTitle_.empty() && onTitle_) {
        onTitle_(pendingTitle_);
    }
}

void Stage::setCursorHandler(CursorHandler handler) { onCursor_ = std::move(handler); }

bool Stage::initializeGraphics(void* (*proc)(const char*)) {
    if (!jadefx_load_gl(proc)) {
        graphicsReady_ = false;
        graphicsOk_ = false;
        return false;
    }
    renderer_ = std::make_unique<UiRenderer>();
    graphicsReady_ = renderer_->initialize();
    graphicsOk_ = graphicsReady_;
    return graphicsReady_;
}

void Stage::shutdownGraphics() {
    if (renderer_) {
        renderer_->shutdown();
        renderer_.reset();
    }
    graphicsReady_ = false;
}

void Stage::pushMove(double x, double y) {
    Event event;
    event.type = Event::Type::Move;
    event.x = x;
    event.y = y;
    events_.push_back(std::move(event));
}

void Stage::pushPointerDelta(double dx, double dy) {
    Event event;
    event.type = Event::Type::PointerDelta;
    event.x = dx;
    event.y = dy;
    events_.push_back(std::move(event));
}

void Stage::pushPointerExit() {
    Event event;
    event.type = Event::Type::Leave;
    events_.push_back(std::move(event));
}

void Stage::pushButton(int button, bool down, double x, double y, int mods) {
    Event event;
    event.type = Event::Type::Button;
    event.mods = mods;
    event.button = button;
    event.down = down;
    event.x = x;
    event.y = y;
    events_.push_back(std::move(event));
}

void Stage::pushScroll(double x, double y, double deltaX, double deltaY) {
    Event event;
    event.type = Event::Type::Scroll;
    event.x = x;
    event.y = y;
    event.dx = deltaX;
    event.dy = deltaY;
    events_.push_back(std::move(event));
}

void Stage::pushKey(int key, bool pressed) { pushKey(key, pressed, 0, false); }

void Stage::pushKey(int key, bool pressed, int mods, bool repeat) {
    Event event;
    event.type = Event::Type::Key;
    event.key = key;
    event.mods = mods;
    event.down = pressed;
    event.repeat = repeat;
    events_.push_back(std::move(event));
}

void Stage::setClipboardText(const std::string& text) {
    clipboard_ = text;
    if (clipboardSet_) {
        clipboardSet_(text);
    }
}

std::string Stage::clipboardText() const {
    if (clipboardGet_) {
        const std::string live = clipboardGet_();
        if (!live.empty()) {
            return live;
        }
    }
    return clipboard_;
}

void Stage::setClipboardHandlers(std::function<void(const std::string&)> setText, std::function<std::string()> getText) {
    clipboardSet_ = std::move(setText);
    clipboardGet_ = std::move(getText);
}

bool Stage::isFocused() const { return scene_ == nullptr || scene_->isWindowFocused(); }

void Stage::pushWindowFocus(bool focused) {
    Event event;
    event.type = Event::Type::Focus;
    event.down = focused;
    events_.push_back(std::move(event));
}

void Stage::pushFileDrop(double x, double y, std::vector<std::string> paths) {
    Event event;
    event.type = Event::Type::Drop;
    event.x = x;
    event.y = y;
    event.paths = std::move(paths);
    events_.push_back(std::move(event));
}

void Stage::pushText(std::string text) {
    Event event;
    event.type = Event::Type::Text;
    event.text = std::move(text);
    events_.push_back(std::move(event));
}

void Stage::processEvents() {
    std::vector<Event> events = std::move(events_);
    events_.clear();
    for (Event& event : events) {
        event.x /= gZoom;
        event.y /= gZoom;
    }
    for (const Event& event : events) {
        switch (event.type) {
            case Event::Type::Move:
                scene_->noteMove(event.x, event.y);
                break;
            case Event::Type::Leave:
                scene_->notePointerExit();
                break;
            case Event::Type::Button:
                scene_->noteButton(event.button, event.down, event.x, event.y, event.mods);
                break;
            case Event::Type::Scroll:
                scene_->noteScroll(event.x, event.y, event.dx, event.dy);
                if (onScroll_) {
                    ScrollEvent scroll;
                    scroll.x = event.x;
                    scroll.y = event.y;
                    scroll.deltaX = event.dx;
                    scroll.deltaY = event.dy;
                    scroll.target = scene_->pick(event.x, event.y);
                    onScroll_(scroll);
                }
                break;
            case Event::Type::Key:
                if (!scene_->noteKey(event.key, event.down, event.repeat, event.mods) && onKey_) {
                    onKey_(event.key, event.down);
                }
                break;
            case Event::Type::Text:
                if (!scene_->noteText(event.text) && onText_) {
                    onText_(event.text);
                }
                break;
            case Event::Type::Focus:
                scene_->noteWindowFocus(event.down);
                break;
            case Event::Type::PointerDelta:
                scene_->notePointerDelta(event.x, event.y);
                break;
            case Event::Type::Drop:
                scene_->noteFileDrop(event.x, event.y, event.paths);
                break;
        }
    }
}

bool Stage::closeRequested() { return !onCloseRequest_ || onCloseRequest_(); }

void Stage::close() {
    if (onClose_) {
        onClose_();
    }
}

void Stage::toFront() {
    if (onToFront_) {
        onToFront_();
    }
}

bool Stage::frame(int pointWidth, int pointHeight, int framebufferWidth, int framebufferHeight) {
    if (!graphicsReady_ || renderer_ == nullptr || pointWidth <= 0 || pointHeight <= 0 || framebufferWidth <= 0 ||
        framebufferHeight <= 0) {
        return graphicsOk_;
    }
    pointWidth_ = pointWidth;
    pointHeight_ = pointHeight;
    const double zoom = gZoom;
    const double layoutWidth = static_cast<double>(pointWidth) / zoom;
    const double layoutHeight = static_cast<double>(pointHeight) / zoom;
    if (frames_ == 0) {
        scene_->setSafeInsets(safe_);
        scene_->layout(layoutWidth, layoutHeight);
    }
    {
        PhaseScope phase(*this, FramePhase::Events);
        // Before input, so a task queued by this frame's click runs after that click has finished.
        drainRunLater();
        processEvents();
    }
    {
        PhaseScope phase(*this, FramePhase::Layout);
        scene_->setSafeInsets(safe_);
        scene_->layout(layoutWidth, layoutHeight);
        syncCursor();
    }

    {
        PhaseScope phase(*this, FramePhase::Render);
        const float scale = static_cast<float>(static_cast<double>(framebufferWidth) / layoutWidth);
        renderer_->begin(framebufferWidth, framebufferHeight, scale, scene_->themeColor(ThemeColor::Background),
                         clearColor_);
        scene_->render(*renderer_, 1.f);
        if (afterUi_) {
            afterUi_(framebufferWidth, framebufferHeight);
        }
        ++frames_;
        if (const char* path = std::getenv("JADEFX_DUMP_PPM")) {
            if (frames_ == 2) {
                renderer_->writePpm(path);
            }
        }
        renderer_->end();
    }
    if (frameTail_) {
        PhaseScope phase(*this, FramePhase::Tail);
        frameTail_();
    }

    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::fprintf(stderr, "OpenGL error during UI frame: 0x%x\n", error);
        graphicsOk_ = false;
        return false;
    }
    return true;
}

void Stage::syncCursor() {
    if (!scene_) {
        return;
    }
    const Cursor next = scene_->hoverCursor();
    if (cursorApplied_ && next == cursor_) {
        return;
    }
    cursor_ = next;
    cursorApplied_ = true;
    if (onCursor_) {
        onCursor_(next);
    }
}

}  // namespace jadefx

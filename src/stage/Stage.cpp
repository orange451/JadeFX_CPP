#include "jadefx/stage/Stage.hpp"

#include "jadefx/scene/layout/StackPane.hpp"
#include "gl/UiRenderer.hpp"
#include "gl/gl.hpp"

#include <cstdlib>
#include <cstdio>
#include <utility>

namespace jadefx {

struct Stage::Event {
    enum class Type { Move, Button, Scroll, Key, Text, Leave };
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
}

void Stage::setScene(std::shared_ptr<Scene> scene) {
    if (!scene) {
        return;
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
    if (!load_gl(proc)) {
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

void Stage::pushPointerExit() {
    Event event;
    event.type = Event::Type::Leave;
    events_.push_back(std::move(event));
}

void Stage::pushButton(int button, bool down, double x, double y) {
    Event event;
    event.type = Event::Type::Button;
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

void Stage::pushText(std::string text) {
    Event event;
    event.type = Event::Type::Text;
    event.text = std::move(text);
    events_.push_back(std::move(event));
}

void Stage::processEvents() {
    const std::vector<Event> events = std::move(events_);
    events_.clear();
    for (const Event& event : events) {
        switch (event.type) {
            case Event::Type::Move:
                scene_->noteMove(event.x, event.y);
                break;
            case Event::Type::Leave:
                scene_->notePointerExit();
                break;
            case Event::Type::Button:
                scene_->noteButton(event.button, event.down, event.x, event.y);
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
        }
    }
}

bool Stage::frame(int pointWidth, int pointHeight, int framebufferWidth, int framebufferHeight) {
    if (!graphicsReady_ || renderer_ == nullptr || pointWidth <= 0 || pointHeight <= 0 || framebufferWidth <= 0 ||
        framebufferHeight <= 0) {
        return graphicsOk_;
    }
    pointWidth_ = pointWidth;
    pointHeight_ = pointHeight;
    if (frames_ == 0) {
        scene_->setSafeInsets(safe_);
        scene_->layout(pointWidth, pointHeight);
    }
    processEvents();
    scene_->setSafeInsets(safe_);
    scene_->layout(pointWidth, pointHeight);
    syncCursor();

    const float scale = static_cast<float>(framebufferWidth) / static_cast<float>(pointWidth);
    renderer_->begin(framebufferWidth, framebufferHeight, scale, Color::rgb8(248, 248, 248), clearColor_);
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

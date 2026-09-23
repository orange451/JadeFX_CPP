#include "jadefx/application/Application.hpp"

#include "platform/GlfwHost.hpp"

#include <cstdlib>
#include <cstdio>

namespace jadefx {
namespace {

MobileChrome gChrome;

}  // namespace

Application::~Application() = default;

void Application::preStart(Stage&, int, char**) {}

Size Application::defaultWindowSize() const { return {800, 600}; }

std::string Application::defaultTitle() const { return "JadeFX"; }

Size MobileApplication::defaultWindowSize() const { return {375, 667}; }

MobileChrome& mobileChrome() { return gChrome; }

void MobileApplication::showStatusBar() {
    gChrome.statusBar = true;
    gChrome.statusBarHidden = false;
    ++gChrome.generation;
}

void MobileApplication::hideStatusBar() {
    gChrome.statusBar = false;
    gChrome.statusBarHidden = true;
    ++gChrome.generation;
}

void MobileApplication::setOrientation(ScreenOrientation orientation) {
    gChrome.orientation = orientation;
    ++gChrome.generation;
}

void MobileApplication::setMultitouchEnabled(bool enabled) {
    gChrome.multitouch = enabled;
    ++gChrome.generation;
}

void MobileApplication::showKeyboard() {
    gChrome.keyboard = true;
    ++gChrome.generation;
}

void MobileApplication::hideKeyboard() {
    gChrome.keyboard = false;
    ++gChrome.generation;
}

int Application::launch(std::unique_ptr<Application> app, int argc, char** argv) {
#if defined(JADEFX_GLFM)
    (void)app;
    (void)argc;
    (void)argv;
    std::fprintf(stderr, "This platform starts from glfmMain.\n");
    return 1;
#else
    if (!app) {
        return 1;
    }
    const Size size = app->defaultWindowSize();
    const std::string title = app->defaultTitle();
    GlfwHost host;
    if (!host.create(static_cast<int>(size.width), static_cast<int>(size.height), title.c_str())) {
        return 1;
    }

    Stage stage;
    host.bind(&stage);
    stage.setHostHandlers([&](int width, int height) { host.setSize(width, height); }, [&]() { host.show(); },
                          [&](const std::string& next) { host.setTitle(next.c_str()); });
    stage.setCursorHandler([&](Cursor cursor) { host.setCursor(cursor); });
    if (!stage.initializeGraphics(&GlfwHost::proc)) {
        host.destroy();
        return 1;
    }

    int smokeFrames = 0;
    if (const char* smoke = std::getenv("JADEFX_SMOKE_FRAMES")) {
        smokeFrames = std::atoi(smoke);
    }
    int rendered = 0;
    bool drawing = false;
    int pumping = 0;
    auto drawFrame = [&]() -> bool {
        if (drawing) {
            return true;
        }
        drawing = true;
        int pointWidth = 0;
        int pointHeight = 0;
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        host.windowSize(pointWidth, pointHeight);
        host.framebufferSize(framebufferWidth, framebufferHeight);
        const bool ok = stage.frame(pointWidth, pointHeight, framebufferWidth, framebufferHeight);
        if (ok) {
            host.swap();
            ++rendered;
        } else {
            host.requestClose();
        }
        drawing = false;
        return ok;
    };
    // Dragging the border blocks poll() inside the OS until the gesture ends.
    host.setRedraw([&] { (void)drawFrame(); });
    stage.setEventPump([&]() -> int {
        if (drawing || pumping > 0) {
            return 0;
        }
        ++pumping;
        host.poll();
        const bool closed = host.shouldClose();
        const bool ok = !closed && drawFrame();
        --pumping;
        if (closed || !ok) {
            return -1;
        }
        return 1;
    });

    app->preStart(stage, argc, argv);
    app->start(stage, argc, argv);
    stage.show();

    while (!host.shouldClose()) {
        host.poll();
        if (host.shouldClose() || !drawFrame()) {
            break;
        }
        if (smokeFrames > 0 && rendered >= smokeFrames) {
            break;
        }
    }

    stage.shutdownGraphics();
    host.destroy();
    return stage.graphicsOk() ? 0 : 1;
#endif
}

}  // namespace jadefx

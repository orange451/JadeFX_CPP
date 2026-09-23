#include "jadefx/application/Application.hpp"

#if defined(JADEFX_GLFM)

#define GLFM_INCLUDE_NONE
#include <glfm.h>

#include <cmath>
#include <cstdio>

namespace jadefx {
namespace {

struct GlfmState {
    std::unique_ptr<Application> app;
    Stage stage;
    bool started = false;
    bool graphics = false;
    int appliedChrome = -1;
    std::string clipboard;
};

GlfmState* State(GLFMDisplay* display) { return static_cast<GlfmState*>(glfmGetUserData(display)); }

void* Proc(const char* name) { return reinterpret_cast<void*>(glfmGetProcAddress(name)); }

void ApplyChrome(GLFMDisplay* display, GlfmState* state) {
    const MobileChrome& chrome = mobileChrome();
    if (state->appliedChrome == chrome.generation) {
        return;
    }
    state->appliedChrome = chrome.generation;
    if (chrome.statusBar) {
        glfmSetDisplayChrome(display, GLFMUserInterfaceChromeNavigationAndStatusBar);
    } else if (chrome.statusBarHidden) {
        glfmSetDisplayChrome(display, GLFMUserInterfaceChromeNone);
    }
    glfmSetMultitouchEnabled(display, chrome.multitouch);
    glfmSetKeyboardVisible(display, chrome.keyboard);
    int orientations = GLFMInterfaceOrientationAll;
    if (chrome.orientation == ScreenOrientation::Portrait) {
        orientations = GLFMInterfaceOrientationPortrait | GLFMInterfaceOrientationPortraitUpsideDown;
    } else if (chrome.orientation == ScreenOrientation::Landscape) {
        orientations = GLFMInterfaceOrientationLandscape;
    }
    glfmSetSupportedInterfaceOrientation(display, static_cast<GLFMInterfaceOrientation>(orientations));
}

void OnSurfaceCreated(GLFMDisplay* display, int width, int height) {
    GlfmState* state = State(display);
    if (state == nullptr) {
        return;
    }
    const double scale = std::max(1.0, glfmGetDisplayScale(display));
    state->stage.setSize(static_cast<int>(std::lround(width / scale)), static_cast<int>(std::lround(height / scale)));
    if (!state->graphics) {
        state->graphics = state->stage.initializeGraphics(Proc);
    }
    state->stage.setClipboardHandlers(
        [state, display](const std::string& text) {
            state->clipboard = text;
            glfmSetClipboardText(display, text.c_str());
        },
        [state]() { return state->clipboard; });
    if (!state->started && state->graphics) {
        state->app->preStart(state->stage, 0, nullptr);
        state->app->start(state->stage, 0, nullptr);
        state->stage.show();
        state->started = true;
    }
    ApplyChrome(display, state);
}

void OnSurfaceDestroyed(GLFMDisplay* display) {
    if (GlfmState* state = State(display)) {
        state->stage.shutdownGraphics();
        state->graphics = false;
    }
}

bool OnTouch(GLFMDisplay* display, int touch, GLFMTouchPhase phase, double x, double y) {
    GlfmState* state = State(display);
    if (state == nullptr || touch != 0) {
        return false;
    }
    const double scale = std::max(1.0, glfmGetDisplayScale(display));
    const double px = x / scale;
    const double py = y / scale;
    if (phase == GLFMTouchPhaseBegan) {
        state->stage.pushButton(0, true, px, py);
    } else if (phase == GLFMTouchPhaseMoved || phase == GLFMTouchPhaseHover) {
        state->stage.pushMove(px, py);
    } else {
        state->stage.pushButton(0, false, px, py);
    }
    return true;
}

int MapKey(int code) {
    switch (code) {
        case 0x08:
            return Key::Backspace;
        case 0x09:
            return Key::Tab;
        case 0x0D:
            return Key::Enter;
        case 0x1B:
            return Key::Escape;
        case 0x7F:
            return Key::Delete;
        case 0x90:
            return Key::Insert;
        case 0x91:
            return Key::PageUp;
        case 0x92:
            return Key::PageDown;
        case 0x93:
            return Key::End;
        case 0x94:
            return Key::Home;
        case 0x95:
            return Key::Left;
        case 0x96:
            return Key::Up;
        case 0x97:
            return Key::Right;
        case 0x98:
            return Key::Down;
        case 0xA5:
            return Key::KpEnter;
        default:
            return code;
    }
}

bool OnKey(GLFMDisplay* display, GLFMKeyCode keyCode, GLFMKeyAction action, int modifiers) {
    if (GlfmState* state = State(display)) {
        const bool repeat = action == GLFMKeyActionRepeated;
        state->stage.pushKey(MapKey(static_cast<int>(keyCode)), action != GLFMKeyActionReleased, modifiers, repeat);
    }
    return false;
}

void OnChar(GLFMDisplay* display, const char* text, int) {
    if (text != nullptr) {
        if (GlfmState* state = State(display)) {
            state->stage.pushText(text);
        }
    }
}

void OnRender(GLFMDisplay* display) {
    GlfmState* state = State(display);
    if (state == nullptr || !state->graphics) {
        return;
    }
    ApplyChrome(display, state);
    int width = 0;
    int height = 0;
    glfmGetDisplaySize(display, &width, &height);
    const double scale = std::max(1.0, glfmGetDisplayScale(display));
    double top = 0;
    double right = 0;
    double bottom = 0;
    double left = 0;
    glfmGetDisplayChromeInsets(display, &top, &right, &bottom, &left);
    state->stage.setSafeInsets({top / scale, right / scale, bottom / scale, left / scale});
    state->stage.frame(static_cast<int>(std::lround(width / scale)), static_cast<int>(std::lround(height / scale)),
                       width, height);
    glfmSwapBuffers(display);
}

}  // namespace

void launchOnGlfm(GLFMDisplay* display, std::unique_ptr<Application> app) {
    if (display == nullptr || !app) {
        return;
    }
    auto* state = new GlfmState();
    state->app = std::move(app);
    glfmSetUserData(display, state);
    glfmSetDisplayConfig(display, GLFMRenderingAPIOpenGLES3, GLFMColorFormatRGBA8888, GLFMDepthFormatNone,
                         GLFMStencilFormatNone, GLFMMultisampleNone);
    glfmSetSurfaceCreatedFunc(display, OnSurfaceCreated);
    glfmSetSurfaceDestroyedFunc(display, OnSurfaceDestroyed);
    glfmSetRenderFunc(display, OnRender);
    glfmSetTouchFunc(display, OnTouch);
    glfmSetKeyFunc(display, OnKey);
    glfmSetCharFunc(display, OnChar);
}

}  // namespace jadefx

#else

namespace jadefx {

void launchOnGlfm(GLFMDisplay*, std::unique_ptr<Application>) {}

}  // namespace jadefx

#endif

#pragma once

#include "jadefx/geometry/Geometry.hpp"
#include "jadefx/stage/Stage.hpp"
#include "jadefx/style/Theme.hpp"

#include <memory>
#include <string>

namespace jadefx {

// Desktop entry. Subclass start(), then call launch() from main.
// Mobile builds enter through glfmMain, which calls the same start().
class Application {
public:
    virtual ~Application();

    static int launch(std::unique_ptr<Application> app, int argc, char** argv);

    // The built-in themes, in the shape of JavaFX's STYLESHEET_MODENA.
    static constexpr const char* STYLESHEET_LIGHT = Theme::LIGHT;
    static constexpr const char* STYLESHEET_DARK = Theme::DARK;
    // The look every scene starts from: STYLESHEET_LIGHT (the default),
    // STYLESHEET_DARK, or CSS text of your own. Scenes restyle at their next layout.
    static void setUserAgentStylesheet(std::string cssOrTheme) { Theme::setUserAgentStylesheet(std::move(cssOrTheme)); }
    static const std::string& getUserAgentStylesheet() { return Theme::getUserAgentStylesheet(); }

    virtual void start(Stage& stage, int argc, char** argv) = 0;

protected:
    virtual void preStart(Stage& stage, int argc, char** argv);
    virtual Size defaultWindowSize() const;
    virtual std::string defaultTitle() const;
    // How many refreshes to wait before showing the frame. 0 presents immediately.
    virtual int swapInterval() const;
};

// Same scene graph as Application. On desktop the window defaults to a phone
// size. On iOS and Android the GLFM backend applies orientation, chrome, and touch.
class MobileApplication : public Application {
public:
    static void showStatusBar();
    static void hideStatusBar();
    static void setOrientation(ScreenOrientation orientation);
    static void setMultitouchEnabled(bool enabled);
    static void showKeyboard();
    static void hideKeyboard();

protected:
    Size defaultWindowSize() const override;
};

struct MobileChrome {
    bool statusBar = false;
    bool statusBarHidden = false;
    bool multitouch = true;
    bool keyboard = false;
    ScreenOrientation orientation = ScreenOrientation::All;
    int generation = 0;
};

MobileChrome& mobileChrome();

// GLFM entry point (iOS, Android, Emscripten). Desktop apps call Application::launch.
struct GLFMDisplay;
void launchOnGlfm(GLFMDisplay* display, std::unique_ptr<Application> app);

}  // namespace jadefx

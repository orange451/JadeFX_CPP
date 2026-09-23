#include "jadefx/jadefx.hpp"

#include <cstdio>
#include <memory>

namespace {

// The same screen as JadeFX's PurpleApp. The blue gradient is the one in the
// project's screenshots. The Java sample leaves it commented out behind a
// yellow test gradient.
constexpr const char* kStylesheet = R"CSS(
scene {
    background-image: linear-gradient(180deg, #40C5FA, #7272EF);
    font-family: "Open Sans";
    font-size: 18px;
    color: white;
    alignment: center;
}
.main-layout {
    background-color: transparent;
    width: calc(100% - 48px);
    height: calc(100% - 48px);
}
.button-layout {
    width: 100%;
    spacing: 16px;
    alignment: center;
}
.innerFontColor {
    color: #707070;
}
.padding {
    padding: 16px 38px;
}
.test-button {
    box-shadow: 0 2px 3px -4px rgba(0, 0, 0, .4),
                0 4px 8px 0 rgba(0, 0, 0, .1),
                0 1px 18px 0 rgba(0, 0, 0, .04);
    border-radius: 24px;
    border-style: none;
    color: #3c4043;
    padding: 16px 48px;
    alignment: center;
    background-color: #fff;
    border-width: 4px;
    width: 100%;
    transition: background-color 0.1s, box-shadow 0.1s, border-color 0.1s, border-width 0.1s;
}
.test-button:hover {
    background-color: #F6F9FE;
    color: #174ea6;
}
.test-button:focus {
    border-style: none;
}
.test-button:active {
    border-style: solid;
    border-color: #FFFFFF50;
    box-shadow: 0px 4px 6px 0px rgba(60, 64, 67, 0.2),
                0px 8px 12px 3px rgba(60, 64, 67, 0.1);
}
.test-button2 {
    width: 100%;
    border-radius: 24px;
    border-style: none;
    padding: 16px 48px;
    border-width: 4px;
    alignment: center;
    background-color: rgba(255, 255, 255, 0.1);
    transition: background-color 0.1s;
}
.test-button2:active {
    background-color: rgba(255, 255, 255, 0.33);
}
)CSS";

class PurpleApp : public jadefx::MobileApplication {
public:
    void start(jadefx::Stage& stage, int, char**) override {
        showStatusBar();
        setOrientation(jadefx::ScreenOrientation::Portrait);
        setMultitouchEnabled(true);

        stage.getScene().setStylesheet(kStylesheet);

        auto layout = jadefx::make<jadefx::BorderPane>();
        layout->getClassList().add("main-layout");
        stage.getScene().setRoot(layout);

        auto buttons = jadefx::make<jadefx::VBox>();
        buttons->getClassList().add("button-layout");
        layout->setBottom(buttons);

        auto signUpLabel = jadefx::make<jadefx::Label>("Get started");
        signUpLabel->getClassList().add("innerFontColor");
        auto signUp = jadefx::make<jadefx::StackPane>();
        signUp->getClassList().add("test-button");
        signUp->getClassList().add("padding");
        signUp->setElementId("SignUp");
        signUp->getChildren().add(signUpLabel);
        signUp->setOnMouseClicked([](const jadefx::MouseEvent&) { std::printf("Clicked test button!\n"); });
        buttons->getChildren().add(signUp);

        auto signInLabel = jadefx::make<jadefx::Label>("Sign in");
        auto signIn = jadefx::make<jadefx::StackPane>();
        signIn->getClassList().add("test-button2");
        signIn->getClassList().add("padding");
        signIn->setElementId("SignIn");
        signIn->getChildren().add(signInLabel);
        signIn->setOnMouseClicked([](const jadefx::MouseEvent&) { std::printf("Clicked sign in!\n"); });
        buttons->getChildren().add(signIn);

        layout->setCenter(jadefx::make<jadefx::Label>("Hello World"));
    }
};

}  // namespace

std::unique_ptr<jadefx::Application> createApplication() {
    return std::make_unique<PurpleApp>();
}

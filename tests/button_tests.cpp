#include "jadefx/jadefx.hpp"

#include <cstdio>
#include <string>

namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

void Click(jadefx::Scene& scene, double x, double y) {
    scene.noteButton(0, true, x, y);
    scene.noteButton(0, false, x, y);
}

void TestButtonAction() {
    int clicks = 0;
    auto button = jadefx::make<jadefx::Button>("Save");
    button->setOnAction([&](jadefx::ActionEvent&) { ++clicks; });
    auto scene = jadefx::make<jadefx::Scene>(button, 200, 80);
    scene->layout(200, 80, 0);
    const double x = button->getAbsoluteX() + button->getWidth() * 0.5;
    const double y = button->getAbsoluteY() + button->getHeight() * 0.5;
    Click(*scene, x, y);
    Expect(clicks == 1, "a click inside a button fires its action");
    Expect(button->isFocused(), "clicking a button focuses it");

    scene->noteButton(0, true, x, y);
    scene->noteButton(0, false, 2, 2);
    Expect(clicks == 1, "releasing outside the button does not fire");

    scene->noteKey(jadefx::Key::Enter, true, false, 0);
    Expect(clicks == 2, "Enter fires the focused button");
    scene->noteKey(jadefx::Key::Space, true, true, 0);
    Expect(clicks == 2, "a repeated Space does not fire again");
    scene->noteKey(jadefx::Key::Space, true, false, 0);
    Expect(clicks == 3, "Space fires the focused button");

    button->setDisable(true);
    scene->layout(200, 80, 0);
    Click(*scene, x, y);
    Expect(clicks == 3, "a disabled button ignores clicks");
    Expect(!button->isFocused(), "a disabled button does not take focus");
    const jadefx::Color faded = button->computedStyle().background.color;
    scene->setStylesheet("button:disabled { background-color: #ff0000; }");
    scene->layout(200, 80, 0);
    const jadefx::Color disabled = button->computedStyle().background.color;
    Expect(disabled.r > 0.8f && disabled.g < 0.2f && disabled.b < 0.2f && disabled.g < faded.g,
           ":disabled matches a disabled button");
    button->fire();
    Expect(clicks == 4, "fire() still runs when the button is disabled");
}

void TestPopup() {
    auto root = jadefx::make<jadefx::Pane>();
    auto anchor = jadefx::make<jadefx::Button>("Menu");
    anchor->setPrefSize(80, 28);
    root->getChildren().add(anchor);
    auto scene = jadefx::make<jadefx::Scene>(root, 240, 160);
    scene->layout(240, 160, 0);

    auto item = jadefx::make<jadefx::Button>("Item");
    int picks = 0;
    item->setOnAction([&](jadefx::ActionEvent&) { ++picks; });
    auto sheet = jadefx::make<jadefx::Pane>();
    sheet->getChildren().add(item);
    sheet->setPrefSize(100, 40);
    sheet->setBackground(jadefx::Color::white());

    jadefx::PopupOptions options;
    options.owner = anchor.get();
    scene->showPopupNear(sheet, anchor.get(), jadefx::Side::Bottom, options);
    Expect(scene->isPopupShowing(sheet.get()), "showPopupNear keeps the popup");
    Expect(sheet->getAbsoluteY() + 1 >= anchor->getAbsoluteY() + anchor->getHeight(),
           "the popup sits under the anchor");

    Click(*scene, anchor->getAbsoluteX() + 4, anchor->getAbsoluteY() + 4);
    Expect(scene->isPopupShowing(sheet.get()), "a press on the owner leaves the popup open");

    Click(*scene, sheet->getAbsoluteX() + 8, sheet->getAbsoluteY() + 8);
    Expect(picks == 1, "a button inside the popup receives the click");
    scene->hidePopup(sheet.get());
    Expect(!scene->isPopupShowing(sheet.get()), "hidePopup removes the popup");

    scene->showPopup(sheet, 20, 20, 100, 40, options);
    scene->noteKey(jadefx::Key::Escape, true, false, 0);
    Expect(!scene->isPopupShowing(sheet.get()), "Escape hides an auto popup");

    options.autoHide = false;
    options.modal = true;
    options.fillScene = true;
    auto dimmer = jadefx::make<jadefx::StackPane>();
    dimmer->setBackground(jadefx::Color::rgba(0.f, 0.f, 0.f, 0.4f));
    scene->showPopup(dimmer, 0, 0, -1, -1, options);
    scene->layout(240, 160, 0);
    Expect(dimmer->getWidth() == 240 && dimmer->getHeight() == 160, "a fillScene popup covers the window");
    Click(*scene, 8, 8);
    Expect(scene->isPopupShowing(dimmer.get()), "a modal popup stays up when the root is pressed");
    scene->noteKey(jadefx::Key::Escape, true, false, 0);
    Expect(scene->isPopupShowing(dimmer.get()), "Escape does not dismiss a modal popup");
    scene->hidePopup(dimmer.get());
}

void TestHoverPopup() {
    auto tag = jadefx::make<jadefx::Label>("Term");
    tag->setPrefSize(80, 24);
    auto tip = jadefx::make<jadefx::Label>("Definition");
    tip->setPrefSize(90, 24);
    jadefx::HoverPopup hover;
    hover.content = tip;
    hover.showDelay = 1;
    hover.hideDelay = 0.2;
    hover.showDuration = 5;
    tag->setHoverPopup(hover);
    auto scene = jadefx::make<jadefx::Scene>(tag, 200, 120);
    scene->layout(200, 120, 0);
    const double x = tag->getAbsoluteX() + 4;
    const double y = tag->getAbsoluteY() + 4;
    scene->noteMove(x, y);
    scene->layout(200, 120, 0.4);
    Expect(!scene->isPopupShowing(tip.get()), "a hover popup waits out its show delay");
    scene->layout(200, 120, 1.1);
    Expect(scene->isPopupShowing(tip.get()), "a hover popup shows after the delay");
    scene->noteMove(180, 100);
    scene->layout(200, 120, 1.15);
    Expect(scene->isPopupShowing(tip.get()), "the hover popup stays during the hide delay");
    scene->layout(200, 120, 1.5);
    Expect(!scene->isPopupShowing(tip.get()), "the hover popup hides after the hide delay");

    scene->noteMove(x, y);
    scene->layout(200, 120, 3);
    Expect(scene->isPopupShowing(tip.get()), "hovering again shows the popup");
    Click(*scene, x, y);
    Expect(!scene->isPopupShowing(tip.get()), "a press hides the hover popup");
}

void TestKeyHook() {
    auto root = jadefx::make<jadefx::Pane>();
    auto scene = jadefx::make<jadefx::Scene>(root, 80, 40);
    scene->layout(80, 40, 0);
    int hits = 0;
    const int id = scene->addKeyHook([&](jadefx::KeyEvent& event) {
        if (event.pressed && event.key == jadefx::Key::S && event.shortcut()) {
            ++hits;
            event.consume();
        }
    });
    scene->noteKey(jadefx::Key::S, true, false, jadefx::Key::ModControl);
    Expect(hits == 1, "a key hook sees the shortcut before focus");
    scene->removeKeyHook(id);
    scene->noteKey(jadefx::Key::S, true, false, jadefx::Key::ModSuper);
    Expect(hits == 1, "removing a key hook stops it");
}

}  // namespace

int RunButtonTests() {
    TestButtonAction();
    TestPopup();
    TestHoverPopup();
    TestKeyHook();
    return gFailures;
}

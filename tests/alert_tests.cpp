#include "jadefx/jadefx.hpp"
#include "jadefx/scene/controls/Alert.hpp"

#include <cstdio>
#include <string>
#include <vector>

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

void ClickNode(jadefx::Scene& scene, jadefx::Node& node) {
    Click(scene, node.getAbsoluteX() + node.getWidth() * 0.5, node.getAbsoluteY() + node.getHeight() * 0.5);
}

jadefx::Node* AlertPanelOf(jadefx::Node* node) {
    for (jadefx::Node* cursor = node; cursor != nullptr; cursor = cursor->getParent()) {
        if (std::string(cursor->getElementType()) == "alert") {
            return cursor;
        }
    }
    return nullptr;
}

bool HasLabel(jadefx::Node* node, const std::string& text, float fontSize) {
    if (node == nullptr) {
        return false;
    }
    if (auto* label = dynamic_cast<jadefx::Label*>(node)) {
        if (label->getText() == text && (fontSize <= 0.f || label->getFont().size() == fontSize)) {
            return true;
        }
    }
    auto* pane = dynamic_cast<jadefx::Pane*>(node);
    if (pane == nullptr) {
        return false;
    }
    for (const std::shared_ptr<jadefx::Node>& child : pane->getChildren().items()) {
        if (HasLabel(child.get(), text, fontSize)) {
            return true;
        }
    }
    return false;
}

const char* TypeName(jadefx::AlertType type) {
    switch (type) {
        case jadefx::AlertType::Information:
            return "Information";
        case jadefx::AlertType::Warning:
            return "Warning";
        case jadefx::AlertType::Confirmation:
            return "Confirmation";
        case jadefx::AlertType::Error:
            return "Error";
        case jadefx::AlertType::None:
            return "";
    }
    return "";
}

std::shared_ptr<jadefx::Scene> MakeScene(const std::shared_ptr<jadefx::Button>& root) {
    auto scene = jadefx::make<jadefx::Scene>(root, 800, 400);
    scene->layout(800, 400, 0);
    return scene;
}

void TestButtonTypes() {
    using jadefx::ButtonType;
    Expect(ButtonType::Ok().getText() == "OK", "OK text");
    Expect(ButtonType::Ok().getButtonData() == ButtonType::Data::OkDone, "OK is OkDone");
    Expect(ButtonType::Cancel().getButtonData() == ButtonType::Data::CancelClose, "Cancel is CancelClose");
    Expect(ButtonType::Yes().getButtonData() == ButtonType::Data::OkDone, "Yes is OkDone");
    Expect(ButtonType::No().getButtonData() == ButtonType::Data::CancelClose, "No is CancelClose");
    Expect(ButtonType::Close().getText() == "Close", "Close text");
    Expect(ButtonType::Close().getButtonData() == ButtonType::Data::CancelClose, "Close is CancelClose");
    Expect(ButtonType::Ok() == ButtonType("OK", ButtonType::Data::OkDone), "equality uses text and data");
    Expect(ButtonType::Ok() != ButtonType("OK", ButtonType::Data::Other), "same text with other data differs");
    Expect(ButtonType::Ok() != ButtonType::Cancel(), "OK is not Cancel");

    const jadefx::AlertType typed[] = {jadefx::AlertType::Information, jadefx::AlertType::Warning,
                                       jadefx::AlertType::Error};
    for (jadefx::AlertType type : typed) {
        jadefx::Alert alert(type);
        Expect(alert.getAlertType() == type, "alert keeps its type");
        Expect(alert.getTitle() == TypeName(type), "default title is the type name");
        Expect(alert.getHeaderText() == TypeName(type), "default header is the type name");
        Expect(alert.getButtonTypes().size() == 1 && alert.getButtonTypes()[0] == ButtonType::Ok(),
               "information, warning, and error default to OK");
    }

    jadefx::Alert none(jadefx::AlertType::None);
    Expect(none.getTitle().empty() && none.getHeaderText().empty(), "None has no default chrome text");
    Expect(none.getButtonTypes().empty(), "None starts with no buttons");

    jadefx::Alert confirm(jadefx::AlertType::Confirmation);
    Expect(confirm.getButtonTypes().size() == 2, "confirmation has two default buttons");
    Expect(confirm.getButtonTypes()[0] == ButtonType::Ok() && confirm.getButtonTypes()[1] == ButtonType::Cancel(),
           "confirmation defaults are OK then Cancel");
}

void TestConfirmation() {
    int rootClicks = 0;
    auto root = jadefx::make<jadefx::Button>("Behind");
    root->setPrefSize(800, 400);
    root->setOnAction([&](jadefx::ActionEvent&) { ++rootClicks; });
    auto scene = MakeScene(root);

    jadefx::Alert alert(jadefx::AlertType::Confirmation, "Delete this item?");
    Expect(alert.getHeaderText() == "Confirmation", "confirmation header");
    Expect(alert.getContentText() == "Delete this item?", "confirmation content");
    Expect(alert.lookupButton(jadefx::ButtonType::Ok()) == nullptr, "lookup is null before show");

    int closed = 0;
    const jadefx::ButtonType* closedResult = nullptr;
    alert.setOnClosed([&](const jadefx::ButtonType* result) {
        ++closed;
        closedResult = result;
    });

    alert.show(*scene);
    scene->layout(800, 400, 0);
    jadefx::Button* ok = alert.lookupButton(jadefx::ButtonType::Ok());
    jadefx::Button* cancel = alert.lookupButton(jadefx::ButtonType::Cancel());
    Expect(ok != nullptr && cancel != nullptr, "confirmation shows OK and Cancel");
    Expect(ok->isDefaultButton() && cancel->isCancelButton(), "OK is default and Cancel is cancel");
    Expect(ok->isFocused(), "show focuses the default button");
    Expect(cancel->getAbsoluteX() + cancel->getWidth() <= ok->getAbsoluteX(), "Cancel sits left of OK");
    const double gap = ok->getAbsoluteX() - (cancel->getAbsoluteX() + cancel->getWidth());
    Expect(gap > 7.0 && gap < 9.0, "button row spacing is 8");

    jadefx::Node* panel = AlertPanelOf(ok);
    jadefx::Node* dimmer = panel != nullptr ? panel->getParent() : nullptr;
    Expect(panel != nullptr && std::string(panel->getElementType()) == "alert", "panel element type is alert");
    Expect(panel != nullptr && panel->getWidth() == 420, "panel is 420 wide");
    Expect(dimmer != nullptr && scene->isPopupShowing(dimmer), "dimmer popup is showing");
    Expect(dimmer != nullptr && dimmer->getWidth() == 800 && dimmer->getHeight() == 400, "dimmer covers the scene");
    Expect(panel != nullptr && panel->getAbsoluteX() > 40, "panel leaves the scene corners free");
    Expect(HasLabel(panel, "Confirmation", 18.f), "title label uses 18px");
    if (auto* title = dynamic_cast<jadefx::Label*>(panel != nullptr ? dynamic_cast<jadefx::Pane*>(panel)->getChildren()[0].get()
                                                                   : nullptr)) {
        Expect(jadefx::near(title->getTextFill(), jadefx::Color::rgb8(0x18, 0x80, 0x38)), "confirmation title color");
    } else {
        Expect(false, "title label is the first panel child");
    }
    Expect(HasLabel(panel, "Delete this item?", 0.f), "content label is shown");

    const jadefx::SizeSpec radius = panel->computedStyle().radius[0];
    Expect(radius.kind == jadefx::SizeKind::Pixels && radius.pixels == 8.0, "panel radius is 8");
    const jadefx::Color veil = dimmer->computedStyle().background.color;
    Expect(veil.a > 0.3f && veil.a < 0.4f && veil.r < 0.05f, "dimmer is a black 0.35 veil");

    Click(*scene, 6, 6);
    Expect(rootClicks == 0, "a corner click does not reach the root button");
    Expect(alert.getResult() == nullptr, "clicking the dimmer does not set a result");
    Expect(scene->isPopupShowing(dimmer), "clicking the dimmer does not dismiss the dialog");
    Click(*scene, panel->getAbsoluteX() + 8, panel->getAbsoluteY() + 8);
    Expect(rootClicks == 0, "clicking the panel does not reach the root button");
    Expect(alert.getResult() == nullptr, "clicking the panel padding does not set a result");

    ClickNode(*scene, *cancel);
    Expect(alert.getResult() != nullptr && *alert.getResult() == jadefx::ButtonType::Cancel(), "Cancel click sets Cancel");
    Expect(closed == 1 && closedResult == alert.getResult(), "onClosed runs once with the result");
    Expect(!scene->isPopupShowing(dimmer), "the popup hides after a result");
    Expect(alert.lookupButton(jadefx::ButtonType::Cancel()) == cancel, "lookup stays valid after hide");
    scene->noteKey(jadefx::Key::Enter, true, false, 0);
    Expect(closed == 1 && *alert.getResult() == jadefx::ButtonType::Cancel(), "the key hook is gone after hide");

    Click(*scene, 20, 20);
    Expect(rootClicks == 1, "the root button receives clicks after the dialog hides");

    alert.show(*scene);
    scene->layout(800, 400, 0);
    Expect(alert.getResult() == nullptr, "show clears the previous result");
    Expect(closed == 1, "replacing a hidden dialog does not close again");
    scene->noteKey(jadefx::Key::Escape, true, false, 0);
    Expect(alert.getResult() != nullptr && *alert.getResult() == jadefx::ButtonType::Cancel(), "Escape selects Cancel");
    Expect(closed == 2, "Escape closes once");

    alert.show(*scene);
    scene->layout(800, 400, 0);
    scene->noteKey(jadefx::Key::Enter, true, true, 0);
    Expect(alert.getResult() == nullptr, "a repeated Enter does not activate the dialog");
    scene->noteKey(jadefx::Key::Enter, true, false, 0);
    Expect(alert.getResult() != nullptr && *alert.getResult() == jadefx::ButtonType::Ok(), "Enter selects OK");
    Expect(closed == 3, "Enter closes once");

    alert.show(*scene);
    scene->layout(800, 400, 0);
    scene->noteKey(jadefx::Key::KpEnter, true, false, 0);
    Expect(alert.getResult() != nullptr && *alert.getResult() == jadefx::ButtonType::Ok(), "KpEnter selects OK");

    alert.show(*scene);
    alert.show(*scene);
    scene->layout(800, 400, 0);
    Expect(alert.getResult() == nullptr && closed == 4, "a second show does not invent a close");
    scene->noteKey(jadefx::Key::Enter, true, false, 0);
    Expect(closed == 5, "only the latest dialog hears Enter");
}

void TestInformationNoneAndCustom() {
    auto root = jadefx::make<jadefx::Button>("Behind");
    root->setPrefSize(800, 400);
    auto scene = MakeScene(root);

    jadefx::Alert info(jadefx::AlertType::Information, "Saved");
    info.show(*scene);
    scene->layout(800, 400, 0);
    Expect(info.lookupButton(jadefx::ButtonType::Ok()) != nullptr, "information has OK");
    Expect(info.lookupButton(jadefx::ButtonType::Cancel()) == nullptr, "information has no Cancel");
    jadefx::Node* panel = AlertPanelOf(info.lookupButton(jadefx::ButtonType::Ok()));
    Expect(HasLabel(panel, "Information", 18.f), "information title");
    if (panel != nullptr) {
        if (auto* title = dynamic_cast<jadefx::Label*>(dynamic_cast<jadefx::Pane*>(panel)->getChildren()[0].get())) {
            Expect(jadefx::near(title->getTextFill(), jadefx::Color::rgb8(0x1a, 0x73, 0xe8)), "information title color");
        }
    }
    scene->noteKey(jadefx::Key::Escape, true, false, 0);
    Expect(info.getResult() == nullptr, "Escape does nothing without a cancel button");
    scene->noteKey(jadefx::Key::Enter, true, false, 0);
    Expect(info.getResult() != nullptr && *info.getResult() == jadefx::ButtonType::Ok(), "Enter selects the OK button");

    jadefx::Alert blank(jadefx::AlertType::Warning, "Disk almost full");
    blank.setTitle("");
    blank.setHeaderText("");
    Expect(blank.getTitle().empty() && blank.getHeaderText().empty(), "explicit empty chrome stays empty");
    blank.show(*scene);
    scene->layout(800, 400, 0);
    jadefx::Button* blankOk = blank.lookupButton(jadefx::ButtonType::Ok());
    jadefx::Node* blankPanel = AlertPanelOf(blankOk);
    Expect(!HasLabel(blankPanel, "Warning", 0.f), "empty header and title hide those labels");
    Expect(HasLabel(blankPanel, "Disk almost full", 0.f), "content stays when the header is hidden");
    Expect(blank.getHeaderText().empty(), "show does not restore a cleared header");
    blank.setResult(jadefx::ButtonType::Ok());

    jadefx::Alert none(jadefx::AlertType::None, "Note");
    Expect(none.lookupButton(jadefx::ButtonType::Close()) == nullptr, "None has no Close yet");
    none.getButtonTypes().add(jadefx::ButtonType::Close());
    Expect(none.lookupButton(jadefx::ButtonType::Close()) == nullptr, "lookup stays null until show");
    none.show(*scene);
    scene->layout(800, 400, 0);
    jadefx::Button* close = none.lookupButton(jadefx::ButtonType::Close());
    Expect(close != nullptr && close->isCancelButton() && close->isFocused(), "Close is shown, cancel, and focused");
    Expect(none.lookupButton(jadefx::ButtonType::Ok()) == nullptr, "adding Close does not invent OK");
    scene->noteKey(jadefx::Key::Escape, true, false, 0);
    Expect(none.getResult() != nullptr && *none.getResult() == jadefx::ButtonType::Close(), "Escape selects Close");

    jadefx::Alert choice(jadefx::AlertType::Confirmation, "Save changes?",
                         {jadefx::ButtonType::Yes(), jadefx::ButtonType::No()});
    Expect(choice.getButtonTypes().size() == 2, "custom list replaces the defaults");
    Expect(choice.getButtonTypes()[0] == jadefx::ButtonType::Yes(), "custom list keeps Yes");
    Expect(choice.getButtonTypes()[1] == jadefx::ButtonType::No(), "custom list keeps No");
    choice.show(*scene);
    scene->layout(800, 400, 0);
    jadefx::Button* yes = choice.lookupButton(jadefx::ButtonType::Yes());
    jadefx::Button* no = choice.lookupButton(jadefx::ButtonType::No());
    Expect(yes != nullptr && no != nullptr, "Yes and No are built");
    Expect(choice.lookupButton(jadefx::ButtonType::Ok()) == nullptr, "custom list has no OK");
    Expect(choice.lookupButton(jadefx::ButtonType::Cancel()) == nullptr, "custom list has no Cancel");
    Expect(no->getAbsoluteX() + no->getWidth() <= yes->getAbsoluteX(), "No sits left of Yes");
    Expect(yes->isFocused() && yes->isDefaultButton() && no->isCancelButton(), "Yes is the default button");
    scene->noteKey(jadefx::Key::Enter, true, false, 0);
    Expect(choice.getResult() != nullptr && *choice.getResult() == jadefx::ButtonType::Yes(), "Enter selects Yes");

    std::vector<jadefx::ButtonType> roles = {
        jadefx::ButtonType("Done", jadefx::ButtonType::Data::OkDone),
        jadefx::ButtonType("Next", jadefx::ButtonType::Data::Right),
        jadefx::ButtonType("Back", jadefx::ButtonType::Data::Left),
        jadefx::ButtonType("Extra", jadefx::ButtonType::Data::Other),
        jadefx::ButtonType("Stop", jadefx::ButtonType::Data::CancelClose),
    };
    jadefx::Alert rolesAlert(jadefx::AlertType::None, "", roles);
    rolesAlert.show(*scene);
    scene->layout(800, 400, 0);
    const char* visual[] = {"Stop", "Extra", "Back", "Next", "Done"};
    double previous = -1;
    for (const char* name : visual) {
        const jadefx::ButtonType sample(name, name == std::string("Done")    ? jadefx::ButtonType::Data::OkDone
                                             : name == std::string("Next")   ? jadefx::ButtonType::Data::Right
                                             : name == std::string("Back")   ? jadefx::ButtonType::Data::Left
                                             : name == std::string("Extra")  ? jadefx::ButtonType::Data::Other
                                                                             : jadefx::ButtonType::Data::CancelClose);
        jadefx::Button* button = rolesAlert.lookupButton(sample);
        Expect(button != nullptr && button->getAbsoluteX() >= previous, "button roles are ordered left to right");
        if (button != nullptr) {
            previous = button->getAbsoluteX() + button->getWidth();
        }
    }
    Expect(rolesAlert.lookupButton(jadefx::ButtonType::Ok()) == nullptr, "Done is not the OK type");
    scene->noteKey(jadefx::Key::Escape, true, false, 0);
    Expect(rolesAlert.getResult() != nullptr && rolesAlert.getResult()->getText() == "Stop", "Escape selects CancelClose");
}

void TestShowAndWait() {
    int rootClicks = 0;
    auto root = jadefx::make<jadefx::Button>("Behind");
    root->setPrefSize(800, 400);
    root->setOnAction([&](jadefx::ActionEvent&) { ++rootClicks; });
    auto scene = MakeScene(root);

    jadefx::Alert alert(jadefx::AlertType::Error, "Could not save");
    Expect(alert.getTitle() == "Error" && alert.getHeaderText() == "Error", "error chrome");
    const jadefx::ButtonType* result = alert.showAndWait(*scene);
    scene->layout(800, 400, 0);
    Expect(result == nullptr && alert.getResult() == nullptr, "showAndWait without a pump returns nullptr");
    jadefx::Button* ok = alert.lookupButton(jadefx::ButtonType::Ok());
    jadefx::Node* panel = AlertPanelOf(ok);
    jadefx::Node* dimmer = panel != nullptr ? panel->getParent() : nullptr;
    Expect(ok != nullptr && dimmer != nullptr && scene->isPopupShowing(dimmer), "the dialog stays up without a pump");
    if (panel != nullptr) {
        if (auto* title = dynamic_cast<jadefx::Label*>(dynamic_cast<jadefx::Pane*>(panel)->getChildren()[0].get())) {
            Expect(jadefx::near(title->getTextFill(), jadefx::Color::rgb8(0xd9, 0x30, 0x25)), "error title color");
        }
    }
    Click(*scene, 6, 6);
    Click(*scene, panel->getAbsoluteX() + panel->getWidth() * 0.5, panel->getAbsoluteY() + 8);
    Expect(rootClicks == 0, "the root stays blocked while showAndWait left the dialog up");
    ClickNode(*scene, *ok);
    Expect(alert.getResult() != nullptr && *alert.getResult() == jadefx::ButtonType::Ok(), "OK click finishes the dialog");
    Expect(!scene->isPopupShowing(dimmer), "the dialog hides after the click");
    ClickNode(*scene, *root);
    Expect(rootClicks == 1, "the root button works after showAndWait is dismissed");

    int turns = 0;
    scene->setEventPump([&] {
        ++turns;
        return 0;
    });
    jadefx::Alert waiting(jadefx::AlertType::Warning, "Check the cable");
    Expect(waiting.showAndWait(*scene) == nullptr, "a pump turn of 0 returns nullptr");
    Expect(turns == 1, "showAndWait stops on the first turn that is not 1");
    Expect(waiting.lookupButton(jadefx::ButtonType::Ok()) != nullptr, "a refused pump leaves the dialog showing");
    jadefx::Node* waitingPanel = AlertPanelOf(waiting.lookupButton(jadefx::ButtonType::Ok()));
    if (waitingPanel != nullptr) {
        if (auto* title = dynamic_cast<jadefx::Label*>(dynamic_cast<jadefx::Pane*>(waitingPanel)->getChildren()[0].get())) {
            Expect(jadefx::near(title->getTextFill(), jadefx::Color::rgb8(0xe3, 0x74, 0x00)), "warning title color");
        }
    }
    waiting.setResult(jadefx::ButtonType::Ok());

    turns = 0;
    scene->setEventPump([&] {
        ++turns;
        if (turns == 2) {
            waiting.setResult(jadefx::ButtonType::Cancel());
        }
        return 1;
    });
    result = waiting.showAndWait(*scene);
    Expect(turns == 2 && result != nullptr && *result == jadefx::ButtonType::Cancel(), "showAndWait pumps until a result");
    scene->setEventPump(nullptr);
}

}  // namespace

int RunAlertTests() {
    TestButtonTypes();
    TestConfirmation();
    TestInformationNoneAndCustom();
    TestShowAndWait();
    return gFailures;
}

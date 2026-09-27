#include "jadefx/jadefx.hpp"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// Toasts and notifications: placement, stacking, timing, and their buttons.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

bool Near(double a, double b, double tolerance = 1.0) { return std::fabs(a - b) <= tolerance; }

struct Rig {
    std::shared_ptr<jadefx::Pane> root = jadefx::make<jadefx::Pane>();
    std::shared_ptr<jadefx::Scene> scene;
    double time = 0;

    Rig() {
        root->setPrefSize(600, 400);
        scene = jadefx::make<jadefx::Scene>(root, 600, 400);
        frame(0);
    }
    // Lays out at a time, then again a little later so cards settle into place.
    void frame(double at) {
        time = at;
        scene->layout(600, 400, at);
    }
    void settle(double from) {
        for (int i = 0; i <= 20; ++i) {
            frame(from + i * 0.05);
        }
    }
    std::vector<jadefx::Node*> find(const char* styleClass) { return scene->getElementsByClassName(styleClass); }
};

void Click(jadefx::Scene& scene, jadefx::Node& node) {
    const double x = node.getAbsoluteX() + node.getWidth() * 0.5;
    const double y = node.getAbsoluteY() + node.getHeight() * 0.5;
    scene.noteButton(0, true, x, y, 0);
    scene.noteButton(0, false, x, y, 0);
}

void TestToast() {
    Rig rig;
    jadefx::Toast::show(*rig.root, "Saved", 1.0);
    rig.frame(0);
    std::vector<jadefx::Node*> toasts = rig.find("toast");
    Expect(toasts.size() == 1 && rig.scene->isPopupShowing(toasts[0]), "a toast shows as a popup");
    if (toasts.empty()) {
        return;
    }
    jadefx::Node* toast = toasts[0];
    Expect(toast->getOpacity() < 0.1f && toast->isMouseTransparent(), "it fades in and lets clicks through");
    for (int i = 1; i <= 12; ++i) {
        rig.frame(i * 0.05);
    }
    Expect(toast->getOpacity() > 0.99f, "and is fully shown after the fade");
    Expect(Near(toast->getAbsoluteX() + toast->getWidth() * 0.5, 300) &&
               Near(toast->getAbsoluteY() + toast->getHeight(), 400 - 16),
           "it sits at the bottom center, above the margin");
    rig.frame(1.1);
    rig.frame(1.2);
    Expect(rig.scene->isPopupShowing(toast) && toast->getOpacity() < 0.99f, "after its time it fades out");
    rig.frame(1.5);
    Expect(rig.find("toast").empty(), "and is gone once faded");
}

void TestNotifications() {
    Rig rig;
    int actions = 0;
    int buttonActions = 0;
    jadefx::Notifications::create()
        .title("Build finished")
        .text("3 warnings\n0 errors")
        .owner(*rig.root)
        .hideAfter(0)
        .onAction([&](jadefx::ActionEvent&) { ++actions; })
        .action("Open log", [&](jadefx::ActionEvent&) { ++buttonActions; })
        .showWarning();
    rig.settle(0);
    std::vector<jadefx::Node*> cards = rig.find("notification");
    Expect(cards.size() == 1 && rig.find("warning").size() == 1, "the card has its type's class");
    Expect(rig.find("title").size() == 1 && rig.find("text").size() == 2, "a title and a label per line of text");
    if (cards.empty()) {
        return;
    }
    jadefx::Node* first = cards[0];
    Expect(Near(first->getAbsoluteX() + first->getWidth(), 600 - 16) &&
               Near(first->getAbsoluteY() + first->getHeight(), 400 - 16),
           "the default position is the bottom right");

    jadefx::Notifications::create().text("Second").owner(*rig.root).hideAfter(2).showInformation();
    rig.settle(1);
    cards = rig.find("notification");
    Expect(cards.size() == 2, "a second card stacks with the first");
    jadefx::Node* second = cards.size() == 2 ? (cards[0] == first ? cards[1] : cards[0]) : nullptr;
    if (second == nullptr) {
        return;
    }
    Expect(Near(second->getAbsoluteY() + second->getHeight(), 400 - 16) &&
               Near(first->getAbsoluteY() + first->getHeight(), second->getAbsoluteY() - 8),
           "the newest sits at the edge and the older one moves up");

    // A pointer resting on a card holds it past its time.
    rig.scene->noteMove(second->getAbsoluteX() + 10, second->getAbsoluteY() + 10);
    rig.settle(3.5);
    Expect(rig.scene->isPopupShowing(second), "a hovered card stays past its time");
    rig.scene->noteMove(1, 1);
    rig.settle(5);
    rig.settle(7);
    Expect(!rig.scene->isPopupShowing(second) && rig.find("notification").size() == 1,
           "it leaves once the pointer is gone");
    Expect(Near(first->getAbsoluteY() + first->getHeight(), 400 - 16), "and the one left slides back to the edge");

    // The action button runs its handler, not the card's, and closes the card.
    std::vector<jadefx::Node*> buttons;
    for (jadefx::Node* node : rig.find("actions")) {
        auto* box = dynamic_cast<jadefx::HBox*>(node);
        if (box != nullptr && !box->getChildren().empty()) {
            buttons.push_back(box->getChildren()[0].get());
        }
    }
    Expect(buttons.size() == 1, "the action is a button in .actions");
    if (!buttons.empty()) {
        Click(*rig.scene, *buttons[0]);
        rig.settle(8);
        Expect(buttonActions == 1 && actions == 0 && rig.find("notification").empty(),
               "an action button runs its handler and closes the card");
    }

    // A click on the card runs onAction.
    jadefx::Notifications::create()
        .title("Update ready")
        .owner(*rig.root)
        .position(jadefx::Pos::TopLeft)
        .hideAfter(0)
        .onAction([&](jadefx::ActionEvent&) { ++actions; })
        .show();
    rig.settle(9);
    cards = rig.find("notification");
    Expect(cards.size() == 1 && Near(cards[0]->getAbsoluteX(), 16) && Near(cards[0]->getAbsoluteY(), 16),
           "a top-left card sits in the top left corner");
    Expect(rig.find("information").empty() && rig.find("notification-icon").empty(), "show() has no type or icon");
    if (!cards.empty()) {
        jadefx::Node* card = cards[0];
        rig.scene->noteButton(0, true, card->getAbsoluteX() + 30, card->getAbsoluteY() + card->getHeight() * 0.5, 0);
        rig.scene->noteButton(0, false, card->getAbsoluteX() + 30, card->getAbsoluteY() + card->getHeight() * 0.5, 0);
        rig.settle(10);
        Expect(actions == 1 && rig.find("notification").empty(), "a click on the card runs onAction and closes it");
    }

    jadefx::Notifications::create().text("a").owner(*rig.root).hideAfter(0).showError();
    jadefx::Toast::show(*rig.root, "b", jadefx::Toast::LENGTH_LONG);
    rig.settle(11);
    Expect(rig.find("notification").size() == 1 && rig.find("toast").size() == 1, "cards and toasts share the scene");
    jadefx::Notifications::hideAll(*rig.root);
    rig.settle(12);
    Expect(rig.find("notification").empty() && rig.find("toast").empty(), "hideAll closes every one");
}

}  // namespace

int RunNotificationTests() {
    gFailures = 0;
    TestToast();
    TestNotifications();
    if (gFailures == 0) {
        std::printf("notification tests passed\n");
    }
    return gFailures;
}

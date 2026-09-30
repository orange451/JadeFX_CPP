#include "jadefx/jadefx.hpp"

#include <cstdio>
#include <memory>
#include <vector>

// The right and middle buttons, for a node that asks for every button.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

struct Press {
    int button;
    bool down;
    jadefx::Node* target;
};

// A pane that writes down each press and release it hears.
class Recorder : public jadefx::Pane {
public:
    std::vector<Press> presses;

    void handleMousePressed(const jadefx::MouseEvent& event) override {
        presses.push_back({event.button, true, event.target});
    }
    void handleMouseReleased(const jadefx::MouseEvent& event) override {
        presses.push_back({event.button, false, event.target});
    }
};

// A flagged view at the top left with a plain child inside it, and a plain box beside it.
struct Rig {
    std::shared_ptr<jadefx::Pane> root = jadefx::make<jadefx::Pane>();
    std::shared_ptr<Recorder> view = std::make_shared<Recorder>();
    std::shared_ptr<Recorder> inner = std::make_shared<Recorder>();
    std::shared_ptr<Recorder> plain = std::make_shared<Recorder>();
    std::shared_ptr<jadefx::Scene> scene;

    Rig() {
        root->setPrefSize(400, 300);
        view->setPrefSize(200, 200);
        inner->setPrefSize(50, 50);
        inner->setTranslateX(100);
        inner->setTranslateY(100);
        view->getChildren().add(inner);
        plain->setPrefSize(100, 100);
        plain->setTranslateX(250);
        root->getChildren().add(view);
        root->getChildren().add(plain);
        view->setReceivesAllButtons(true);
        scene = jadefx::make<jadefx::Scene>(root, 400, 300);
        scene->layout(400, 300, 0);
    }
};

// A flagged node hears a right press and its release, even with the pointer moved off it.
void TestRightButtonReachesFlaggedNode() {
    Rig rig;
    Expect(rig.view->receivesAllButtons(), "the flag is recorded");
    Expect(!rig.plain->receivesAllButtons(), "a node starts without the flag");
    rig.scene->noteButton(1, true, 20, 20);
    rig.scene->noteMove(380, 280);
    rig.scene->noteButton(1, false, 380, 280);
    Expect(rig.view->presses.size() == 2, "the flagged node hears the right press and release");
    if (rig.view->presses.size() == 2) {
        Expect(rig.view->presses[0].button == 1 && rig.view->presses[0].down, "the press carries button 1");
        Expect(rig.view->presses[1].button == 1 && !rig.view->presses[1].down, "the release carries button 1");
        Expect(rig.view->presses[0].target == rig.view.get(), "the event targets the flagged node");
    }
}

// A press on a plain child goes to its nearest flagged ancestor, not to the child.
void TestRightButtonWalksUpToFlaggedAncestor() {
    Rig rig;
    rig.scene->noteButton(1, true, 120, 120);
    rig.scene->noteButton(1, false, 120, 120);
    Expect(rig.inner->presses.empty(), "the plain child under the pointer hears nothing");
    Expect(rig.view->presses.size() == 2, "its flagged ancestor hears the press and release");
}

// A node without the flag, and no flagged ancestor, still hears nothing but the left button.
void TestUnflaggedNodeHearsOnlyLeft() {
    Rig rig;
    rig.scene->noteButton(1, true, 300, 50);
    rig.scene->noteButton(1, false, 300, 50);
    rig.scene->noteButton(2, true, 300, 50);
    rig.scene->noteButton(2, false, 300, 50);
    Expect(rig.plain->presses.empty(), "an unflagged node hears no right or middle button");
    rig.scene->noteButton(0, true, 300, 50);
    rig.scene->noteButton(0, false, 300, 50);
    Expect(rig.plain->presses.size() == 2, "an unflagged node still hears the left button");
    Expect(rig.view->presses.empty(), "the flagged node hears nothing pressed elsewhere");
}

// A right press on a flagged node still asks for a context menu.
void TestContextMenuStillFires() {
    Rig rig;
    int menus = 0;
    rig.view->setOnContextMenuRequested([&menus](const jadefx::MouseEvent&) { ++menus; });
    rig.scene->noteButton(1, true, 20, 20);
    Expect(menus == 1, "a right press still fires the context menu");
    Expect(rig.view->presses.size() == 1, "and the flagged node hears the press");
    rig.scene->noteButton(1, false, 20, 20);
    Expect(menus == 1, "the release does not fire it again");
}

// The middle button works the same way, and each button keeps its own target.
void TestMiddleButton() {
    Rig rig;
    rig.scene->noteButton(2, true, 20, 20);
    rig.scene->noteButton(1, true, 20, 20);
    rig.scene->noteButton(2, false, 300, 50);
    rig.scene->noteButton(1, false, 300, 50);
    Expect(rig.view->presses.size() == 4, "the flagged node hears both buttons");
    if (rig.view->presses.size() == 4) {
        Expect(rig.view->presses[0].button == 2 && rig.view->presses[0].down, "a middle press carries button 2");
        Expect(rig.view->presses[2].button == 2 && !rig.view->presses[2].down, "a middle release carries button 2");
        Expect(rig.view->presses[3].button == 1 && !rig.view->presses[3].down, "the right release follows");
    }
    Expect(rig.plain->presses.empty(), "the node under the releases hears nothing");
}

// A target that leaves the scene is forgotten, so its release is not delivered anywhere.
void TestRemovedTargetIsForgotten() {
    Rig rig;
    rig.scene->noteButton(1, true, 20, 20);
    rig.root->getChildren().removeAt(0);
    rig.scene->noteButton(1, false, 20, 20);
    Expect(rig.view->presses.size() == 1, "a removed target hears no release");
}

// Losing the window's focus ends a held button, since its release goes to another window.
void TestFocusLossReleases() {
    Rig rig;
    rig.scene->noteButton(1, true, 20, 20);
    rig.scene->noteWindowFocus(false);
    Expect(rig.view->presses.size() == 2, "losing focus releases the held right button");
    if (rig.view->presses.size() == 2) {
        Expect(rig.view->presses[1].button == 1 && !rig.view->presses[1].down, "as a release of button 1");
    }
    rig.scene->noteWindowFocus(true);
    rig.scene->noteButton(1, false, 20, 20);
    Expect(rig.view->presses.size() == 2, "the late release is not delivered again");
}

}  // namespace

int RunMouseButtonTests() {
    TestRightButtonReachesFlaggedNode();
    TestRightButtonWalksUpToFlaggedAncestor();
    TestUnflaggedNodeHearsOnlyLeft();
    TestContextMenuStillFires();
    TestMiddleButton();
    TestRemovedTargetIsForgotten();
    TestFocusLossReleases();
    return gFailures;
}

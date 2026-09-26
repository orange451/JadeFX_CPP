#include "jadefx/jadefx.hpp"
#include "jadefx/scene/controls/MenuBar.hpp"
#include "jadefx/scene/controls/MenuButton.hpp"

#include <cmath>
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

void ClickNode(jadefx::Scene& scene, const jadefx::Node& node) {
    Click(scene, node.getAbsoluteX() + node.getWidth() * 0.5, node.getAbsoluteY() + node.getHeight() * 0.5);
}

void MoveTo(jadefx::Scene& scene, const jadefx::Node& node) {
    scene.noteMove(node.getAbsoluteX() + node.getWidth() * 0.5, node.getAbsoluteY() + node.getHeight() * 0.5);
}

std::shared_ptr<jadefx::Pane> FullRoot() {
    auto root = jadefx::make<jadefx::Pane>();
    root->setPrefSize(720, 420);
    return root;
}

std::vector<jadefx::Node*> Popups(jadefx::Scene& scene) { return scene.getElementsByClassName("menu-popup"); }

void TestMenuItemBasics() {
    auto item = jadefx::make<jadefx::MenuItem>("Save");
    Expect(item->isVisible(), "a menu item starts visible");
    Expect(!item->isDisable(), "a menu item starts enabled");
    Expect(item->getParentMenu() == nullptr, "a new item has no parent menu");
    int fires = 0;
    jadefx::Node* source = reinterpret_cast<jadefx::Node*>(1);
    item->setOnAction([&](jadefx::ActionEvent& event) {
        ++fires;
        source = event.source;
    });
    item->fire();
    Expect(fires == 1 && source == nullptr, "fire() runs the action with a null source");
    item->setDisable(true);
    item->fire();
    Expect(fires == 1, "fire() does nothing while the item is disabled");
    item->setAccelerator(jadefx::Key::S, jadefx::Key::ModControl | jadefx::Key::ModShift);
    Expect(item->getAcceleratorKey() == jadefx::Key::S, "the accelerator key is stored");
    Expect(item->getAcceleratorMods() == (jadefx::Key::ModControl | jadefx::Key::ModShift),
           "the accelerator modifiers are stored");

    auto first = jadefx::make<jadefx::Menu>("First");
    auto second = jadefx::make<jadefx::Menu>("Second");
    item->setDisable(false);
    first->getItems().add(item);
    Expect(item->getParentMenu() == first.get(), "adding an item sets its parent menu");
    second->getItems().add(item);
    Expect(item->getParentMenu() == second.get(), "moving an item reparents it");
    Expect(first->getItems().empty(), "moving an item removes it from the old menu");
    Expect(second->getItems().size() == 1, "the item lives in the new menu");
    second->getItems().add(item);
    Expect(second->getItems().size() == 1, "a menu does not keep two copies of one item");

    auto loop = jadefx::make<jadefx::Menu>("Loop");
    loop->getItems().add(loop);
    Expect(loop->getItems().empty(), "a menu cannot contain itself");
    Expect(loop->getParentMenu() == nullptr, "rejecting a cycle leaves the menu unparented");
}

void TestMenuButtonActivatesItem() {
    int fires = 0;
    int buttonFires = 0;
    jadefx::Node* source = reinterpret_cast<jadefx::Node*>(1);
    auto save = jadefx::make<jadefx::MenuItem>("Save");
    save->setOnAction([&](jadefx::ActionEvent& event) {
        ++fires;
        source = event.source;
    });
    auto button = jadefx::make<jadefx::MenuButton>("File");
    button->setOnAction([&](jadefx::ActionEvent&) { ++buttonFires; });
    button->getItems().add(save);
    auto root = FullRoot();
    root->getChildren().add(button);
    auto scene = jadefx::make<jadefx::Scene>(root, 720, 420);
    scene->layout(720, 420, 0);
    Expect(std::string(button->getElementType()) == "menubutton", "a menu button reports menubutton");

    ClickNode(*scene, *button);
    Expect(button->isShowing(), "clicking a menu button shows its menu");
    const std::vector<jadefx::Node*> popups = Popups(*scene);
    Expect(popups.size() == 1, "one popup is showing after the menu opens");
    Expect(!popups.empty() && scene->isPopupShowing(popups[0]), "isPopupShowing sees the open menu");
    Expect(!popups.empty() && popups[0]->getWidth() >= 160, "the menu popup is at least 160 wide");
    Expect(!popups.empty() &&
               popups[0]->getAbsoluteY() + 0.5 >= button->getAbsoluteY() + button->getHeight(),
           "the menu popup sits under the button");
    jadefx::Node* row = scene->getElementById("Save");
    Expect(row != nullptr && std::string(row->getElementType()) == "menu-item", "the item row is a menu-item");
    Expect(row != nullptr && row->getHeight() == 28, "a menu item row is 28 tall");
    Expect(row != nullptr && !popups.empty() && row->getWidth() + 8 == popups[0]->getWidth(),
           "the item row fills the popup's inner width");
    scene->layout(720, 420, 0);
    if (row != nullptr) {
        ClickNode(*scene, *row);
    }
    Expect(fires == 1 && source == nullptr, "clicking the item runs its action");
    Expect(buttonFires == 0, "opening a menu button is not a button action");
    Expect(!button->isShowing(), "activating an item hides the menu");
    Expect(popups.empty() || !scene->isPopupShowing(popups[0]), "the popup is gone after the item fires");
}

void TestMenuButtonToggleAndOutsidePress() {
    auto button = jadefx::make<jadefx::MenuButton>("File");
    button->getItems().add(jadefx::make<jadefx::MenuItem>("Save"));
    auto root = FullRoot();
    root->getChildren().add(button);
    auto scene = jadefx::make<jadefx::Scene>(root, 720, 420);
    scene->layout(720, 420, 0);

    ClickNode(*scene, *button);
    const std::vector<jadefx::Node*> popups = Popups(*scene);
    Expect(button->isShowing() && !popups.empty(), "the menu opens on the first click");
    ClickNode(*scene, *button);
    Expect(!button->isShowing(), "clicking the menu button again hides it");
    Expect(popups.empty() || !scene->isPopupShowing(popups[0]), "the second click drops the popup");

    ClickNode(*scene, *button);
    Expect(button->isShowing(), "the menu opens again");
    const std::vector<jadefx::Node*> again = Popups(*scene);
    Click(*scene, 700, 400);
    Expect(!button->isShowing(), "clicking elsewhere hides the menu");
    Expect(again.empty() || !scene->isPopupShowing(again[0]), "an outside press drops the popup");
}

void TestSeparatorAndDisabled() {
    int separatorFires = 0;
    int disabledFires = 0;
    int enabledFires = 0;
    auto separator = jadefx::make<jadefx::SeparatorMenuItem>();
    separator->setOnAction([&](jadefx::ActionEvent&) { ++separatorFires; });
    auto blocked = jadefx::make<jadefx::MenuItem>("Blocked");
    blocked->setDisable(true);
    blocked->setOnAction([&](jadefx::ActionEvent&) { ++disabledFires; });
    auto allowed = jadefx::make<jadefx::MenuItem>("Allowed");
    allowed->setOnAction([&](jadefx::ActionEvent&) { ++enabledFires; });

    auto button = jadefx::make<jadefx::MenuButton>("Edit");
    button->getItems().add(blocked);
    button->getItems().add(separator);
    button->getItems().add(allowed);
    auto root = FullRoot();
    root->getChildren().add(button);
    auto scene = jadefx::make<jadefx::Scene>(root, 720, 420);
    scene->layout(720, 420, 0);
    ClickNode(*scene, *button);
    scene->layout(720, 420, 0);

    const std::vector<jadefx::Node*> lines = scene->getElementsByClassName("menu-separator");
    Expect(lines.size() == 1, "a separator row is built");
    Expect(!lines.empty() && std::string(lines[0]->getElementType()) == "separator", "the divider reports separator");
    Expect(!lines.empty() && lines[0]->getHeight() == 9, "a separator row is 9 tall");
    if (!lines.empty()) {
        ClickNode(*scene, *lines[0]);
    }
    Expect(separatorFires == 0, "a separator does not fire");
    Expect(button->isShowing(), "clicking a separator leaves the menu open");

    jadefx::Node* blockedRow = scene->getElementById("Blocked");
    Expect(blockedRow != nullptr, "a disabled item still has a row");
    Expect(blockedRow != nullptr && blockedRow->computedStyle().opacity > 0.4f &&
               blockedRow->computedStyle().opacity < 0.5f,
           "a disabled row is drawn at 0.45 opacity");
    if (blockedRow != nullptr) {
        ClickNode(*scene, *blockedRow);
    }
    Expect(disabledFires == 0, "a disabled item does not fire");
    Expect(button->isShowing(), "clicking a disabled item leaves the menu open");

    jadefx::Node* allowedRow = scene->getElementById("Allowed");
    if (allowedRow != nullptr) {
        ClickNode(*scene, *allowedRow);
    }
    Expect(enabledFires == 1, "an enabled item still fires");
    Expect(!button->isShowing(), "the enabled item hides the menu");
}

void TestMenuBarSwitchesOnMove() {
    auto alpha = jadefx::make<jadefx::Menu>("Alpha");
    alpha->getItems().add(jadefx::make<jadefx::MenuItem>("One"));
    auto beta = jadefx::make<jadefx::Menu>("Beta");
    beta->getItems().add(jadefx::make<jadefx::MenuItem>("Two"));
    auto bar = jadefx::make<jadefx::MenuBar>();
    Expect(std::string(bar->getElementType()) == "menubar", "a menu bar reports menubar");
    bar->getMenus().add(alpha);
    bar->getMenus().add(beta);
    auto root = FullRoot();
    root->getChildren().add(bar);
    auto scene = jadefx::make<jadefx::Scene>(root, 720, 420);
    scene->layout(720, 420, 0);

    jadefx::Node* first = scene->getElementById("menu:Alpha");
    jadefx::Node* second = scene->getElementById("menu:Beta");
    Expect(first != nullptr && std::string(first->getElementType()) == "menu", "the first title is a menu");
    Expect(second != nullptr && std::string(second->getElementType()) == "menu", "the second title is a menu");
    Expect(first != nullptr && first->getHeight() == 28, "a menu title is 28 tall");
    if (first == nullptr || second == nullptr) {
        return;
    }
    ClickNode(*scene, *first);
    Expect(alpha->isShowing(), "clicking the first title shows that menu");
    Expect(!beta->isShowing(), "the second menu stays closed");
    Expect(scene->getElementById("One") != nullptr, "the first menu's item is showing");
    const std::vector<jadefx::Node*> firstPopups = Popups(*scene);
    Expect(firstPopups.size() == 1 && scene->isPopupShowing(firstPopups[0]), "the first menu popup is showing");

    MoveTo(*scene, *second);
    Expect(beta->isShowing(), "moving onto the second title shows that menu");
    Expect(!alpha->isShowing(), "moving onto the second title hides the first menu");
    Expect(scene->getElementById("Two") != nullptr, "the second menu's item is showing");
    Expect(scene->getElementById("One") == nullptr, "the first menu's item is gone");
    const std::vector<jadefx::Node*> secondPopups = Popups(*scene);
    Expect(secondPopups.size() == 1 && scene->isPopupShowing(secondPopups[0]), "only the second popup remains");
    Expect(firstPopups.empty() || !scene->isPopupShowing(firstPopups[0]), "the first popup was hidden");

    ClickNode(*scene, *second);
    Expect(!beta->isShowing(), "clicking the open title again hides it");
    ClickNode(*scene, *first);
    Expect(alpha->isShowing() && !beta->isShowing(), "clicking a title opens that menu");
    ClickNode(*scene, *second);
    Expect(beta->isShowing() && !alpha->isShowing(), "clicking another title switches menus");
}

void TestSubmenuOpensRight() {
    int fires = 0;
    auto leaf = jadefx::make<jadefx::MenuItem>("Leaf");
    leaf->setOnAction([&](jadefx::ActionEvent&) { ++fires; });
    auto more = jadefx::make<jadefx::Menu>("More");
    more->getItems().add(leaf);
    auto button = jadefx::make<jadefx::MenuButton>("Tools");
    button->getItems().add(more);
    auto root = FullRoot();
    root->getChildren().add(button);
    auto scene = jadefx::make<jadefx::Scene>(root, 720, 420);
    scene->layout(720, 420, 0);
    ClickNode(*scene, *button);
    scene->layout(720, 420, 0);

    jadefx::Node* row = scene->getElementById("More");
    Expect(row != nullptr, "the submenu row is showing");
    if (row == nullptr) {
        return;
    }
    MoveTo(*scene, *row);
    Expect(more->isShowing(), "hovering a submenu row opens it");
    jadefx::Node* leafRow = scene->getElementById("Leaf");
    Expect(leafRow != nullptr, "the submenu leaf is showing");
    Expect(leafRow != nullptr && leafRow->getParent() != nullptr && scene->isPopupShowing(leafRow->getParent()),
           "the submenu popup is showing");
    Expect(leafRow != nullptr &&
               leafRow->getAbsoluteX() + 0.5 >= row->getAbsoluteX() + row->getWidth(),
           "the submenu opens to the right of its row");
    if (leafRow != nullptr) {
        ClickNode(*scene, *leafRow);
    }
    Expect(fires == 1, "the submenu leaf fires");
    Expect(!button->isShowing() && !more->isShowing(), "activating the leaf hides the menus");
    Expect(Popups(*scene).empty(), "submenu popups are gone after the leaf fires");
}

void TestAcceleratorWhileClosed() {
    int fires = 0;
    int nestedFires = 0;
    int blockedFires = 0;
    int hiddenFires = 0;
    auto save = jadefx::make<jadefx::MenuItem>("Save");
    save->setAccelerator(jadefx::Key::S, jadefx::Key::ModControl);
    save->setOnAction([&](jadefx::ActionEvent&) { ++fires; });
    auto nested = jadefx::make<jadefx::MenuItem>("Look");
    nested->setAccelerator(jadefx::Key::L, jadefx::Key::ModSuper);
    nested->setOnAction([&](jadefx::ActionEvent&) { ++nestedFires; });
    auto more = jadefx::make<jadefx::Menu>("More");
    more->getItems().add(nested);
    auto blocked = jadefx::make<jadefx::MenuItem>("Blocked");
    blocked->setDisable(true);
    blocked->setAccelerator(jadefx::Key::D, jadefx::Key::ModControl);
    blocked->setOnAction([&](jadefx::ActionEvent&) { ++blockedFires; });
    auto ghost = jadefx::make<jadefx::MenuItem>("Ghost");
    ghost->setVisible(false);
    ghost->setAccelerator(jadefx::Key::G, jadefx::Key::ModControl);
    ghost->setOnAction([&](jadefx::ActionEvent&) { ++hiddenFires; });

    auto button = jadefx::make<jadefx::MenuButton>("File");
    button->getItems().add(save);
    button->getItems().add(more);
    button->getItems().add(blocked);
    button->getItems().add(ghost);
    auto root = FullRoot();
    root->getChildren().add(button);
    auto scene = jadefx::make<jadefx::Scene>(root, 720, 420);
    scene->layout(720, 420, 0);
    root->requestFocus();
    Expect(scene->focusedNode() == root.get(), "a different node is focused");
    Expect(!button->isShowing(), "the accelerator runs while the menu is closed");

    scene->noteKey(jadefx::Key::S, true, true, jadefx::Key::ModControl);
    Expect(fires == 0, "a repeated accelerator does not fire");
    scene->noteKey(jadefx::Key::S, false, false, jadefx::Key::ModControl);
    Expect(fires == 0, "releasing the accelerator does not fire");
    scene->noteKey(jadefx::Key::S, true, false, jadefx::Key::ModControl | jadefx::Key::ModShift);
    Expect(fires == 0, "Shift+Ctrl+S does not match Ctrl+S");
    scene->noteKey(jadefx::Key::S, true, false, 0);
    Expect(fires == 0, "plain S does not match a Ctrl+S accelerator");
    scene->noteKey(jadefx::Key::S, true, false, jadefx::Key::ModControl);
    Expect(fires == 1, "Ctrl+S fires the item while the menu is closed");
    Expect(!button->isShowing(), "the accelerator leaves the menu closed");
    Expect(scene->focusedNode() == root.get(), "the accelerator does not move focus");

    scene->noteKey(jadefx::Key::S, true, false, jadefx::Key::ModSuper);
    Expect(fires == 2, "Command+S matches a ModControl accelerator");
    scene->noteKey(jadefx::Key::L, true, false, jadefx::Key::ModControl);
    Expect(nestedFires == 1, "an accelerator inside a closed submenu fires");
    scene->noteKey(jadefx::Key::D, true, false, jadefx::Key::ModControl);
    scene->noteKey(jadefx::Key::G, true, false, jadefx::Key::ModControl);
    Expect(blockedFires == 0, "a disabled item's accelerator does not fire");
    Expect(hiddenFires == 0, "a hidden item's accelerator does not fire");

    ClickNode(*scene, *button);
    Expect(button->isShowing(), "the menu is open before the accelerator hides it");
    Expect(scene->getElementById("Ghost") == nullptr, "a hidden item has no row");
    scene->noteKey(jadefx::Key::S, true, false, jadefx::Key::ModControl);
    Expect(fires == 3, "the accelerator still fires while the menu is open");
    Expect(!button->isShowing(), "the accelerator hides the open menu");
}

void TestRebuildWhileOpen() {
    auto button = jadefx::make<jadefx::MenuButton>("File");
    button->getItems().add(jadefx::make<jadefx::MenuItem>("Save"));
    auto root = FullRoot();
    root->getChildren().add(button);
    auto scene = jadefx::make<jadefx::Scene>(root, 720, 420);
    scene->layout(720, 420, 0);
    ClickNode(*scene, *button);
    auto extra = jadefx::make<jadefx::MenuItem>("Extra");
    button->getItems().add(extra);
    scene->layout(720, 420, 0);
    Expect(button->isShowing(), "adding an item keeps the menu open");
    Expect(scene->getElementById("Extra") != nullptr, "the new item is in the open menu");
    button->getItems().removeAt(button->getItems().size() - 1);
    scene->layout(720, 420, 0);
    Expect(button->isShowing(), "removing an item keeps the menu open");
    Expect(scene->getElementById("Extra") == nullptr, "the removed item leaves the open menu");
}

void TestLeavingScene() {
    int fires = 0;
    auto save = jadefx::make<jadefx::MenuItem>("Save");
    save->setAccelerator(jadefx::Key::S, jadefx::Key::ModControl);
    save->setOnAction([&](jadefx::ActionEvent&) { ++fires; });
    auto button = jadefx::make<jadefx::MenuButton>("File");
    button->getItems().add(save);
    auto bar = jadefx::make<jadefx::MenuBar>();
    auto menu = jadefx::make<jadefx::Menu>("Alpha");
    int barFires = 0;
    auto other = jadefx::make<jadefx::MenuItem>("Other");
    other->setAccelerator(jadefx::Key::O, jadefx::Key::ModControl);
    other->setOnAction([&](jadefx::ActionEvent&) { ++barFires; });
    menu->getItems().add(other);
    bar->getMenus().add(menu);
    button->setTranslateY(80);
    auto root = FullRoot();
    root->getChildren().add(button);
    root->getChildren().add(bar);
    auto scene = jadefx::make<jadefx::Scene>(root, 720, 420);
    scene->layout(720, 420, 0);
    ClickNode(*scene, *button);
    Expect(button->isShowing(), "the menu is open before the button leaves");
    root->getChildren().removeAt(0);
    Expect(!button->isShowing(), "leaving the scene hides the menu button popup");
    scene->noteKey(jadefx::Key::S, true, false, jadefx::Key::ModControl);
    Expect(fires == 0, "a menu button outside the scene ignores its accelerator");

    jadefx::Node* title = scene->getElementById("menu:Alpha");
    Expect(title != nullptr, "the menu title is in the bar");
    if (title != nullptr) {
        ClickNode(*scene, *title);
    }
    Expect(menu->isShowing(), "the bar menu is open before the bar leaves");
    root->getChildren().clear();
    Expect(!menu->isShowing(), "leaving the scene hides the menu bar popup");
    scene->noteKey(jadefx::Key::O, true, false, jadefx::Key::ModControl);
    Expect(barFires == 0, "a menu bar outside the scene ignores its accelerator");
}

void TestEscapeHidesMenu() {
    auto button = jadefx::make<jadefx::MenuButton>("File");
    button->getItems().add(jadefx::make<jadefx::MenuItem>("Save"));
    auto root = FullRoot();
    root->getChildren().add(button);
    auto scene = jadefx::make<jadefx::Scene>(root, 720, 420);
    scene->layout(720, 420, 0);
    ClickNode(*scene, *button);
    const std::vector<jadefx::Node*> popups = Popups(*scene);
    Expect(button->isShowing() && !popups.empty() && scene->isPopupShowing(popups[0]), "the menu is open before Escape");
    scene->noteKey(jadefx::Key::Escape, true, false, 0);
    Expect(!button->isShowing(), "Escape hides the open menu");
    Expect(popups.empty() || !scene->isPopupShowing(popups[0]), "Escape drops the popup");
}

void TestMenuItemGraphic() {
    auto icon = jadefx::make<jadefx::Pane>();
    icon->setPrefSize(16, 16);
    icon->setElementId("save-icon");
    auto save = jadefx::make<jadefx::MenuItem>("Save");
    Expect(save->getGraphic() == nullptr, "a menu item starts without a graphic");
    save->setGraphic(icon);
    Expect(save->getGraphic() == icon, "setGraphic keeps the node");
    save->setAccelerator(jadefx::Key::S, jadefx::Key::ModControl);
    int fires = 0;
    save->setOnAction([&](jadefx::ActionEvent&) { ++fires; });

    auto plain = jadefx::make<jadefx::MenuItem>("Open");
    auto button = jadefx::make<jadefx::MenuButton>("File");
    button->getItems().add(save);
    button->getItems().add(plain);
    auto root = FullRoot();
    root->getChildren().add(button);
    auto scene = jadefx::make<jadefx::Scene>(root, 720, 420);
    scene->layout(720, 420, 0);
    ClickNode(*scene, *button);
    scene->layout(720, 420, 0);

    jadefx::Node* row = scene->getElementById("Save");
    jadefx::Node* mark = scene->getElementById("save-icon");
    jadefx::Node* saveLabel = scene->getElementById("menu-label:Save");
    jadefx::Node* openLabel = scene->getElementById("menu-label:Open");
    Expect(mark != nullptr && mark->getParent() == row, "the graphic is a child of its row");
    Expect(mark != nullptr && std::abs(mark->getX() - 12) < 0.5, "the graphic sits in the row padding");
    Expect(mark != nullptr && std::abs(mark->getWidth() - 16) < 0.5 && std::abs(mark->getHeight() - 16) < 0.5,
           "the graphic keeps its 16px size");
    Expect(saveLabel != nullptr && mark != nullptr && saveLabel->getX() + 0.5 >= mark->getX() + mark->getWidth(),
           "the label starts to the right of the graphic");
    Expect(saveLabel != nullptr && openLabel != nullptr && std::abs(saveLabel->getX() - openLabel->getX()) < 0.5,
           "a row without a graphic lines its label up with the icon column");
    jadefx::Node* accel = scene->getElementById("menu-accel:Save");
    Expect(accel != nullptr && saveLabel != nullptr && accel->getX() + 0.5 >= saveLabel->getX() + saveLabel->getWidth(),
           "the accelerator stays to the right of the label");
    if (mark != nullptr) {
        Click(*scene, mark->getAbsoluteX() + mark->getWidth() * 0.5, mark->getAbsoluteY() + mark->getHeight() * 0.5);
    }
    Expect(fires == 1, "a click on the graphic runs the item");
    Expect(!button->isShowing(), "the graphic click hides the menu");

    ClickNode(*scene, *button);
    scene->layout(720, 420, 0);
    auto next = jadefx::make<jadefx::Pane>();
    next->setPrefSize(16, 16);
    next->setElementId("next-icon");
    save->setGraphic(next);
    scene->layout(720, 420, 0);
    Expect(scene->getElementById("save-icon") == nullptr, "replacing the graphic removes the old one");
    Expect(scene->getElementById("next-icon") != nullptr, "the open menu shows the new graphic");
    Expect(save->getGraphic() == next, "getGraphic returns the replacement");

    save->setGraphic(nullptr);
    scene->layout(720, 420, 0);
    Expect(save->getGraphic() == nullptr, "clearing the graphic drops it");
    Expect(scene->getElementById("next-icon") == nullptr, "the open menu drops a cleared graphic");
    saveLabel = scene->getElementById("menu-label:Save");
    openLabel = scene->getElementById("menu-label:Open");
    Expect(saveLabel != nullptr && std::abs(saveLabel->getX() - 12) < 0.5, "the label returns to the row padding");
    Expect(saveLabel != nullptr && openLabel != nullptr && std::abs(saveLabel->getX() - openLabel->getX()) < 0.5,
           "clearing the graphic lines both labels up");
}

void TestGraphicSurvivesReopen() {
    auto icon = jadefx::make<jadefx::Pane>();
    icon->setPrefSize(16, 16);
    icon->setElementId("new-icon");
    auto item = jadefx::make<jadefx::MenuItem>("New");
    item->setGraphic(icon);
    auto file = jadefx::make<jadefx::Menu>("File");
    file->getItems().add(item);
    auto bar = jadefx::make<jadefx::MenuBar>();
    bar->getMenus().add(file);
    auto root = FullRoot();
    root->getChildren().add(bar);
    auto scene = jadefx::make<jadefx::Scene>(root, 720, 420);
    scene->layout(720, 420, 0);
    jadefx::Node* title = scene->getElementById("menu:File");
    if (title == nullptr) {
        Expect(false, "the File title exists");
        return;
    }
    // Each open rebuilds the rows. The old row is freed while the item still
    // owns its graphic, so the next row must not detach it from that row.
    for (int round = 0; round < 3; ++round) {
        ClickNode(*scene, *title);
        scene->layout(720, 420, 0);
        jadefx::Node* row = scene->getElementById("New");
        Expect(file->isShowing(), "the menu reopens");
        Expect(row != nullptr && icon->getParent() == row, "the reopened row owns the graphic");
        if (row != nullptr) {
            ClickNode(*scene, *row);
            scene->layout(720, 420, 0);
        }
    }
    Expect(!file->isShowing(), "the last click hides the menu");
}

void TestMenuShowAtPoint() {
    auto menu = jadefx::make<jadefx::Menu>();
    int fires = 0;
    auto item = jadefx::make<jadefx::MenuItem>("Rename");
    item->setOnAction([&](jadefx::ActionEvent&) { ++fires; });
    menu->getItems().add(item);
    auto root = FullRoot();
    auto scene = jadefx::make<jadefx::Scene>(root, 720, 420);
    scene->layout(720, 420, 0);
    menu->show(*scene, 40, 50);
    Expect(menu->isShowing(), "show at a point opens the menu");
    std::vector<jadefx::Node*> popups = Popups(*scene);
    Expect(popups.size() == 1, "the point menu has one popup");
    if (popups.size() == 1) {
        Expect(std::abs(popups[0]->getX() - 40) < 0.5 && std::abs(popups[0]->getY() - 50) < 0.5,
               "the popup sits at the requested point");
        jadefx::Node* row = scene->getElementById("Rename");
        Expect(row != nullptr, "the row is in the popup");
        if (row != nullptr) {
            ClickNode(*scene, *row);
        }
        Expect(fires == 1, "the point menu runs its item");
        Expect(!menu->isShowing(), "the point menu hides after the item runs");
    }

    menu->show(*scene, 700, 400);
    popups = Popups(*scene);
    Expect(menu->isShowing() && popups.size() == 1, "a second show at a point opens again");
    if (popups.size() == 1) {
        Expect(popups[0]->getX() >= -0.5 && popups[0]->getY() >= -0.5, "the popup stays on the scene");
        Expect(popups[0]->getX() + popups[0]->getWidth() <= scene->getWidth() + 0.5, "the popup stays inside the width");
        Expect(popups[0]->getY() + popups[0]->getHeight() <= scene->getHeight() + 0.5, "the popup stays inside the height");
    }
    scene->noteButton(0, true, 8, 8);
    Expect(!menu->isShowing(), "a press outside the point menu hides it");
}

}  // namespace

int RunMenuTests() {
    TestMenuItemBasics();
    TestMenuButtonActivatesItem();
    TestMenuButtonToggleAndOutsidePress();
    TestSeparatorAndDisabled();
    TestMenuBarSwitchesOnMove();
    TestSubmenuOpensRight();
    TestAcceleratorWhileClosed();
    TestRebuildWhileOpen();
    TestLeavingScene();
    TestEscapeHidesMenu();
    TestMenuItemGraphic();
    TestGraphicSurvivesReopen();
    TestMenuShowAtPoint();
    return gFailures;
}

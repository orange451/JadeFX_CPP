#include "jadefx/jadefx.hpp"

#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

constexpr double kWidth = 240;
constexpr double kHeight = 300;
constexpr int kShift = jadefx::Key::ModShift;
constexpr int kControl = jadefx::Key::ModControl;

// Five rows under a hidden root: A, B, C, D, E.
struct Rig {
    std::shared_ptr<jadefx::TreeItem> root = jadefx::make<jadefx::TreeItem>("Root");
    std::vector<std::shared_ptr<jadefx::TreeItem>> rows;
    std::shared_ptr<jadefx::TreeView> tree;
    std::shared_ptr<jadefx::Scene> scene;
    int itemChanges = 0;

    explicit Rig(jadefx::SelectionMode mode) {
        for (const char* name : {"A", "B", "C", "D", "E"}) {
            rows.push_back(jadefx::make<jadefx::TreeItem>(name));
            root->getChildren().add(rows.back());
        }
        root->setExpanded(true);
        tree = jadefx::make<jadefx::TreeView>(root);
        tree->setShowRoot(false);
        tree->setSelectionMode(mode);
        tree->setOnSelectedItemsChanged([this] { ++itemChanges; });
        scene = jadefx::make<jadefx::Scene>(tree, kWidth, kHeight);
        frame();
    }

    void frame() { scene->layout(kWidth, kHeight); }

    // Left click in the middle of row index, clear of the disclosure arrow.
    void click(int index, int mods = 0, int button = 0) {
        frame();
        jadefx::Node* cell = tree->getCell(rows[static_cast<std::size_t>(index)].get());
        Expect(cell != nullptr, "the row to click is on screen");
        if (cell == nullptr) {
            return;
        }
        const double x = cell->getAbsoluteX() + cell->getWidth() * 0.5;
        const double y = cell->getAbsoluteY() + cell->getHeight() * 0.5;
        scene->noteButton(button, true, x, y, mods);
        scene->noteButton(button, false, x, y, mods);
    }

    std::vector<jadefx::TreeItem*> pick(std::initializer_list<int> indexes) const {
        std::vector<jadefx::TreeItem*> out;
        for (int index : indexes) {
            out.push_back(rows[static_cast<std::size_t>(index)].get());
        }
        return out;
    }

    bool drawnSelected(int index) {
        frame();
        jadefx::Node* cell = tree->getCell(rows[static_cast<std::size_t>(index)].get());
        return cell != nullptr && cell->isSelected();
    }
};

void TestSingleIgnoresModifiers() {
    Rig rig(jadefx::SelectionMode::Single);
    rig.click(0);
    rig.click(2, kControl);
    Expect(rig.tree->getSelectedItems() == rig.pick({2}), "Single mode keeps one row on a Ctrl click");
    rig.click(4, kShift);
    Expect(rig.tree->getSelectedItems() == rig.pick({4}), "Single mode keeps one row on a Shift click");
    rig.tree->selectItems(rig.pick({0, 1}));
    Expect(rig.tree->getSelectedItems() == rig.pick({1}), "selectItems in Single mode keeps the last");
}

void TestControlClickToggles() {
    Rig rig(jadefx::SelectionMode::Multiple);
    rig.click(0);
    rig.click(2, kControl);
    Expect(rig.tree->getSelectedItems() == rig.pick({0, 2}), "Ctrl and a click adds a row");
    Expect(rig.tree->getSelectedItem() == rig.rows[2].get(), "the added row is the selected item");
    Expect(rig.drawnSelected(0) && rig.drawnSelected(2) && !rig.drawnSelected(1), "each selected row is drawn selected");
    rig.click(3, jadefx::Key::ModSuper);
    Expect(rig.tree->getSelectedItems() == rig.pick({0, 2, 3}), "Command and a click adds a row too");
    rig.click(2, kControl);
    Expect(rig.tree->getSelectedItems() == rig.pick({0, 3}), "Ctrl and a click on a selected row removes it");
    Expect(rig.tree->getSelectedItem() == rig.rows[3].get(), "removing a row picks the last one left");
    Expect(!rig.drawnSelected(2), "the removed row is no longer drawn selected");
    rig.click(1);
    Expect(rig.tree->getSelectedItems() == rig.pick({1}), "a plain click selects one row");
}

void TestShiftClickSelectsRange() {
    Rig rig(jadefx::SelectionMode::Multiple);
    rig.click(3);
    rig.click(1, kShift);
    Expect(rig.tree->getSelectedItems() == rig.pick({3, 2, 1}), "Shift selects from the anchor to the row");
    Expect(rig.tree->getSelectedItem() == rig.rows[1].get(), "the clicked row is the selected item");
    rig.click(4, kShift);
    Expect(rig.tree->getSelectedItems() == rig.pick({3, 4}), "a second Shift click keeps the same anchor");
    rig.click(0, kControl);
    rig.click(1, kControl | kShift);
    Expect(rig.tree->getSelectedItems() == rig.pick({3, 4, 0, 1}), "Ctrl and Shift adds a range to the selection");
}

void TestModifiedClicksDoNotActivate() {
    Rig rig(jadefx::SelectionMode::Multiple);
    int activations = 0;
    rig.tree->setOnItemActivated([&](jadefx::TreeItem&) {
        ++activations;
        return true;
    });
    rig.click(0, kControl);
    rig.click(0, kControl);
    Expect(activations == 0, "two Ctrl clicks are not a double-click");
    rig.click(1);
    rig.click(1);
    Expect(activations == 1, "two plain clicks still activate");
}

void TestRightClickKeepsSelection() {
    Rig rig(jadefx::SelectionMode::Multiple);
    int menus = 0;
    rig.tree->setOnContextMenuRequested([&](jadefx::TreeItem&, const jadefx::MouseEvent&) { ++menus; });
    rig.click(0);
    rig.click(2, kControl);
    rig.click(0, 0, 1);
    Expect(menus == 1, "a right-click asks for the menu");
    Expect(rig.tree->getSelectedItems() == rig.pick({0, 2}), "a right-click on a selected row keeps the others");
    Expect(rig.tree->getSelectedItem() == rig.rows[0].get(), "that row becomes the selected item");
    rig.click(4, 0, 1);
    Expect(rig.tree->getSelectedItems() == rig.pick({4}), "a right-click on another row selects only it");
}

void TestShiftArrowExtends() {
    Rig rig(jadefx::SelectionMode::Multiple);
    rig.click(1);
    jadefx::KeyEvent down;
    down.key = jadefx::Key::Down;
    down.pressed = true;
    down.shift = true;
    rig.tree->handleKey(down);
    jadefx::KeyEvent again = down;
    rig.tree->handleKey(again);
    Expect(rig.tree->getSelectedItems() == rig.pick({1, 2, 3}), "Shift and Down grows the range");
    jadefx::KeyEvent plain;
    plain.key = jadefx::Key::Up;
    plain.pressed = true;
    rig.tree->handleKey(plain);
    Expect(rig.tree->getSelectedItems() == rig.pick({2}), "a plain arrow selects one row");
}

void TestArrowDoesNotSelect() {
    Rig rig(jadefx::SelectionMode::Multiple);
    rig.rows[1]->getChildren().add(jadefx::make<jadefx::TreeItem>("B1"));
    rig.click(3);
    rig.itemChanges = 0;
    rig.frame();
    jadefx::Node* cell = rig.tree->getCell(rig.rows[1].get());
    Expect(cell != nullptr, "the branch row is on screen");
    if (cell == nullptr) {
        return;
    }
    // Walk across the row until the point lands on the arrow.
    const double y = cell->getAbsoluteY() + cell->getHeight() * 0.5;
    double x = -1;
    for (double at = cell->getAbsoluteX(); at < cell->getAbsoluteX() + cell->getWidth() && x < 0; at += 1) {
        jadefx::Node* hit = rig.tree->pick(at, y);
        if (hit != nullptr && std::string_view(hit->getElementType()) == "tree-disclosure-node") {
            x = at + 2;
        }
    }
    Expect(x >= 0, "the branch row has an arrow");
    if (x < 0) {
        return;
    }
    rig.scene->noteButton(0, true, x, y, 0);
    rig.scene->noteButton(0, false, x, y, 0);
    Expect(rig.rows[1]->isExpanded(), "the arrow expands the row");
    Expect(rig.tree->getSelectedItems() == rig.pick({3}), "the arrow leaves the selection alone");
    Expect(rig.itemChanges == 0, "the arrow does not notify a selection change");
    rig.scene->noteButton(0, true, x, y, 0);
    rig.scene->noteButton(0, false, x, y, 0);
    Expect(!rig.rows[1]->isExpanded(), "a second arrow click collapses the row");
    Expect(rig.tree->getSelectedItems() == rig.pick({3}), "collapsing leaves the selection alone");
}

void TestSelectItemsAndRemoval() {
    Rig rig(jadefx::SelectionMode::Multiple);
    rig.itemChanges = 0;
    rig.tree->selectItems(rig.pick({4, 1, 4}));
    Expect(rig.tree->getSelectedItems() == rig.pick({4, 1}), "selectItems drops a repeat");
    Expect(rig.tree->getSelectedItem() == rig.rows[1].get(), "the last item is the selected item");
    Expect(rig.itemChanges == 1, "selectItems notifies once");
    rig.tree->selectItems(rig.pick({4, 1}));
    Expect(rig.itemChanges == 1, "the same selection does not notify");
    Expect(rig.drawnSelected(4) && rig.drawnSelected(1), "selectItems draws every row");

    rig.root->getChildren().removeIf(
        [&](const std::shared_ptr<jadefx::TreeItem>& child) { return child.get() == rig.rows[1].get(); });
    Expect(rig.tree->getSelectedItems() == rig.pick({4}), "a removed row leaves the selection");
    Expect(rig.tree->getSelectedItem() == rig.rows[4].get(), "the selected item moves to a row still there");
    Expect(rig.itemChanges == 2, "the removal notifies");

    rig.tree->setSelectionMode(jadefx::SelectionMode::Single);
    rig.tree->selectItems(rig.pick({0, 2}));
    rig.tree->setSelectionMode(jadefx::SelectionMode::Multiple);
    rig.tree->selectItems(rig.pick({0, 2}));
    rig.tree->setSelectionMode(jadefx::SelectionMode::Single);
    Expect(rig.tree->getSelectedItems() == rig.pick({2}), "switching to Single keeps the selected item");
    rig.tree->clearSelection();
    Expect(rig.tree->getSelectedItems().empty() && rig.tree->getSelectedItem() == nullptr, "clearSelection empties it");
}

}  // namespace

int RunTreeViewTests() {
    TestSingleIgnoresModifiers();
    TestControlClickToggles();
    TestShiftClickSelectsRange();
    TestModifiedClicksDoNotActivate();
    TestRightClickKeepsSelection();
    TestShiftArrowExtends();
    TestArrowDoesNotSelect();
    TestSelectItemsAndRemoval();
    return gFailures;
}

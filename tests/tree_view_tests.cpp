#include "jadefx/jadefx.hpp"

#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
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

// Rows A, B, B1, B2, C under a hidden root. B is open and holds B1 and B2.
struct DragRig {
    std::shared_ptr<jadefx::TreeItem> root = jadefx::make<jadefx::TreeItem>("Root");
    std::shared_ptr<jadefx::TreeItem> a = jadefx::make<jadefx::TreeItem>("A");
    std::shared_ptr<jadefx::TreeItem> b = jadefx::make<jadefx::TreeItem>("B");
    std::shared_ptr<jadefx::TreeItem> b1 = jadefx::make<jadefx::TreeItem>("B1");
    std::shared_ptr<jadefx::TreeItem> b2 = jadefx::make<jadefx::TreeItem>("B2");
    std::shared_ptr<jadefx::TreeItem> c = jadefx::make<jadefx::TreeItem>("C");
    std::shared_ptr<jadefx::TreeView> tree;
    std::shared_ptr<jadefx::Scene> scene;
    std::vector<jadefx::TreeDrop> drops;
    int activations = 0;

    explicit DragRig(bool droppable = true) {
        b->getChildren().add(b1);
        b->getChildren().add(b2);
        b->setExpanded(true);
        root->getChildren().add(a);
        root->getChildren().add(b);
        root->getChildren().add(c);
        root->setExpanded(true);
        tree = jadefx::make<jadefx::TreeView>(root);
        tree->setShowRoot(false);
        tree->setSelectionMode(jadefx::SelectionMode::Multiple);
        tree->setOnItemActivated([this](jadefx::TreeItem&) {
            ++activations;
            return true;
        });
        if (droppable) {
            tree->setOnItemsDropped([this](const jadefx::TreeDrop& drop) { drops.push_back(drop); });
        }
        scene = jadefx::make<jadefx::Scene>(tree, kWidth, kHeight);
        frame();
    }

    void frame() { scene->layout(kWidth, kHeight); }

    // A point on item's row: along is 0 at its top and 1 at its bottom, and
    // x is from the row's left edge. A negative x is the middle of the row.
    std::pair<double, double> at(const jadefx::TreeItem* item, double along, double x = -1) {
        frame();
        jadefx::Node* cell = tree->getCell(item);
        Expect(cell != nullptr, "the row to drag over is on screen");
        if (cell == nullptr) {
            return {0, 0};
        }
        const double left = x < 0 ? cell->getWidth() * 0.5 : x;
        return {cell->getAbsoluteX() + left, cell->getAbsoluteY() + cell->getHeight() * along};
    }

    void press(const jadefx::TreeItem* item, int mods = 0) {
        const auto [x, y] = at(item, 0.5);
        scene->noteButton(0, true, x, y, mods);
    }

    void move(std::pair<double, double> point) {
        scene->noteMove(point.first, point.second);
        frame();
    }

    void release(std::pair<double, double> point) { scene->noteButton(0, false, point.first, point.second, 0); }

    void drag(const jadefx::TreeItem* from, std::pair<double, double> to) {
        press(from);
        move(to);
        release(to);
    }
};

bool IsDrop(const jadefx::TreeDrop& drop, std::vector<jadefx::TreeItem*> items, const jadefx::TreeItem* target,
            jadefx::TreeDropPosition position) {
    return drop.items == items && drop.target == target && drop.position == position;
}

void TestDragIsOffWithoutHandler() {
    DragRig rig(false);
    rig.drag(rig.a.get(), rig.at(rig.c.get(), 0.5));
    Expect(!rig.tree->isDraggingItems(), "a tree without a drop handler never drags");
    rig.press(rig.a.get());
    rig.move(rig.at(rig.a.get(), 0.9));
    Expect(!rig.tree->isDraggingItems(), "a drag stays off without a drop handler");
    rig.release(rig.at(rig.a.get(), 0.9));
    Expect(rig.tree->getSelectedItems() == std::vector<jadefx::TreeItem*>{rig.a.get()},
           "without a drop handler, a press that wanders on its row is still a click");
}

void TestDropZones() {
    using Pos = jadefx::TreeDropPosition;
    DragRig rig;
    rig.press(rig.a.get());
    rig.move(rig.at(rig.a.get(), 0.55));
    Expect(!rig.tree->isDraggingItems(), "a small wobble is not a drag");
    rig.move(rig.at(rig.c.get(), 0.5));
    Expect(rig.tree->isDraggingItems(), "moving past the hysteresis starts a drag");
    Expect(rig.tree->getSelectedItems() == std::vector<jadefx::TreeItem*>{rig.a.get()},
           "a drag from an unselected row selects it");
    Expect(IsDrop(rig.tree->getPendingDrop(), {rig.a.get()}, rig.c.get(), Pos::Into), "the middle of a row is Into");
    Expect(rig.tree->getPendingDrop().parent() == rig.c.get(), "Into's parent is the target");
    rig.move(rig.at(rig.c.get(), 0.1, 2));
    Expect(IsDrop(rig.tree->getPendingDrop(), {rig.a.get()}, rig.b.get(), Pos::After),
           "the top of a row under a deeper row picks the shallow level at the left");
    rig.move(rig.at(rig.c.get(), 0.9));
    Expect(IsDrop(rig.tree->getPendingDrop(), {rig.a.get()}, rig.c.get(), Pos::After), "the bottom of a row is After");
    rig.move(rig.at(rig.b.get(), 0.9));
    Expect(IsDrop(rig.tree->getPendingDrop(), {rig.a.get()}, rig.b1.get(), Pos::Before),
           "the bottom of an open branch is above its first child");
    rig.move(rig.at(rig.b1.get(), 0.1));
    Expect(IsDrop(rig.tree->getPendingDrop(), {rig.a.get()}, rig.b1.get(), Pos::Before),
           "the top of a first child is Before it");
    Expect(rig.tree->getPendingDrop().parent() == rig.b.get(), "Before's parent is the target's parent");
    rig.move(rig.at(rig.b2.get(), 0.9, 2));
    Expect(IsDrop(rig.tree->getPendingDrop(), {rig.a.get()}, rig.b.get(), Pos::After),
           "under a branch's last row, the far left is After the branch");
    rig.move(rig.at(rig.b2.get(), 0.9, 40));
    Expect(IsDrop(rig.tree->getPendingDrop(), {rig.a.get()}, rig.b2.get(), Pos::After),
           "under a branch's last row, the right is After that row");
    const auto last = rig.at(rig.c.get(), 0.5);
    const auto below = std::make_pair(last.first, last.second + 40);
    rig.move(below);
    Expect(IsDrop(rig.tree->getPendingDrop(), {rig.a.get()}, rig.c.get(), Pos::After),
           "below every row is After the last top-level row");
    rig.release(rig.at(rig.b.get(), 0.5));
    Expect(!rig.tree->isDraggingItems(), "the release ends the drag");
    Expect(rig.drops.size() == 1 && IsDrop(rig.drops[0], {rig.a.get()}, rig.b.get(), Pos::Into),
           "the release drops on the place under the pointer");
    Expect(rig.a->getParent() == rig.root.get(), "the view never moves the items itself");
}

void TestDragCarriesSelection() {
    using Pos = jadefx::TreeDropPosition;
    DragRig rig;
    rig.tree->selectItems({rig.c.get(), rig.b.get(), rig.b1.get()});
    rig.drag(rig.c.get(), rig.at(rig.a.get(), 0.5));
    Expect(rig.drops.size() == 1 && IsDrop(rig.drops[0], {rig.b.get(), rig.c.get()}, rig.a.get(), Pos::Into),
           "a selected row carries the selection in row order, without rows inside carried rows");
    Expect(rig.tree->getSelectedItems().size() == 3, "dragging a selected row keeps the selection");
}

void TestDropRefusals() {
    DragRig rig;
    rig.tree->selectItems({rig.b.get()});
    rig.press(rig.b.get());
    rig.move(rig.at(rig.b1.get(), 0.5));
    Expect(rig.tree->isDraggingItems(), "the drag starts");
    Expect(rig.tree->getPendingDrop().items.empty(), "a row cannot go inside its own child");
    rig.move(rig.at(rig.b.get(), 0.9));
    Expect(rig.tree->getPendingDrop().items.empty(), "a row cannot go beside its own child");
    rig.move(rig.at(rig.b.get(), 0.5));
    Expect(rig.tree->getPendingDrop().items.empty(), "a row cannot go into itself");
    Expect(rig.tree->cursorAt(0, 0) == jadefx::Cursor::NotAllowed, "a refused place shows NotAllowed");
    // The release lands on the pressed row, which would click it without the drag.
    rig.release(rig.at(rig.b.get(), 0.5));
    Expect(rig.drops.empty(), "a refused place drops nothing");
    rig.release(rig.at(rig.b.get(), 0.5));

    rig.tree->setDropAcceptor([&](const jadefx::TreeDrop& drop) { return drop.target != rig.c.get(); });
    rig.drag(rig.a.get(), rig.at(rig.c.get(), 0.5));
    Expect(rig.drops.empty(), "the acceptor can refuse a place");
    rig.drag(rig.a.get(), rig.at(rig.b2.get(), 0.5));
    Expect(rig.drops.size() == 1, "the acceptor lets other places through");
}

void TestDragDoesNotClick() {
    DragRig rig;
    rig.tree->selectItems({rig.a.get(), rig.c.get()});
    // Down and back up on the same row: a drag that ends where it began.
    rig.press(rig.c.get());
    rig.move(rig.at(rig.a.get(), 0.5));
    rig.move(rig.at(rig.c.get(), 0.4));
    rig.release(rig.at(rig.c.get(), 0.4));
    Expect(rig.tree->getSelectedItems().size() == 2, "the release after a drag does not click the row");
    rig.press(rig.c.get());
    rig.release(rig.at(rig.c.get(), 0.5));
    rig.press(rig.c.get());
    rig.release(rig.at(rig.c.get(), 0.5));
    Expect(rig.activations == 1, "clicks after a drag still double-click");
}

void TestEscapeCancelsDrag() {
    DragRig rig;
    rig.press(rig.a.get());
    rig.move(rig.at(rig.c.get(), 0.5));
    Expect(rig.tree->isDraggingItems(), "the drag starts");
    rig.scene->noteKey(jadefx::Key::Escape, true, false, 0);
    Expect(!rig.tree->isDraggingItems(), "Escape ends the drag");
    rig.move(rig.at(rig.b.get(), 0.5));
    Expect(!rig.tree->isDraggingItems(), "the rest of that press drags nothing");
    rig.release(rig.at(rig.b.get(), 0.5));
    Expect(rig.drops.empty(), "a cancelled drag drops nothing");
    rig.drag(rig.a.get(), rig.at(rig.c.get(), 0.5));
    Expect(rig.drops.size() == 1, "the next press drags again");
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
    TestDragIsOffWithoutHandler();
    TestDropZones();
    TestDragCarriesSelection();
    TestDropRefusals();
    TestDragDoesNotClick();
    TestEscapeCancelsDrag();
    return gFailures;
}

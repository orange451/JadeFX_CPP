#include "jadefx/jadefx.hpp"

#include <cmath>
#include <cstdio>
#include <memory>
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

constexpr double kWidth = 240;
constexpr double kHeight = 300;
constexpr int kShift = jadefx::Key::ModShift;
constexpr int kControl = jadefx::Key::ModControl;

using StringList = jadefx::ListView<std::string>;

std::vector<std::string> Numbered(int count) {
    std::vector<std::string> items;
    for (int i = 0; i < count; ++i) {
        items.push_back("Item " + std::to_string(i));
    }
    return items;
}

// A list in a scene, with helpers to find, click, and type.
template <typename View>
struct Rig {
    std::shared_ptr<View> list = jadefx::make<View>();
    std::shared_ptr<jadefx::Scene> scene;

    Rig() {
        list->setPrefSize(kWidth, kHeight);
        auto root = jadefx::make<jadefx::Pane>();
        root->getChildren().add(list);
        scene = jadefx::make<jadefx::Scene>(root, 400, 400);
    }

    void frame() { scene->layout(400, 400, 0); }

    // Visible cells, in row order.
    std::vector<jadefx::IndexedCell*> cells() {
        frame();
        std::vector<jadefx::IndexedCell*> out;
        for (jadefx::Node* node : scene->getElementsByClassName("list-cell")) {
            auto* cell = dynamic_cast<jadefx::IndexedCell*>(node);
            if (cell != nullptr && cell->isVisible() && cell->getIndex() >= 0) {
                out.push_back(cell);
            }
        }
        std::sort(out.begin(), out.end(),
                  [](jadefx::IndexedCell* a, jadefx::IndexedCell* b) { return a->getIndex() < b->getIndex(); });
        return out;
    }

    jadefx::IndexedCell* cell(int index) {
        for (jadefx::IndexedCell* candidate : cells()) {
            if (candidate->getIndex() == index) {
                return candidate;
            }
        }
        return nullptr;
    }

    void click(int index, int mods = 0) {
        jadefx::IndexedCell* target = cell(index);
        Expect(target != nullptr, "the row to click is on screen");
        if (target == nullptr) {
            return;
        }
        const double x = target->getAbsoluteX() + 20;
        const double y = target->getAbsoluteY() + target->getHeight() * 0.5;
        scene->noteButton(0, true, x, y, mods);
        scene->noteButton(0, false, x, y, mods);
        frame();
    }

    void key(int code, int mods = 0) {
        scene->noteKey(code, true, false, mods);
        scene->noteKey(code, false, false, mods);
        frame();
    }
};

void TestVirtualRows() {
    Rig<StringList> rig;
    rig.list->getItems().setAll(Numbered(10000));
    std::vector<jadefx::IndexedCell*> shown = rig.cells();
    Expect(!shown.empty() && shown.front()->getIndex() == 0 && shown.front()->getText() == "Item 0",
           "the first row shows the first item");
    Expect(shown.size() < 30, "ten thousand items make only a screenful of cells");
    const std::size_t made = rig.scene->getElementsByClassName("list-cell").size();
    const double row = shown.empty() ? 0 : shown.front()->getHeight();
    Expect(row > 10 && row < 60, "a row measures to its text");
    Expect(!shown.empty() && std::fabs(shown.back()->getAbsoluteY() - rig.list->getAbsoluteY() -
                                       row * static_cast<double>(shown.back()->getIndex())) < 1.5,
           "rows stack at one row length apart");

    rig.list->scrollTo(5000);
    shown = rig.cells();
    Expect(!shown.empty() && shown.front()->getText() == "Item 5000", "scrollTo puts the row at the top");
    Expect(rig.scene->getElementsByClassName("list-cell").size() <= made + 1, "scrolling reuses the cells");

    rig.list->scrollTo(9999);
    shown = rig.cells();
    Expect(!shown.empty() && shown.back()->getText() == "Item 9999" &&
               shown.back()->getAbsoluteY() + shown.back()->getHeight() <= rig.list->getAbsoluteY() + kHeight + 0.5,
           "scrolling past the end stops with the last row at the bottom");

    rig.list->scrollTo(0);
    rig.frame();
    const double x = rig.list->getAbsoluteX() + 30;
    const double y = rig.list->getAbsoluteY() + 30;
    rig.scene->noteScroll(x, y, 0, -2);
    shown = rig.cells();
    Expect(!shown.empty() && shown.front()->getIndex() == static_cast<int>(80 / row),
           "the wheel scrolls 40 points a notch");
}

void TestMouseSelection() {
    Rig<StringList> rig;
    rig.list->getItems().setAll(Numbered(20));
    int changes = 0;
    rig.list->getSelectionModel().addListener([&] { ++changes; });
    rig.click(3);
    auto& model = rig.list->getSelectionModel();
    Expect(model.getSelectedIndex() == 3 && model.getSelectedItem() == std::string("Item 3"), "a click selects the row");
    Expect(changes == 1, "the click reports one selection change");
    Expect(rig.cell(3) != nullptr && rig.cell(3)->isSelected(), "the selected row's cell is :selected");
    Expect(rig.list->isFocused(), "a click on a row focuses the list");
    rig.click(5, kControl);
    Expect(model.getSelectedIndices().size() == 1 && model.getSelectedIndex() == 5,
           "Single mode ignores Ctrl and keeps one row");

    model.setSelectionMode(jadefx::SelectionMode::Multiple);
    rig.click(2);
    rig.click(4, kControl);
    Expect(model.isSelected(2) && model.isSelected(4) && model.getSelectedIndices().size() == 2,
           "Ctrl and a click adds a row");
    rig.click(2, kControl);
    Expect(!model.isSelected(2) && model.isSelected(4), "Ctrl and a click on a selected row removes it");
    rig.click(7, kShift);
    Expect(model.getSelectedIndices().size() == 4 && model.isSelected(4) && model.isSelected(7),
           "Shift and a click selects from the anchor");
    const std::vector<std::string> items = model.getSelectedItems();
    Expect(items.size() == 4 && items.front() == "Item 4" && items.back() == "Item 7",
           "getSelectedItems lists the range in order");
}

void TestKeyboard() {
    Rig<StringList> rig;
    rig.list->getItems().setAll(Numbered(100));
    auto& model = rig.list->getSelectionModel();
    rig.click(0);
    rig.key(jadefx::Key::Down);
    rig.key(jadefx::Key::Down);
    Expect(model.getSelectedIndex() == 2, "Down selects the next row");
    rig.key(jadefx::Key::Up);
    Expect(model.getSelectedIndex() == 1, "Up selects the row before");
    rig.key(jadefx::Key::End);
    Expect(model.getSelectedIndex() == 99, "End selects the last row");
    Expect(rig.cell(99) != nullptr, "the selection scrolls into view");
    rig.key(jadefx::Key::Home);
    Expect(model.getSelectedIndex() == 0 && rig.cell(0) != nullptr, "Home selects the first row and shows it");
    rig.key(jadefx::Key::PageDown);
    Expect(model.getSelectedIndex() > 3 && rig.cell(model.getSelectedIndex()) != nullptr,
           "Page Down moves by a page and keeps the row on screen");

    model.setSelectionMode(jadefx::SelectionMode::Multiple);
    rig.list->scrollTo(8);
    rig.click(10);
    rig.key(jadefx::Key::Down, kShift);
    rig.key(jadefx::Key::Down, kShift);
    Expect(model.getSelectedIndices().size() == 3 && model.isSelected(12), "Shift and Down extends the selection");
    rig.key(jadefx::Key::Down, kControl);
    Expect(rig.list->getFocusModel().getFocusedIndex() == 13 && !model.isSelected(13),
           "Ctrl and Down moves the focus alone");
    Expect(rig.cell(13) != nullptr && rig.cell(13)->pseudoState("focus-visible"),
           "the focused row is :focus-visible");
    rig.key(jadefx::Key::Space, kControl);
    Expect(model.isSelected(13) && model.getSelectedIndices().size() == 4, "Ctrl and Space adds the focused row");
    rig.key(jadefx::Key::A, kControl);
    Expect(model.getSelectedIndices().size() == 100, "Ctrl and A selects every row");
}

void TestItemChanges() {
    Rig<StringList> rig;
    rig.list->getItems().setAll(Numbered(5));
    auto& model = rig.list->getSelectionModel();
    rig.click(2);
    rig.list->getItems().insert(0, "New");
    rig.frame();
    Expect(model.getSelectedIndex() == 3 && model.getSelectedItem() == std::string("Item 2"),
           "an item added above keeps the same item selected");
    Expect(rig.cell(0) != nullptr && rig.cell(0)->getText() == "New", "the new row shows at once");
    rig.list->getItems().set(1, "Changed");
    Expect(rig.cell(1) != nullptr && rig.cell(1)->getText() == "Changed", "a replaced item shows its new text");
    rig.list->getItems().removeAt(3);
    Expect(model.getSelectedIndex() == -1 && model.isEmpty(), "removing the selected item clears the selection");

    auto placeholder = jadefx::make<jadefx::Label>("Nothing here");
    rig.list->setPlaceholder(placeholder);
    rig.list->getItems().clear();
    rig.frame();
    Expect(placeholder->isVisible() && placeholder->getWidth() > 0, "an empty list shows its placeholder");
    Expect(rig.cells().empty(), "an empty list shows no rows");

    auto shared = std::make_shared<jadefx::ObservableList<std::string>>();
    shared->add("Shared");
    rig.list->setItems(shared);
    auto other = jadefx::make<StringList>(shared);
    shared->add("Both see this");
    rig.frame();
    Expect(!placeholder->isVisible(), "the placeholder hides once there are rows");
    Expect(other->getItems().size() == 2 && rig.cell(1) != nullptr && rig.cell(1)->getText() == "Both see this",
           "two lists share one item list");
}

// Shows numbers with a prefix, to check updateItem runs for each row.
class HashCell : public jadefx::ListCell<int> {
protected:
    void updateItem(const int& item, bool empty) override {
        jadefx::ListCell<int>::updateItem(item, empty);
        setText(empty ? std::string() : "#" + std::to_string(item));
    }
};

void TestCellFactoryAndStyles() {
    Rig<jadefx::ListView<int>> rig;
    for (int i = 0; i < 50; ++i) {
        rig.list->getItems().add(i * 10);
    }
    std::vector<jadefx::IndexedCell*> shown = rig.cells();
    Expect(!shown.empty() && shown[1]->getText() == "10", "the default cell shows a number as text");
    rig.list->setCellFactory([](jadefx::ListView<int>&) { return std::make_shared<HashCell>(); });
    shown = rig.cells();
    Expect(!shown.empty() && shown[1]->getText() == "#10", "a cell factory draws the rows");

    rig.scene->setStylesheet(
        "list-cell:nth-child(odd) { background-color: #102030; } list-cell:nth-child(2n) { background-color: #405060; }");
    rig.frame();
    shown = rig.cells();
    auto shade = [](jadefx::IndexedCell* cell) { return cell->computedStyle().background.color; };
    Expect(shown.size() > 2 && shade(shown[0]).r < 0.1f && shade(shown[1]).r > 0.2f,
           ":nth-child(odd) is the first row, and :nth-child(2n) the second");
    rig.list->scrollTo(1);
    rig.frame();
    shown = rig.cells();
    Expect(!shown.empty() && shown[0]->getIndex() == 1 && shade(shown[0]).r > 0.2f,
           "a recycled cell takes its new row's parity");

    rig.list->setFixedCellSize(40);
    shown = rig.cells();
    Expect(!shown.empty() && std::fabs(shown[0]->getHeight() - 40) < 0.5, "fixedCellSize sets every row's length");

    rig.list->setOrientation(jadefx::Orientation::Horizontal);
    shown = rig.cells();
    Expect(shown.size() > 2 && shown[1]->getAbsoluteX() > shown[0]->getAbsoluteX() &&
               std::fabs(shown[1]->getAbsoluteY() - shown[0]->getAbsoluteY()) < 0.5,
           "a horizontal list runs left to right");
}

void TestNodeItems() {
    Rig<jadefx::ListView<std::shared_ptr<jadefx::Node>>> rig;
    auto swatch = jadefx::make<jadefx::Pane>();
    swatch->setPrefSize(30, 12);
    rig.list->getItems().add(swatch);
    rig.frame();
    Expect(swatch->getParent() != nullptr && swatch->getWidth() > 0, "a Node item is the cell's graphic");
}

void TestEditing() {
    Rig<StringList> rig;
    rig.list->getItems().setAll(Numbered(10));
    rig.list->setCellFactory(jadefx::TextFieldListCell<std::string>::forListView());
    int commits = 0;
    int cancels = 0;
    rig.list->setOnEditCancel([&](const StringList::EditEvent&) { ++cancels; });

    // Not editable yet: a double-click only selects.
    rig.click(2);
    rig.click(2);
    Expect(rig.list->getEditingIndex() == -1, "a list that is not editable does not edit");

    rig.list->setEditable(true);
    rig.click(4);
    rig.click(4);
    Expect(rig.list->getEditingIndex() == 4, "a double-click edits the row");
    jadefx::IndexedCell* cell = rig.cell(4);
    auto* field = cell != nullptr ? dynamic_cast<jadefx::TextField*>(cell->getGraphic().get()) : nullptr;
    Expect(cell != nullptr && cell->isEditing() && field != nullptr && field->isFocused(),
           "the cell shows a focused text field");
    if (field != nullptr) {
        field->selectAll();
        rig.scene->noteText("Renamed");
        rig.key(jadefx::Key::Enter);
    }
    Expect(rig.list->getItems()[4] == "Renamed", "Enter commits the text into the list");
    Expect(rig.list->getEditingIndex() == -1 && rig.cell(4) != nullptr && !rig.cell(4)->isEditing() &&
               rig.cell(4)->getText() == "Renamed",
           "the row shows the new text");
    Expect(rig.list->isFocused(), "the list takes the keys back after the edit");

    rig.list->setOnEditCommit([&](const StringList::EditEvent& event) {
        ++commits;
        Expect(event.index == 5 && event.newValue == std::string("Kept out"), "the commit event names the row and value");
    });
    rig.click(5);
    rig.key(jadefx::Key::F2);
    Expect(rig.list->getEditingIndex() == 5, "F2 edits the focused row");
    rig.scene->noteText("Kept out");
    rig.key(jadefx::Key::Enter);
    Expect(commits == 1 && rig.list->getItems()[5] == "Item 5", "an onEditCommit handler decides what is stored");

    rig.click(6);
    rig.key(jadefx::Key::F2);
    rig.key(jadefx::Key::Escape);
    Expect(rig.list->getEditingIndex() == -1 && cancels == 1 && rig.cell(6)->getText() == "Item 6",
           "Escape cancels the edit");
    rig.key(jadefx::Key::F2);
    rig.click(8);
    Expect(rig.list->getEditingIndex() == -1 && cancels == 2, "clicking another row cancels the edit");
}

}  // namespace

int RunListViewTests() {
    TestVirtualRows();
    TestMouseSelection();
    TestKeyboard();
    TestItemChanges();
    TestCellFactoryAndStyles();
    TestNodeItems();
    TestEditing();
    if (gFailures == 0) {
        std::printf("list view tests passed\n");
    }
    return gFailures;
}

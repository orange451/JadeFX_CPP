#include "jadefx/jadefx.hpp"

#include <algorithm>
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

bool Near(double a, double b, double tolerance = 0.75) { return std::fabs(a - b) <= tolerance; }

struct Person {
    std::string name;
    int age = 0;
    bool operator==(const Person& other) const { return name == other.name && age == other.age; }
};

using Table = jadefx::TableView<Person>;
using NameColumn = jadefx::TableColumn<Person, std::string>;
using AgeColumn = jadefx::TableColumn<Person, int>;

struct Rig {
    std::shared_ptr<Table> table = jadefx::make<Table>();
    std::shared_ptr<NameColumn> name = jadefx::make<NameColumn>("Name");
    std::shared_ptr<AgeColumn> age = jadefx::make<AgeColumn>("Age");
    std::shared_ptr<jadefx::Scene> scene;

    explicit Rig(double width = 300) {
        name->setCellValueFactory([](const Person& person) { return person.name; });
        age->setCellValueFactory([](const Person& person) { return person.age; });
        table->getColumns().add(name);
        table->getColumns().add(age);
        table->getItems().setAll({{"Carol", 41}, {"Alice", 30}, {"Dan", 30}, {"Bob", 25}});
        table->setPrefSize(width, 300);
        auto root = jadefx::make<jadefx::Pane>();
        root->getChildren().add(table);
        scene = jadefx::make<jadefx::Scene>(root, 600, 400);
        frame();
    }

    void frame() { scene->layout(600, 400, 0); }

    std::vector<jadefx::IndexedCell*> rows() {
        frame();
        std::vector<jadefx::IndexedCell*> out;
        for (jadefx::Node* node : scene->getElementsByClassName("table-row-cell")) {
            auto* row = dynamic_cast<jadefx::IndexedCell*>(node);
            if (row != nullptr && row->isVisible() && row->getIndex() >= 0) {
                out.push_back(row);
            }
        }
        std::sort(out.begin(), out.end(), [](auto* a, auto* b) { return a->getIndex() < b->getIndex(); });
        return out;
    }

    // The cells of a row, left to right.
    std::vector<jadefx::IndexedCell*> cells(int index) {
        std::vector<jadefx::IndexedCell*> out;
        for (jadefx::IndexedCell* row : rows()) {
            if (row->getIndex() != index) {
                continue;
            }
            for (jadefx::Node* node : row->getElementsByClassName("table-cell")) {
                auto* cell = dynamic_cast<jadefx::IndexedCell*>(node);
                if (cell != nullptr && cell->getParent() == row) {
                    out.push_back(cell);
                }
            }
        }
        std::sort(out.begin(), out.end(), [](auto* a, auto* b) { return a->getX() < b->getX(); });
        return out;
    }

    jadefx::Node* header(const std::string& text) {
        frame();
        for (jadefx::Node* node : scene->getElementsByClassName("column-header")) {
            auto* labeled = dynamic_cast<jadefx::Labeled*>(node);
            if (labeled != nullptr && labeled->getText() == text) {
                return node;
            }
        }
        return nullptr;
    }

    void press(double x, double y, int mods = 0) { scene->noteButton(0, true, x, y, mods); }
    void release(double x, double y, int mods = 0) {
        scene->noteButton(0, false, x, y, mods);
        frame();
    }
    void click(jadefx::Node* node, int mods = 0, double dx = 20) {
        Expect(node != nullptr, "the node to click is on screen");
        if (node == nullptr) {
            return;
        }
        const double x = node->getAbsoluteX() + dx;
        const double y = node->getAbsoluteY() + node->getHeight() * 0.5;
        press(x, y, mods);
        release(x, y, mods);
    }
    void key(int code, int mods = 0) {
        scene->noteKey(code, true, false, mods);
        scene->noteKey(code, false, false, mods);
        frame();
    }

    std::vector<std::string> names() const {
        std::vector<std::string> out;
        for (const Person& person : table->getItems()) {
            out.push_back(person.name);
        }
        return out;
    }
};

void TestLayout() {
    Rig rig;
    jadefx::Node* nameHeader = rig.header("Name");
    jadefx::Node* ageHeader = rig.header("Age");
    Expect(nameHeader != nullptr && ageHeader != nullptr, "each column has a header");
    Expect(nameHeader != nullptr && std::string(nameHeader->getElementType()) == "th", "a header is a th");
    std::vector<jadefx::IndexedCell*> cells = rig.cells(0);
    Expect(cells.size() == 2 && cells[0]->getText() == "Carol" && cells[1]->getText() == "41",
           "a row shows each column's value");
    Expect(cells.size() == 2 && std::string(cells[0]->getElementType()) == "td" &&
               std::string(rig.rows()[0]->getElementType()) == "tr",
           "rows are tr and cells td");
    Expect(Near(rig.name->getWidth(), 80) && Near(rig.age->getWidth(), 80), "columns take their preferred width");
    Expect(cells.size() == 2 && ageHeader != nullptr && Near(cells[1]->getAbsoluteX(), ageHeader->getAbsoluteX()),
           "cells line up under their headers");

    rig.table->setColumnResizePolicy(jadefx::ColumnResizePolicy::Constrained);
    rig.name->setPrefWidth(200);
    rig.age->setPrefWidth(100);
    rig.frame();
    Expect(Near(rig.name->getWidth() + rig.age->getWidth(), 300) && Near(rig.name->getWidth(), 200),
           "a constrained table fills its width in proportion");

    rig.age->setVisible(false);
    Expect(rig.cells(0).size() == 1 && rig.header("Age") == nullptr, "a hidden column leaves the rows and the header");
    rig.age->setVisible(true);

    rig.table->getItems().clear();
    rig.frame();
    Expect(rig.rows().empty(), "an empty table shows no rows");
    auto* placeholder = dynamic_cast<jadefx::Label*>(rig.table->getPlaceholder().get());
    Expect(placeholder != nullptr && placeholder->isVisible() && placeholder->getText() == "No content in table",
           "an empty table says so");
}

void TestSorting() {
    Rig rig;
    rig.click(rig.cells(3)[0]);
    Expect(rig.table->getSelectionModel().getSelectedItem()->name == "Bob", "Bob is selected before the sort");
    rig.click(rig.header("Age"));
    Expect(rig.names() == std::vector<std::string>({"Bob", "Alice", "Dan", "Carol"}),
           "a header click sorts ascending, keeping ties in order");
    Expect(rig.table->getSelectionModel().getSelectedItem()->name == "Bob" &&
               rig.table->getSelectionModel().getSelectedIndex() == 0,
           "the selection follows its item through the sort");
    Expect(rig.header("Age")->pseudoState("ascending"), "the header shows the sort");
    rig.click(rig.header("Age"));
    Expect(rig.names() == std::vector<std::string>({"Carol", "Alice", "Dan", "Bob"}), "a second click sorts descending");
    rig.click(rig.header("Age"));
    Expect(rig.table->getSortOrder().empty() && !rig.header("Age")->pseudoState("sorted"),
           "a third click stops sorting by the column");

    rig.click(rig.header("Age"));
    rig.click(rig.header("Name"), jadefx::Key::ModShift);
    Expect(rig.table->getSortOrder().size() == 2, "Shift and a click adds a column to the sort");
    Expect(rig.names() == std::vector<std::string>({"Bob", "Alice", "Dan", "Carol"}),
           "ties on the first column sort by the second");
    rig.name->setSortType(jadefx::SortType::Descending);
    rig.table->sort();
    Expect(rig.names() == std::vector<std::string>({"Bob", "Dan", "Alice", "Carol"}), "each column keeps its own direction");

    rig.age->setSortable(false);
    rig.table->setSortOrder({});
    const std::vector<std::string> before = rig.names();
    rig.click(rig.header("Age"));
    Expect(rig.names() == before, "a column that is not sortable ignores the click");
}

void TestResizeAndReorder() {
    Rig rig;
    jadefx::Node* nameHeader = rig.header("Name");
    const double edge = nameHeader->getAbsoluteX() + nameHeader->getWidth() - 1;
    const double y = nameHeader->getAbsoluteY() + nameHeader->getHeight() * 0.5;
    Expect(rig.scene->pick(edge, y) == nameHeader && nameHeader->cursorAt(edge, y) == jadefx::Cursor::EwResize,
           "the header's right edge shows the resize cursor");
    rig.press(edge, y);
    rig.scene->noteMove(edge + 50, y);
    rig.release(edge + 50, y);
    Expect(Near(rig.name->getWidth(), 130), "dragging the edge widens the column");
    Expect(rig.names().front() == "Carol", "a resize does not sort");
    rig.press(nameHeader->getAbsoluteX() + nameHeader->getWidth() - 1, y);
    rig.scene->noteMove(nameHeader->getAbsoluteX() - 200, y);
    rig.release(nameHeader->getAbsoluteX() - 200, y);
    Expect(Near(rig.name->getWidth(), rig.name->getMinWidth()), "a column stops at its minimum width");

    rig.name->setPrefWidth(80);
    rig.click(rig.header("Age"));
    jadefx::Node* ageHeader = rig.header("Age");
    const double start = ageHeader->getAbsoluteX() + 20;
    rig.press(start, y);
    rig.scene->noteMove(start - 30, y);
    rig.scene->noteMove(rig.header("Name")->getAbsoluteX() + 5, y);
    rig.frame();
    Expect(rig.scene->getElementsByClassName("column-drag-marker").front()->isVisible(),
           "dragging a header shows where it will land");
    rig.release(rig.header("Name")->getAbsoluteX() + 5, y);
    Expect(rig.table->getColumns()[0] == rig.age && rig.cells(0)[0]->getText() == "25",
           "dropping a header moves its column");
    Expect(rig.table->getSortOrder().size() == 1, "moving a column keeps the sort");
}

void TestEditing() {
    Rig rig;
    rig.name->setCellFactory(jadefx::TextFieldTableCell<Person, std::string>::forTableColumn());
    rig.name->setCellValueSetter([](Person& person, const std::string& value) { person.name = value; });
    rig.table->setEditable(true);
    jadefx::IndexedCell* carol = rig.cells(0)[0];
    rig.click(carol);
    rig.click(carol);
    Expect(rig.table->getEditingCell().row == 0 && rig.table->getEditingCell().column == rig.name.get(),
           "a double-click edits the cell under it");
    rig.scene->noteText("Caroline");
    rig.key(jadefx::Key::Enter);
    Expect(rig.table->getItems()[0].name == "Caroline" && rig.table->getItems()[0].age == 41,
           "Enter writes the value back into the row item");
    Expect(rig.cells(0)[0]->getText() == "Caroline" && rig.table->isFocused(), "the cell shows it and the table has the keys");

    // The age column is not editable, so F2 on it edits the first editable column.
    rig.click(rig.cells(1)[1]);
    Expect(rig.table->getFocusedColumn() == rig.age.get(), "a click focuses the cell's column");
    rig.age->setEditable(false);
    rig.key(jadefx::Key::F2);
    Expect(rig.table->getEditingCell().row == 1 && rig.table->getEditingCell().column == rig.name.get(),
           "F2 edits the first column that can edit");
    rig.key(jadefx::Key::Escape);
    Expect(rig.table->getEditingCell().row == -1 && rig.table->getItems()[1].name == "Alice", "Escape cancels the edit");

    rig.key(jadefx::Key::Right);
    Expect(rig.table->getFocusedColumn() == rig.age.get(), "Right moves to the next column");
    rig.key(jadefx::Key::Left);
    Expect(rig.table->getFocusedColumn() == rig.name.get(), "Left moves back");
}

void TestWideTable() {
    Rig rig(200);
    for (int i = 0; i < 5; ++i) {
        auto extra = jadefx::make<AgeColumn>("Extra " + std::to_string(i));
        extra->setCellValueFactory([i](const Person& person) { return person.age + i; });
        rig.table->getColumns().add(extra);
    }
    rig.frame();
    jadefx::Node* nameHeader = rig.header("Name");
    const double before = nameHeader->getAbsoluteX();
    rig.scene->noteScroll(rig.table->getAbsoluteX() + 50, rig.table->getAbsoluteY() + 60, -2, 0);
    rig.frame();
    Expect(Near(nameHeader->getAbsoluteX(), before - 80), "a sideways scroll moves the header with the rows");
    Expect(Near(rig.cells(0)[0]->getAbsoluteX(), nameHeader->getAbsoluteX()), "cells stay under their headers");
}

}  // namespace

int RunTableViewTests() {
    TestLayout();
    TestSorting();
    TestResizeAndReorder();
    TestEditing();
    TestWideTable();
    if (gFailures == 0) {
        std::printf("table view tests passed\n");
    }
    return gFailures;
}

#include "jadefx/jadefx.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr const char* kStylesheet = R"CSS(
scene {
    font-family: "Open Sans";
    font-size: 14px;
}
.frame {
    background-color: var(--surface-color);
    width: calc(100% - 48px);
    height: calc(100% - 48px);
    border-radius: 8px;
    border-width: 1px;
    border-color: var(--border-color);
    box-shadow: 8px 16px 32px 0px rgba(0, 0, 0, 0.18);
    padding: 12px;
}
.list-view {
    border-width: 1px;
    border-color: var(--border-color);
}
list-cell:nth-child(even) {
    background-color: var(--subtle-color);
}
list-cell:selected {
    background-color: var(--selection-color);
}
.status {
    color: var(--muted-color);
    font-size: 13px;
}
.bar {
    spacing: 8px;
    alignment: center-left;
    padding: 0 0 8px 0;
}
button {
    border-width: 1px;
    border-color: var(--border-color);
    border-radius: 6px;
    padding: 4px 12px;
}
button:hover {
    background-color: var(--track-color);
}
tr:nth-child(even) {
    background-color: var(--subtle-color);
}
tr:selected {
    background-color: var(--selection-color);
}
th {
    color: var(--muted-color);
    font-size: 13px;
}
)CSS";

struct Employee {
    std::string name;
    std::string team;
    int age = 0;
    double rating = 0;
};

// Two thousand people, made up from a few names and teams.
std::shared_ptr<jadefx::TableView<Employee>> MakeTable() {
    static const char* const kFirst[] = {"Ada", "Ben", "Cleo", "Dev", "Emil", "Fay", "Gus", "Hana", "Ivo", "Jun"};
    static const char* const kLast[] = {"Park", "Nolan", "Silva", "Okafor", "Berg", "Ito", "Moreau"};
    static const char* const kTeams[] = {"Design", "Engine", "Support", "Sales", "Tools"};
    auto table = jadefx::make<jadefx::TableView<Employee>>();
    std::vector<Employee> people;
    for (int i = 0; i < 2000; ++i) {
        people.push_back({std::string(kFirst[i % 10]) + " " + kLast[(i / 10) % 7], kTeams[(i * 7) % 5], 22 + (i * 13) % 40,
                          static_cast<double>((i * 37) % 50) / 10.0});
    }
    table->getItems().setAll(std::move(people));

    auto name = jadefx::make<jadefx::TableColumn<Employee, std::string>>("Name");
    name->setCellValueFactory([](const Employee& person) { return person.name; });
    name->setCellValueSetter([](Employee& person, const std::string& value) { person.name = value; });
    name->setCellFactory(jadefx::TextFieldTableCell<Employee, std::string>::forTableColumn());
    name->setPrefWidth(150);
    auto team = jadefx::make<jadefx::TableColumn<Employee, std::string>>("Team");
    team->setCellValueFactory([](const Employee& person) { return person.team; });
    auto age = jadefx::make<jadefx::TableColumn<Employee, int>>("Age");
    age->setCellValueFactory([](const Employee& person) { return person.age; });
    age->setPrefWidth(60);
    auto rating = jadefx::make<jadefx::TableColumn<Employee, double>>("Rating");
    rating->setCellValueFactory([](const Employee& person) { return person.rating; });
    table->getColumns().add(name);
    table->getColumns().add(team);
    table->getColumns().add(age);
    table->getColumns().add(rating);
    table->setColumnResizePolicy(jadefx::ColumnResizePolicy::Constrained);
    table->setEditable(true);
    table->getSelectionModel().setSelectionMode(jadefx::SelectionMode::Multiple);
    return table;
}

class ListsApp : public jadefx::Application {
public:
    void start(jadefx::Stage& stage, int, char**) override {
        auto list = jadefx::make<jadefx::ListView<std::string>>();
        std::vector<std::string> names;
        for (int i = 1; i <= 10000; ++i) {
            names.push_back("Task " + std::to_string(i));
        }
        list->getItems().setAll(std::move(names));
        list->getSelectionModel().setSelectionMode(jadefx::SelectionMode::Multiple);
        list->setEditable(true);
        list->setCellFactory(jadefx::TextFieldListCell<std::string>::forListView());
        list->setPlaceholder(jadefx::make<jadefx::Label>("No tasks"));

        auto status = jadefx::make<jadefx::Label>("");
        status->getClassList().add("status");
        auto describe = [list, status] {
            const std::size_t count = list->getSelectionModel().getSelectedIndices().size();
            const std::string total = std::to_string(list->getItems().size()) + " tasks";
            status->setText(count == 0 ? total + ". Double-click or press F2 to rename one."
                                       : total + ", " + std::to_string(count) + " selected.");
        };
        list->getSelectionModel().addListener(describe);
        describe();

        auto add = jadefx::make<jadefx::Button>("Add");
        add->setOnAction([list, describe](jadefx::ActionEvent&) {
            list->getItems().insert(0, "New task");
            list->getSelectionModel().clearAndSelect(0);
            list->scrollTo(0);
            describe();
        });
        auto remove = jadefx::make<jadefx::Button>("Remove selected");
        remove->setOnAction([list, describe](jadefx::ActionEvent&) {
            std::vector<int> rows = list->getSelectionModel().getSelectedIndices();
            std::sort(rows.rbegin(), rows.rend());
            for (const int row : rows) {
                list->getItems().removeAt(static_cast<std::size_t>(row));
            }
            describe();
        });
        auto bar = jadefx::make<jadefx::HBox>();
        bar->getClassList().add("bar");
        bar->getChildren().add(add);
        bar->getChildren().add(remove);

        auto listPage = jadefx::make<jadefx::BorderPane>();
        listPage->setTop(bar);
        listPage->setCenter(list);
        listPage->setBottom(status);

        auto tabs = jadefx::make<jadefx::TabPane>();
        tabs->setTabClosingPolicy(jadefx::TabPane::TabClosingPolicy::Unavailable);
        tabs->getTabs().add(jadefx::make<jadefx::Tab>("List", listPage));
        tabs->getTabs().add(jadefx::make<jadefx::Tab>("Table", MakeTable()));

        auto frame = jadefx::make<jadefx::BorderPane>();
        frame->getClassList().add("frame");
        frame->setCenter(tabs);

        auto scene = jadefx::make<jadefx::Scene>(frame, 520, 560);
        scene->setStylesheet(kStylesheet);
        stage.setTitle("Lists");
        stage.setScene(scene);
    }
};

}  // namespace

int main(int argc, char** argv) {
    return jadefx::Application::launch(std::make_unique<ListsApp>(), argc, argv);
}

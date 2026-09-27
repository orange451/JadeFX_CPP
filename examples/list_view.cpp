#include "jadefx/jadefx.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr const char* kStylesheet = R"CSS(
scene {
    background-color: #eceff1;
    font-family: "Open Sans";
    font-size: 14px;
    color: #202124;
}
.frame {
    background-color: white;
    width: calc(100% - 48px);
    height: calc(100% - 48px);
    border-radius: 8px;
    border-width: 1px;
    border-color: #dadce0;
    box-shadow: 8px 16px 32px 0px rgba(0, 0, 0, 0.18);
    padding: 12px;
}
.list-view {
    border-width: 1px;
    border-color: #dadce0;
}
list-cell:nth-child(even) {
    background-color: #fafafa;
}
list-cell:selected {
    background-color: #e8f0fe;
}
.status {
    color: #5f6368;
    font-size: 13px;
}
.bar {
    spacing: 8px;
    alignment: center-left;
    padding: 0 0 8px 0;
}
button {
    background-color: white;
    border-width: 1px;
    border-color: #dadce0;
    border-radius: 6px;
    padding: 4px 12px;
}
button:hover {
    background-color: #f1f3f4;
}
)CSS";

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

        auto frame = jadefx::make<jadefx::BorderPane>();
        frame->getClassList().add("frame");
        frame->setTop(bar);
        frame->setCenter(list);
        frame->setBottom(status);

        auto scene = jadefx::make<jadefx::Scene>(frame, 440, 520);
        scene->setStylesheet(kStylesheet);
        stage.setTitle("Lists");
        stage.setScene(scene);
    }
};

}  // namespace

int main(int argc, char** argv) {
    return jadefx::Application::launch(std::make_unique<ListsApp>(), argc, argv);
}

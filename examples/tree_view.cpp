#include "jadefx/jadefx.hpp"

#include <initializer_list>
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
    padding: 8px 8px 12px 8px;
}
.tree-view {
    background-color: white;
}
tree-cell:hover {
    background-color: #f5f5f5;
}
tree-cell:selected {
    background-color: #e8f0fe;
}
tree-cell:selected:hover {
    background-color: #d2e3fc;
}
.selection-bar {
    background-color: #1a73e8;
}
tree-disclosure-node {
    color: #757575;
}
.tree-path {
    color: #5f6368;
    font-size: 13px;
    padding: 4px 12px 0 12px;
}
)CSS";

std::string PathOf(const jadefx::TreeItem* item) {
    if (item == nullptr) {
        return "Select a row";
    }
    std::vector<std::string> parts;
    for (const jadefx::TreeItem* cursor = item; cursor != nullptr; cursor = cursor->getParent()) {
        parts.push_back(cursor->getValue());
    }
    std::string path;
    for (std::size_t i = parts.size(); i-- > 0;) {
        if (!path.empty()) {
            path += "  /  ";
        }
        path += parts[i];
    }
    return path;
}

std::shared_ptr<jadefx::TreeItem> Branch(const char* name, std::initializer_list<const char*> people) {
    auto branch = jadefx::make<jadefx::TreeItem>(name);
    branch->setExpanded(true);
    for (const char* person : people) {
        branch->getChildren().add(jadefx::make<jadefx::TreeItem>(person));
    }
    return branch;
}

class TreeApp : public jadefx::Application {
public:
    void start(jadefx::Stage& stage, int, char**) override {
        auto root = jadefx::make<jadefx::TreeItem>("MyCompany Human Resources");
        root->setExpanded(true);
        root->getChildren().add(Branch("Accounts Department", {"Alice", "Bob", "Charlie"}));
        root->getChildren().add(Branch("Sales Department", {"Diana", "Ethan"}));
        root->getChildren().add(Branch("IT Support", {"Fiona", "George", "Hannah"}));
        root->getChildren().add(Branch("Operations", {"Ivan", "Julia"}));

        auto path = jadefx::make<jadefx::Label>("Select a row");
        path->getClassList().add("tree-path");

        auto tree = jadefx::make<jadefx::TreeView>(root);
        tree->setOnSelectionChanged([path](jadefx::TreeItem* item) { path->setText(PathOf(item)); });

        jadefx::TreeItem* sales = root->getChildren()[1].get();
        jadefx::TreeItem* diana = sales->getChildren()[0].get();
        tree->select(diana);
        path->setText(PathOf(diana));

        auto frame = jadefx::make<jadefx::BorderPane>();
        frame->getClassList().add("frame");
        frame->setSpacing(12);
        frame->setCenter(tree);
        frame->setBottom(path);

        auto scene = jadefx::make<jadefx::Scene>(frame, 440, 520);
        scene->setStylesheet(kStylesheet);
        stage.setTitle("Tree");
        stage.setScene(scene);
    }
};

}  // namespace

int main(int argc, char** argv) {
    return jadefx::Application::launch(std::make_unique<TreeApp>(), argc, argv);
}

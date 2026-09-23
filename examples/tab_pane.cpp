#include "jadefx/jadefx.hpp"

#include <memory>
#include <string>

namespace {

constexpr const char* kStylesheet = R"CSS(
scene {
    background-color: #e8eaed;
    font-family: "Open Sans";
    font-size: 16px;
    color: #202124;
}
.tab-pane {
    background-color: white;
    width: calc(100% - 48px);
    height: calc(100% - 48px);
    border-radius: 8px;
    border-width: 1px;
    border-color: rgb(160, 160, 160);
    box-shadow: 8px 16px 32px 0px rgba(0, 0, 0, 0.25);
}
tab {
    background-color: #e8eaed;
    border-radius: 8px 8px 0 0;
    color: #3c4043;
}
tab:selected {
    background-color: white;
    color: #1a73e8;
}
tab:hover {
    color: #174ea6;
}
.page {
    alignment: center;
    padding: 24px;
}
)CSS";

std::shared_ptr<jadefx::Tab> Page(const std::string& title, const std::string& body, const char* color) {
    auto page = jadefx::make<jadefx::StackPane>(jadefx::make<jadefx::Label>(body));
    page->getClassList().add("page");
    page->setStyle(std::string("background-color: ") + color + ";");
    return jadefx::make<jadefx::Tab>(title, page);
}

class TabPaneApp : public jadefx::Application {
public:
    void start(jadefx::Stage& stage, int, char**) override {
        auto tabs = jadefx::make<jadefx::TabPane>();
        tabs->getClassList().add("tab-pane");
        tabs->setTabClosingPolicy(jadefx::TabPane::TabClosingPolicy::AllTabs);
        tabs->getTabs().add(Page("Inbox", "Three unread notes", "#f8fbff"));
        tabs->getTabs().add(Page("Travel", "Train to the coast on Friday", "#f3fbf6"));
        tabs->getTabs().add(Page("Notes", "Buy more Open Sans ink", "#fff8f0"));

        auto scene = jadefx::make<jadefx::Scene>(tabs, 480, 360);
        scene->setStylesheet(kStylesheet);
        stage.setTitle("Tabs");
        stage.setScene(scene);
    }
};

}  // namespace

int main(int argc, char** argv) {
    return jadefx::Application::launch(std::make_unique<TabPaneApp>(), argc, argv);
}

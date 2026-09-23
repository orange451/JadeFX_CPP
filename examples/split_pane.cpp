#include "jadefx/jadefx.hpp"

#include <memory>

namespace {

// A horizontal split with a vertical split nested on the right. Each region is
// tinted, and the dividers can be dragged.
constexpr const char* kStylesheet = R"CSS(
scene {
    background-color: #e8eaed;
    font-family: "Open Sans";
    font-size: 16px;
    color: #202124;
}
.split-frame {
    width: calc(100% - 32px);
    height: calc(100% - 32px);
    background-color: white;
    border-radius: 8px;
    border-width: 1px;
    border-color: rgb(160, 160, 160);
}
split-pane:horizontal > .split-pane-divider,
split-pane:vertical > .split-pane-divider {
    padding: 0 3px;
    background-color: #d2d6dc;
}
.pane {
    alignment: center;
    padding: 16px;
}
.folders { background-color: #d2e3fc; }
.messages { background-color: #f1f3f4; }
.reading { background-color: #ceead6; }
.notes { background-color: #feefc3; }
)CSS";

std::shared_ptr<jadefx::StackPane> Pane(const char* text, const char* slot) {
    auto pane = jadefx::make<jadefx::StackPane>(jadefx::make<jadefx::Label>(text));
    pane->getClassList().add("pane");
    pane->getClassList().add(slot);
    return pane;
}

class SplitPaneApp : public jadefx::Application {
public:
    void start(jadefx::Stage& stage, int, char**) override {
        auto reading = jadefx::make<jadefx::SplitPane>();
        reading->setOrientation(jadefx::Orientation::Vertical);
        reading->getItems().add(Pane("Reading", "reading"));
        reading->getItems().add(Pane("Notes", "notes"));
        reading->setDividerPositions({0.62});

        auto split = jadefx::make<jadefx::SplitPane>();
        split->getClassList().add("split-frame");
        auto folders = Pane("Folders", "folders");
        jadefx::SplitPane::setResizableWithParent(*folders, false);
        folders->setMinSize(140, 0);
        split->getItems().add(folders);
        split->getItems().add(Pane("Messages", "messages"));
        split->getItems().add(reading);
        split->setDividerPositions({0.22, 0.58});

        auto scene = jadefx::make<jadefx::Scene>(split, 720, 460);
        scene->setStylesheet(kStylesheet);
        stage.setTitle("Split");
        stage.setScene(scene);
    }
};

}  // namespace

int main(int argc, char** argv) {
    return jadefx::Application::launch(std::make_unique<SplitPaneApp>(), argc, argv);
}

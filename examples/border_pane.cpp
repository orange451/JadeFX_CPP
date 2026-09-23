#include "jadefx/jadefx.hpp"

#include <memory>

namespace {

// The same window as JadeFX's BorderPaneTest. Each slot is tinted so the five
// regions are visible: top and bottom span the width, left and right fill the
// leftover height, and center takes what remains.
constexpr const char* kStylesheet = R"CSS(
scene {
    background-color: #e8eaed;
    font-family: "Open Sans";
    font-size: 16px;
    color: #202124;
}
.box {
    box-shadow: 8px 16px 32px 0px rgba(0, 0, 0, 0.3),
                2px 3px 6px 0px rgba(0, 0, 0, 0.1);
    width: calc(100% - 64px);
    height: calc(100% - 64px);
    background-color: white;
    border-radius: 8px;
    border-width: 1px;
    border-color: rgb(160, 160, 160);
    padding: 8px;
}
.region {
    alignment: center;
    padding: 12px 20px;
    border-radius: 4px;
}
.top { background-color: #d2e3fc; }
.bottom { background-color: #fce8e6; }
.left, .right { background-color: #ceead6; min-width: 96px; }
.right { background-color: #feefc3; }
.center { background-color: #f1f3f4; }
)CSS";

std::shared_ptr<jadefx::StackPane> Region(const char* text, const char* slot) {
    auto pane = jadefx::make<jadefx::StackPane>(jadefx::make<jadefx::Label>(text));
    pane->getClassList().add("region");
    pane->getClassList().add(slot);
    return pane;
}

class BorderPaneApp : public jadefx::Application {
public:
    void start(jadefx::Stage& stage, int, char**) override {
        auto layout = jadefx::make<jadefx::BorderPane>();
        layout->getClassList().add("box");
        layout->setTop(Region("Top", "top"));
        layout->setLeft(Region("Left", "left"));
        layout->setRight(Region("Right", "right"));
        layout->setBottom(Region("Bottom", "bottom"));
        layout->setCenter(Region("Center", "center"));

        auto scene = jadefx::make<jadefx::Scene>(layout, 480, 360);
        scene->setStylesheet(kStylesheet);
        stage.setTitle("BorderPane");
        stage.setScene(scene);
    }
};

}  // namespace

int main(int argc, char** argv) {
    return jadefx::Application::launch(std::make_unique<BorderPaneApp>(), argc, argv);
}

#include "jadefx/jadefx.hpp"

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

// Times JadeFX's styles-and-layout pass on a Studio-sized tree: about 2,000 nodes
// in nested boxes, under a stylesheet with descendant selectors and var(). Use the
// Release build: build/Release/jadefx-style-bench.exe
namespace {

using Clock = std::chrono::steady_clock;

constexpr int kFrames = 100;
constexpr double kWidth = 1600;
constexpr double kHeight = 900;
constexpr int kPanels = 6;
constexpr int kGroups = 5;
constexpr int kRows = 22;
constexpr int kButtons = 20;

const char* const kStylesheet = R"css(
:root { --panel-bg: #20242c; --accent: #3d7eff; --gap: 4px; --pad: 6px; }
.panel { background-color: var(--panel-bg); padding: var(--pad); spacing: var(--gap); border-width: 1px; border-color: #383c44; border-style: solid; }
.panel .row { padding: 2px 6px; spacing: 6px; }
.panel .row:hover { background-color: #2c313a; }
.panel .row .name { color: #d8dce4; font-size: 13px; }
.panel .row .value { color: var(--accent); }
.panel > .header { background-color: #2a2f38; font-size: 14px; }
.toolbar button { padding: 4px 8px; border-radius: 4px; transition: background-color 0.1s; }
.toolbar button:hover { background-color: #343a46; }
#explorer .row:nth-child(odd) { background-color: #23272f; }
label { font-family: "Open Sans"; }
)css";

struct Bench {
    std::shared_ptr<jadefx::Scene> scene;
    std::shared_ptr<jadefx::Button> first;
    std::shared_ptr<jadefx::Button> second;
    int nodes = 0;
};

template <typename T, typename... Args>
std::shared_ptr<T> Add(Bench& bench, jadefx::Pane& parent, Args&&... args) {
    auto node = jadefx::make<T>(std::forward<Args>(args)...);
    parent.getChildren().add(node);
    ++bench.nodes;
    return node;
}

Bench Build() {
    Bench bench;
    auto body = jadefx::make<jadefx::VBox>();
    bench.nodes = 1;
    auto toolbar = Add<jadefx::HBox>(bench, *body);
    toolbar->getClassList().add("toolbar");
    for (int i = 0; i < kButtons; ++i) {
        auto button = Add<jadefx::Button>(bench, *toolbar, "Tool " + std::to_string(i));
        if (i == 0) {
            bench.first = button;
        } else if (i == 1) {
            bench.second = button;
        }
    }
    auto workspace = Add<jadefx::HBox>(bench, *body);
    const char* const ids[kPanels] = {"explorer", "properties", "output", "assets", "console", "problems"};
    for (int p = 0; p < kPanels; ++p) {
        auto panel = Add<jadefx::VBox>(bench, *workspace);
        panel->getClassList().add("panel");
        panel->setElementId(ids[p]);
        auto header = Add<jadefx::Label>(bench, *panel, std::string(ids[p]));
        header->getClassList().add("header");
        for (int g = 0; g < kGroups; ++g) {
            auto group = Add<jadefx::VBox>(bench, *panel);
            group->getClassList().add("group");
            for (int r = 0; r < kRows; ++r) {
                auto row = Add<jadefx::HBox>(bench, *group);
                row->getClassList().add("row");
                auto name = Add<jadefx::Label>(bench, *row, "Item " + std::to_string(r));
                name->getClassList().add("name");
                auto value = Add<jadefx::Label>(bench, *row, std::to_string(r * 3) + " units");
                value->getClassList().add("value");
            }
        }
    }
    bench.scene = jadefx::make<jadefx::Scene>(body, kWidth, kHeight);
    bench.scene->setStylesheet(kStylesheet);
    return bench;
}

double Ms(Clock::duration duration) { return std::chrono::duration<double, std::milli>(duration).count(); }

}  // namespace

int main() {
    Bench bench = Build();
    double styles = 0;
    double layout = 0;
    double popups = 0;
    bool counting = false;
    Clock::time_point passStart;
    bench.scene->setLayoutPassHook([&](jadefx::LayoutPass pass, bool begin) {
        if (begin) {
            passStart = Clock::now();
            return;
        }
        if (!counting) {
            return;
        }
        const double spent = Ms(Clock::now() - passStart);
        switch (pass) {
            case jadefx::LayoutPass::Styles:
                styles += spent;
                break;
            case jadefx::LayoutPass::Layout:
                layout += spent;
                break;
            case jadefx::LayoutPass::Popups:
                popups += spent;
                break;
        }
    });

    double time = 0;
    const Clock::time_point firstStart = Clock::now();
    bench.scene->layout(kWidth, kHeight, time);
    const double first = Ms(Clock::now() - firstStart);

    counting = true;
    double steady = 0;
    for (int i = 0; i < kFrames; ++i) {
        time += 1.0 / 60.0;
        const Clock::time_point start = Clock::now();
        bench.scene->layout(kWidth, kHeight, time);
        steady += Ms(Clock::now() - start);
    }
    counting = false;

    const double firstX = bench.first->getAbsoluteX() + 4;
    const double firstY = bench.first->getAbsoluteY() + 4;
    const double secondX = bench.second->getAbsoluteX() + 4;
    const double secondY = bench.second->getAbsoluteY() + 4;
    double hover = 0;
    for (int i = 0; i < kFrames; ++i) {
        time += 1.0 / 60.0;
        if (i % 2 == 0) {
            bench.scene->noteMove(firstX, firstY);
        } else {
            bench.scene->noteMove(secondX, secondY);
        }
        const Clock::time_point start = Clock::now();
        bench.scene->layout(kWidth, kHeight, time);
        hover += Ms(Clock::now() - start);
    }

    std::printf("style-layout bench: %d nodes\n", bench.nodes);
    std::printf("  first frame  %8.3f ms\n", first);
    std::printf("  steady frame %8.3f ms (styles %.3f, layout %.3f, popups %.3f)\n", steady / kFrames,
                styles / kFrames, layout / kFrames, popups / kFrames);
    std::printf("  hover frame  %8.3f ms\n", hover / kFrames);
    return 0;
}

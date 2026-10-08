# JadeFX Style and Layout Performance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Cut the cost of Studio's "Styles and layout" frame zone by making JadeFX's full style and layout pass cheaper (Phase A), then skipping nodes whose style and layout did not change (Phase B), with no visible behavior change.

**Architecture:** Phase A keeps a full pass every frame and removes waste from it: a per-stylesheet selector index, declarations parsed once at load, pooled scratch buffers, a shared transition table, a per-pass measure cache, a theme-color cache, and one popup restyle per frame instead of two. Phase B adds per-node style and layout dirty flags, set by every input that can change a style or a layout, so an idle frame restyles and lays out nothing. A debug-only verification mode re-runs a full pass after each incremental one and aborts on any difference.

**Tech Stack:** C++17 on MSVC 14.23 (Visual Studio 16 2019 generator, CMake 3.16). JadeFX tests are plain functions with an `Expect` helper, run from `tests/layout_tests.cpp`'s `main`. AnarchyEngine tests run through `ctest`.

**Spec:** `docs/superpowers/specs/2026-10-08-jadefx-style-layout-perf-design.md`

## Global Constraints

- Visible behavior must not change. All existing JadeFX and AnarchyEngine tests pass unchanged, apart from the known flakes listed below.
- There is no hard budget. Before and after numbers come from the benchmark (`jadefx-style-bench`) and Studio's profiler, and they are recorded in commit messages: the baseline in Task 3, Phase A in Task 7, and the final numbers in Task 16.
- Phase A (Tasks 1–7) comes before Phase B (Tasks 8–16). Instrumentation and the benchmark come first, so a baseline is recorded before any optimization.
- Verification mode: in debug builds only (`#ifndef NDEBUG`), `JADEFX_VERIFY_INCREMENTAL=1` runs a full pass after each incremental pass and aborts with the node path and the differing field.
- Out of scope: rendering cost, multithreading, and changes to CSS feature coverage.
- Engine distances are called "units", never "studs", in code, comments, tests, and commit messages.
- Toolchain: code builds warning-free at `/W4`. From Git Bash, prefix cmake with `MSYS_NO_PATHCONV=1`. cmake is `"C:/Program Files/CMake/bin/cmake.exe"` and ctest is `"C:/Program Files/CMake/bin/ctest.exe"`. MSBuild rebuilds from timestamps, so `touch` a file restored from a copy. A float literal for FLT_MAX fails, so use `std::numeric_limits<float>::max()`.
- Branches: JadeFX work goes on `style-layout-perf`, created from `master` in Task 1. AnarchyEngine work goes on `jadefx-style-perf`, created in Task 2. Never push. Leave the engine's untracked `docs/superpowers/specs/2026-10-07-*.md` files alone.
- Comments follow the repo's style: plain sentences saying what the code does and why, with no task or plan references.
- Setters mark dirty only when the value actually changes. A setter that marks unconditionally makes idle frames do work, which the idle-frame test catches.
- **JadeFX build (Release):** `MSYS_NO_PATHCONV=1 "C:/Program Files/CMake/bin/cmake.exe" --build C:/Users/Andrew/Documents/GitHub/JadeFX_CPP/build --config Release --target jadefx-tests --parallel`
- **JadeFX tests (Release):** `cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && ./build/Release/jadefx-tests.exe`. It prints `layout tests passed`. The only failures allowed are these six known Release tree-view flakes: `tree-cell:selected paints the selected row`, `children of a hidden root still indent one level`, two scroll-rows tests, and two `:nth-child` parity tests.
- **Engine build (Release):** `MSYS_NO_PATHCONV=1 "C:/Program Files/CMake/bin/cmake.exe" --build C:/Users/Andrew/Documents/GitHub/AnarchyEngine-CPP/build --config Release --parallel`
- **Engine tests (Release):** `cd C:/Users/Andrew/Documents/GitHub/AnarchyEngine-CPP/build && "C:/Program Files/CMake/bin/ctest.exe" -C Release --output-on-failure`. The only known Release flake is studio-tests' `FAIL six frames recorded`. The known Debug flakes are the sandbox `[decomposition]` Q1–Q6 timeouts under load, and DS1/DS3 CloudCover.

## Review Focus

1. **A node changed outside a frame and then measured outside a frame**, for example `label->setText(...)` followed by `scene->showPopupNear(...)`. The popup must get the new size and not a cached one. Pinned by Task 7 `TestMeasureOutsideLayoutIsFresh` and Task 12 `TestPopupFollowsItsContent`.
2. **A descendant selector keyed on an ancestor's state**, such as `.item:hover .inner` or `.panel:focus-within`. The descendant must restyle when only the ancestor's state flips. Pinned by Task 10 `TestHoverRestylesDescendants` and Task 11 `TestFocusWithinFollowsFocus`.
3. **Layout driven by time**: an indeterminate ProgressBar, smooth scrolling, and spinner auto-repeat. These must keep moving on frames where nothing else changes. Pinned by Task 14 `TestIndeterminateBarKeepsMoving`.
4. **Nodes a control styles during layout with `applyCss`**, such as VirtualFlow cells rebound to new items. They must measure with their new content. Pinned by Task 14 `TestListViewFollowsItemChanges`.
5. **A runtime theme switch** (`Theme::setUserAgentStylesheet`) must restyle every scene that uses the global theme. Pinned by Task 10 `TestThemeSwitchRestylesEverything`.

---

## File map

| File | Responsibility |
|---|---|
| `include/jadefx/scene/Scene.hpp`, `src/scene/Scene.cpp` | `LayoutPass` hook; frame pass orchestration; incremental switch; theme generation; animating set; verification call |
| `include/jadefx/stage/Stage.hpp`, `src/stage/Stage.cpp` | Forward the layout-pass hook to the current scene |
| `include/jadefx/style/Style.hpp`, `src/style/Stylesheet.cpp` | `PropertyId`, `DeclarationValue`, pre-parsing, selector index, `TransitionTable`, enum dispatch |
| `include/jadefx/scene/Node.hpp`, `src/scene/Node.cpp` | Scratch pool, measure cache, theme-color cache, dirty flags, skip logic, invalidation setters, focus-within counter, transition end times |
| `include/jadefx/style/Theme.hpp`, `src/style/Theme.cpp` | `kThemeColorCount`, `Theme::generation()`, clearing the theme-color cache |
| `src/scene/IncrementalCheck.hpp/.cpp` (new) | Snapshot, compare, full-pass verify |
| `tests/style_layout_bench.cpp` (new) | Benchmark executable `jadefx-style-bench` |
| `tests/style_perf_tests.cpp` (new) | Phase A unit tests (`RunStylePerfTests`) |
| `tests/incremental_tests.cpp` (new) | Phase B unit tests (`RunIncrementalTests`) |
| `src/scene/controls/*.cpp`, `include/jadefx/scene/controls/*.hpp` | Invalidation calls in control setters and time-driven layouts (Task 14) |
| `AnarchyEngine-CPP/src/runner/UiFrameProfile.*`, `tests/UiFrameProfileTest.cpp` | Sub-zone mapping (Task 2) |
| `AnarchyEngine-CPP/src/ide/*`, `src/runner/GameView.hpp`, `src/runner/GuiLayer.hpp` | Invalidation calls in engine node subclasses (Task 15) |

---

### Task 1: Report layout sub-passes through a hook

**Files:**
- Modify: `include/jadefx/scene/Scene.hpp` (enum before `class Scene`; public setter; private member)
- Modify: `src/scene/Scene.cpp:112-147` (`Scene::layout`)
- Modify: `include/jadefx/stage/Stage.hpp:130-137`, `src/stage/Stage.cpp:63-81,112-116`
- Create: `tests/style_perf_tests.cpp`
- Modify: `CMakeLists.txt:348-385` (add the test file), `tests/layout_tests.cpp:1968,1988` (declare and run)

**Interfaces:**
- Produces: `enum class jadefx::LayoutPass { Styles, Layout, Popups };`, `using jadefx::LayoutPassHook = std::function<void(LayoutPass pass, bool begin)>;`, `void Scene::setLayoutPassHook(LayoutPassHook hook)`, `void Stage::setLayoutPassHook(LayoutPassHook hook)`, `void Stage::noteLayoutPass(LayoutPass pass, bool begin) const`, `int RunStylePerfTests()`.

- [ ] **Step 1: Create the branch**

```bash
cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && git checkout -b style-layout-perf
```

- [ ] **Step 2: Write the failing test file `tests/style_perf_tests.cpp`**

```cpp
#include "jadefx/jadefx.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

// The full style and layout pass, made cheaper without changing what it computes.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

using jadefx::LayoutPass;
using PassNote = std::pair<LayoutPass, bool>;

void TestScenePassesReported() {
    auto scene = jadefx::make<jadefx::Scene>(jadefx::make<jadefx::VBox>(), 200, 100);
    std::vector<PassNote> notes;
    scene->setLayoutPassHook([&](LayoutPass pass, bool begin) { notes.emplace_back(pass, begin); });
    scene->layout(200, 100, 0);
    const std::vector<PassNote> expected{{LayoutPass::Styles, true}, {LayoutPass::Styles, false},
                                         {LayoutPass::Layout, true}, {LayoutPass::Layout, false},
                                         {LayoutPass::Popups, true}, {LayoutPass::Popups, false}};
    Expect(notes == expected, "a scene layout reports styles, layout, and popups, each begun then ended");
}

void TestStageForwardsPasses() {
    jadefx::Stage stage;
    std::vector<PassNote> notes;
    stage.setLayoutPassHook([&](LayoutPass pass, bool begin) { notes.emplace_back(pass, begin); });
    stage.noteLayoutPass(LayoutPass::Popups, true);
    Expect(notes.size() == 1 && notes[0] == PassNote{LayoutPass::Popups, true}, "the stage hook hears a noted pass");

    auto scene = jadefx::make<jadefx::Scene>(jadefx::make<jadefx::Pane>(), 100, 100);
    stage.setScene(scene);
    notes.clear();
    scene->layout(100, 100, 0);
    Expect(notes.size() == 6, "a scene on the stage reports its passes to the stage hook");

    stage.setScene(jadefx::make<jadefx::Scene>(jadefx::make<jadefx::Pane>(), 100, 100));
    notes.clear();
    scene->layout(100, 100, 0);
    Expect(notes.empty(), "a scene the stage replaced stops reporting");
}

}  // namespace

int RunStylePerfTests() {
    TestScenePassesReported();
    TestStageForwardsPasses();
    return gFailures;
}
```

- [ ] **Step 3: Register the file and runner**

In `CMakeLists.txt`, add `tests/style_perf_tests.cpp` after the `tests/frame_phase_tests.cpp` line in the `add_executable(jadefx-tests ...)` list. In `tests/layout_tests.cpp`, add `int RunStylePerfTests();` after `int RunFramePhaseTests();`, and add `gFailures += RunStylePerfTests();` after `gFailures += RunFramePhaseTests();` in `main`.

- [ ] **Step 4: Build to verify it fails**

Run the JadeFX build command (Global Constraints).
Expected: compile errors `'LayoutPass': is not a member of 'jadefx'` and `'setLayoutPassHook': is not a member`.

- [ ] **Step 5: Add the hook to `Scene.hpp`**

Before `class Scene : public Node {`:

```cpp
// The parts of Scene::layout, for a host that times them inside FramePhase::Layout.
// Styles resolves the cascade, Layout places the root, and Popups places each popup.
enum class LayoutPass { Styles, Layout, Popups };
using LayoutPassHook = std::function<void(LayoutPass pass, bool begin)>;
```

In the public section, after `void setSafeInsets(...)`:

```cpp
    // Told when each LayoutPass of layout begins and ends. Stage sets this.
    void setLayoutPassHook(LayoutPassHook hook) { passHook_ = std::move(hook); }
```

In the private members, after `std::function<int()> eventPump_;`:

```cpp
    LayoutPassHook passHook_;
```

- [ ] **Step 6: Report the passes in `Scene::layout`**

In `src/scene/Scene.cpp`'s anonymous namespace, add:

```cpp
// Reports a layout pass from construction to destruction.
class PassScope {
public:
    PassScope(const LayoutPassHook& hook, LayoutPass pass) : hook_(hook), pass_(pass) {
        if (hook_) {
            hook_(pass_, true);
        }
    }
    ~PassScope() {
        if (hook_) {
            hook_(pass_, false);
        }
    }
    PassScope(const PassScope&) = delete;
    PassScope& operator=(const PassScope&) = delete;

private:
    const LayoutPassHook& hook_;
    LayoutPass pass_;
};
```

Replace the body of `Scene::layout(double width, double height, double timeSeconds)` up to the hover-popup update with:

```cpp
void Scene::layout(double width, double height, double timeSeconds) {
    {
        PassScope pass(passHook_, LayoutPass::Styles);
        applyStyles(rootInheritance(), timeSeconds);
    }

    x_ = 0;
    y_ = 0;
    width_ = std::max(0.0, width);
    height_ = std::max(0.0, height);
    lastWidth_ = width_;
    lastHeight_ = height_;
    lastTime_ = timeSeconds;
    laidOut_ = true;

    if (!internal_) {
        return;
    }
    {
        PassScope pass(passHook_, LayoutPass::Layout);
        const double right = computed_.padding.right + computed_.border.right;
        const double bottom = computed_.padding.bottom + computed_.border.bottom;
        const double x = safe_.left + contentLeft();
        const double y = safe_.top + contentTop();
        const double innerWidth = std::max(0.0, width_ - safe_.left - safe_.right - contentLeft() - right);
        const double innerHeight = std::max(0.0, height_ - safe_.top - safe_.bottom - contentTop() - bottom);
        internal_->performLayout(x, y, innerWidth, innerHeight);
    }
    {
        PassScope pass(passHook_, LayoutPass::Popups);
        for (std::size_t i = 0; i < popups_.size(); ++i) {
            layoutPopup(popups_[i]);
        }
    }
    if (pointerValid_) {
        updateHoverPopup(pick(pointerX_, pointerY_));
    }
    // A copy, since a listener may add or remove listeners.
    const auto listeners = pulseListeners_;
    for (const auto& listener : listeners) {
        if (listener.second) {
            listener.second();
        }
    }
}
```

- [ ] **Step 7: Forward from the Stage**

In `Stage.hpp`, after `void notePhase(FramePhase phase, bool begin) const;`:

```cpp
    // Told when each LayoutPass begins and ends, inside FramePhase::Layout. Given to
    // the current scene and to a scene installed later with setScene.
    void setLayoutPassHook(LayoutPassHook hook);
    // Reports a layout pass to the hook. Does nothing without one.
    void noteLayoutPass(LayoutPass pass, bool begin) const;
```

and in the private members, after `FramePhaseHook phaseHook_;`, add `LayoutPassHook passHook_;`.

In `Stage.cpp`, in `Stage::setScene`, add `scene_->setLayoutPassHook(nullptr);` inside the `if (scene_ && scene_ != scene)` block, and add `scene_->setLayoutPassHook(passHook_);` right after `scene_ = std::move(scene);`. After `Stage::notePhase`, add:

```cpp
void Stage::setLayoutPassHook(LayoutPassHook hook) {
    passHook_ = std::move(hook);
    if (scene_) {
        scene_->setLayoutPassHook(passHook_);
    }
}

void Stage::noteLayoutPass(LayoutPass pass, bool begin) const {
    if (passHook_) {
        passHook_(pass, begin);
    }
}
```

- [ ] **Step 8: Build and run the tests**

Run the JadeFX build and test commands.
Expected: `layout tests passed`, or only the six known tree-view flakes. `frame_phase_tests` stays green, because FramePhase notes are unchanged.

- [ ] **Step 9: Commit**

```bash
cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && git add include/jadefx/scene/Scene.hpp src/scene/Scene.cpp include/jadefx/stage/Stage.hpp src/stage/Stage.cpp tests/style_perf_tests.cpp tests/layout_tests.cpp CMakeLists.txt && git commit -m "Report the styles, layout, and popups passes of a scene layout through a hook

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Show the sub-passes in AnarchyEngine's profiler

**Files:**
- Modify: `C:/Users/Andrew/Documents/GitHub/AnarchyEngine-CPP/src/runner/UiFrameProfile.cpp`
- Modify: `C:/Users/Andrew/Documents/GitHub/AnarchyEngine-CPP/src/runner/UiFrameProfile.hpp` (comment)
- Test: `C:/Users/Andrew/Documents/GitHub/AnarchyEngine-CPP/tests/UiFrameProfileTest.cpp`

**Interfaces:**
- Consumes: `jadefx::LayoutPass`, `Stage::setLayoutPassHook`, `Stage::noteLayoutPass` (Task 1).
- Produces: profiler scopes named `Styles`, `Layout`, and `Popups` (Group::Engine), nested at depth 1 under `Styles and layout` on the UI row.

- [ ] **Step 1: Create the engine branch**

```bash
cd C:/Users/Andrew/Documents/GitHub/AnarchyEngine-CPP && git checkout -b jadefx-style-perf
```

- [ ] **Step 2: Write the failing test**

In `tests/UiFrameProfileTest.cpp`, insert this block before the final `profiler::release();` at the end of `RunUiFrameProfileTests()`:

```cpp
    // The layout phase's own passes nest under it, so the profiler shows where
    // the styles-and-layout time goes.
    at(40);
    profiler::frame_boundary();
    at(41);
    stage.notePhase(FramePhase::Layout, true);
    stage.noteLayoutPass(jadefx::LayoutPass::Styles, true);
    at(43);
    stage.noteLayoutPass(jadefx::LayoutPass::Styles, false);
    stage.noteLayoutPass(jadefx::LayoutPass::Layout, true);
    at(44);
    stage.noteLayoutPass(jadefx::LayoutPass::Layout, false);
    stage.noteLayoutPass(jadefx::LayoutPass::Popups, true);
    at(45);
    stage.noteLayoutPass(jadefx::LayoutPass::Popups, false);
    stage.notePhase(FramePhase::Layout, false);
    at(50);
    profiler::frame_boundary();
    profiler::collect();
    {
        const profiler::History history = live();
        expect(!history.frames.empty(), "a frame with layout passes");
        if (!history.frames.empty()) {
            const profiler::Frame& frame = history.frames.back();
            const profiler::ScopeRecord* outer = find(history, frame, "Styles and layout");
            const profiler::ScopeRecord* styles = find(history, frame, "Styles");
            const profiler::ScopeRecord* layout = find(history, frame, "Layout");
            const profiler::ScopeRecord* popups = find(history, frame, "Popups");
            expect(outer != nullptr && ms(*outer) == 4.0, "the layout phase is a 4 ms scope");
            expect(styles != nullptr && ms(*styles) == 2.0, "the styles pass is a 2 ms scope");
            expect(layout != nullptr && ms(*layout) == 1.0, "the layout pass is a 1 ms scope");
            expect(popups != nullptr && ms(*popups) == 1.0, "the popups pass is a 1 ms scope");
            expect(styles != nullptr && styles->depth == 1 && layout != nullptr && layout->depth == 1 &&
                       popups != nullptr && popups->depth == 1,
                   "the passes nest inside the layout phase");
        }
    }
```

- [ ] **Step 3: Build and run to verify it fails**

```bash
MSYS_NO_PATHCONV=1 "C:/Program Files/CMake/bin/cmake.exe" --build C:/Users/Andrew/Documents/GitHub/AnarchyEngine-CPP/build --config Release --target studio-tests --parallel && cd C:/Users/Andrew/Documents/GitHub/AnarchyEngine-CPP && ./build/Release/studio-tests.exe
```

Expected: `FAIL the styles pass is a 2 ms scope` and the other pass expectations fail, because no hook is installed yet.

- [ ] **Step 4: Map the passes**

In `UiFrameProfile.cpp`'s anonymous namespace, after `kPhaseCount`:

```cpp
profiler::ScopeId pass_scope(jadefx::LayoutPass pass) {
    using profiler::Group;
    static const profiler::ScopeId kStyles = profiler::intern("Styles", Group::Engine);
    static const profiler::ScopeId kLayout = profiler::intern("Layout", Group::Engine);
    static const profiler::ScopeId kPopups = profiler::intern("Popups", Group::Engine);
    switch (pass) {
        case jadefx::LayoutPass::Styles:
            return kStyles;
        case jadefx::LayoutPass::Layout:
            return kLayout;
        case jadefx::LayoutPass::Popups:
            return kPopups;
    }
    return kStyles;
}

constexpr std::size_t kPassCount = static_cast<std::size_t>(jadefx::LayoutPass::Popups) + 1;
```

At the end of `profile_ui_frames`, add:

```cpp
    // The styles, layout, and popups passes inside the layout phase, as nested scopes.
    stage.setLayoutPassHook([open = std::array<bool, kPassCount>{}](jadefx::LayoutPass pass, bool begin) mutable {
        const std::size_t at = static_cast<std::size_t>(pass);
        if (at >= kPassCount) {
            return;
        }
        if (begin) {
            open[at] = profiler::enabled();
            if (open[at]) {
                profiler::begin(pass_scope(pass));
            }
        } else if (open[at]) {
            open[at] = false;
            profiler::end();
        }
    });
```

In `UiFrameProfile.hpp`, append this sentence to the `profile_ui_frames` comment: `Styles and layout is split into its styles, layout, and popups passes.`

- [ ] **Step 5: Build and run to verify it passes**

Run the Step 3 command.
Expected: no new FAIL lines. Only the known `six frames recorded` flake may appear.

- [ ] **Step 6: Commit**

```bash
cd C:/Users/Andrew/Documents/GitHub/AnarchyEngine-CPP && git add src/runner/UiFrameProfile.cpp src/runner/UiFrameProfile.hpp tests/UiFrameProfileTest.cpp && git commit -m "Split the profiler's styles-and-layout scope into its styles, layout, and popups passes

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Benchmark and baseline

**Files:**
- Create: `tests/style_layout_bench.cpp`
- Modify: `CMakeLists.txt` (new target after `jadefx_warnings(jadefx-tests)`)
- Create (untracked, under ignored `build/`): `build/studio_profile.sh`

**Interfaces:**
- Consumes: `Scene::setLayoutPassHook` (Task 1).
- Produces: executable `jadefx-style-bench`, which prints `first frame`, `steady frame` (with styles, layout, and popups split), and `hover frame` means in ms. Script `build/studio_profile.sh` prints Studio's `Styles and layout`, `Styles`, `Layout`, and `Popups` rows. Tasks 7 and 16 rerun both.

- [ ] **Step 1: Write `tests/style_layout_bench.cpp`**

```cpp
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
```

- [ ] **Step 2: Add the target**

In `CMakeLists.txt`, after `jadefx_warnings(jadefx-tests)`:

```cmake
    # Times the styles-and-layout pass on a Studio-sized tree. Run the Release build.
    add_executable(jadefx-style-bench tests/style_layout_bench.cpp)
    target_link_libraries(jadefx-style-bench PRIVATE jadefx)
    jadefx_warnings(jadefx-style-bench)
```

- [ ] **Step 3: Build and run the benchmark three times**

```bash
MSYS_NO_PATHCONV=1 "C:/Program Files/CMake/bin/cmake.exe" --build C:/Users/Andrew/Documents/GitHub/JadeFX_CPP/build --config Release --target jadefx-style-bench --parallel && cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && for i in 1 2 3; do ./build/Release/jadefx-style-bench.exe; done
```

Expected: three reports that each start `style-layout bench: 2045 nodes`, give or take a few nodes from controls' internal children. Take the median of each line as the baseline.

- [ ] **Step 4: Write `build/studio_profile.sh` to read Studio's zone live**

```bash
#!/usr/bin/env bash
# Prints Studio's "Styles and layout" scope and its passes from a live Release
# AnarchyStudio over MCP, measured in the editor at rest. Usage: bash build/studio_profile.sh
set -u
ENGINE="C:/Users/Andrew/Documents/GitHub/AnarchyEngine-CPP"
PROJECT="C:/Users/Andrew/Documents/Anarchy Engine Projects/Bounce"
PORT=47231
TOKEN="style-perf-$RANDOM"
SCRATCH="$(mktemp -d)"
APPDATA="$SCRATCH" ANARCHY_MCP_PORT=$PORT ANARCHY_MCP_TOKEN=$TOKEN "$ENGINE/build/Release/AnarchyStudio.exe" "$PROJECT" &
STUDIO=$!
call() {
  curl -s -X POST "http://127.0.0.1:$PORT/mcp" -H "Authorization: Bearer $TOKEN" \
    -H "Content-Type: application/json" -H "Accept: application/json, text/event-stream" -d "$1"
}
for i in $(seq 1 60); do
  if call '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"style-perf","version":"1"}}}' | grep -q '"result"'; then break; fi
  sleep 1
done
sleep 5
call '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"get_profile","arguments":{"seconds":5,"top":80,"include_timeline":false}}}' > "$SCRATCH/profile.json"
sed 's/\\n/\n/g' "$SCRATCH/profile.json" | grep -E "Styles and layout|Styles|Layout|Popups"
kill $STUDIO 2>/dev/null
wait $STUDIO 2>/dev/null
```

- [ ] **Step 5: Record the Studio baseline**

Run the engine build (Global Constraints), which builds AnarchyStudio from this JadeFX tree including Task 1. Then run:

```bash
bash C:/Users/Andrew/Documents/GitHub/JadeFX_CPP/build/studio_profile.sh
```

Expected: lines with the per-frame average for `Styles and layout`, plus `Styles`, `Layout`, and `Popups`. If the output format differs, open `$SCRATCH/profile.json` and read the same rows by hand. Note the averages.

- [ ] **Step 6: Commit with the baseline numbers**

Replace each `<…>` with the measured median.

```bash
cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && git add tests/style_layout_bench.cpp CMakeLists.txt && git commit -m "Add a styles-and-layout benchmark on a Studio-sized tree

Baseline (Release, median of three runs, 100 frames each):
  first frame  <F> ms
  steady frame <S> ms (styles <s>, layout <l>, popups <p>)
  hover frame  <H> ms
Studio editor at rest (get_profile, Bounce): Styles and layout <Z> ms
(Styles <zs>, Layout <zl>, Popups <zp>)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Index selectors by their rightmost compound

**Files:**
- Modify: `src/style/Stylesheet.cpp:986-1058` (`Stylesheet::Data`, end of `Stylesheet::parse`, `collectMatching`)
- Test: `tests/style_perf_tests.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `Stylesheet::collectMatching(Node&, std::vector<MatchedDeclaration>&) const` with unchanged output: rules in source order, each with the highest specificity among its matching selectors.

- [ ] **Step 1: Write the failing-if-wrong test**

Add to `tests/style_perf_tests.cpp`, inside the namespace:

```cpp
// A computed color against 0-255 channels.
bool Is(const jadefx::Color& color, int r, int g, int b) {
    auto close = [](float channel, int value) { return std::fabs(channel * 255.f - static_cast<float>(value)) < 1.5f; };
    return close(color.r, r) && close(color.g, g) && close(color.b, b);
}

void TestIndexedSelectorsMatch() {
    auto root = jadefx::make<jadefx::VBox>();
    auto byId = jadefx::make<jadefx::StackPane>();
    byId->setElementId("only");
    auto twoClasses = jadefx::make<jadefx::StackPane>();
    twoClasses->getClassList().add("b");
    twoClasses->getClassList().add("a");
    auto typed = jadefx::make<jadefx::Label>("typed");
    typed->getClassList().add("primary");
    auto outer = jadefx::make<jadefx::StackPane>();
    outer->getClassList().add("outer");
    auto inner = jadefx::make<jadefx::StackPane>();
    inner->getClassList().add("inner");
    outer->getChildren().add(inner);
    auto plain = jadefx::make<jadefx::StackPane>();
    for (const std::shared_ptr<jadefx::Node>& node :
         std::vector<std::shared_ptr<jadefx::Node>>{byId, twoClasses, typed, outer, plain}) {
        root->getChildren().add(node);
    }
    auto scene = jadefx::make<jadefx::Scene>(root, 200, 300);
    scene->setStylesheet("* { border-color: #010203; }"
                         "#only { background-color: #ff0000; }"
                         ".a.b { background-color: #00ff00; }"
                         "label.primary { color: #0000ff; }"
                         "label { color: #123456; }"
                         ".outer .inner { background-color: #abcdef; }"
                         "stackpane, .never { background-color: #fedcba; }"
                         ".never, #only { color: #102030; }");
    scene->layout(200, 300, 0);
    Expect(Is(byId->computedStyle().background.color, 255, 0, 0), "an id rule beats a type rule");
    Expect(Is(byId->computedStyle().color, 16, 32, 48), "a rule matched through its second selector applies");
    Expect(Is(twoClasses->computedStyle().background.color, 0, 255, 0),
           "a two-class selector matches classes listed in either order");
    Expect(Is(typed->computedStyle().color, 0, 0, 255), "a type-and-class selector beats a bare type selector");
    Expect(Is(inner->computedStyle().background.color, 171, 205, 239), "a descendant selector matches");
    Expect(Is(plain->computedStyle().background.color, 254, 220, 186), "a type selector in a selector list matches");
    Expect(Is(plain->computedStyle().borderColor, 1, 2, 3), "the universal selector matches every node");
}
```

Add `TestIndexedSelectorsMatch();` to `RunStylePerfTests()`.

- [ ] **Step 2: Run it on the unindexed code**

Run the JadeFX build and test commands.
Expected: PASS. This test pins the current behavior so the index cannot change it.

- [ ] **Step 3: Build the index**

In `Stylesheet.cpp`, add `#include <cstdint>`, `#include <string_view>`, and `#include <unordered_map>`. At the end of the anonymous namespace, add:

```cpp
// A selector, by its rule and its place in that rule.
struct IndexEntry {
    std::uint32_t rule = 0;
    std::uint32_t selector = 0;
};

bool operator<(const IndexEntry& a, const IndexEntry& b) {
    return a.rule != b.rule ? a.rule < b.rule : a.selector < b.selector;
}

bool operator==(const IndexEntry& a, const IndexEntry& b) { return a.rule == b.rule && a.selector == b.selector; }

using IndexBuckets = std::unordered_map<std::string_view, std::vector<IndexEntry>>;
```

Replace `struct Stylesheet::Data { std::vector<Rule> rules; };` with:

```cpp
// Rules, and each selector filed under its rightmost compound: by id when it has
// one, else by its first class, else by its type, else as universal. A node can
// only match selectors filed under its own id, classes, or type, or the universal
// ones. The keys view strings inside rules, which never change after parse.
struct Stylesheet::Data {
    std::vector<Rule> rules;
    IndexBuckets byId;
    IndexBuckets byClass;
    IndexBuckets byType;
    std::vector<IndexEntry> universal;
};
```

At the end of `Stylesheet::parse`, before `return sheet;`:

```cpp
    Data& data = *sheet.data_;
    for (std::uint32_t r = 0; r < data.rules.size(); ++r) {
        const Rule& rule = data.rules[r];
        for (std::uint32_t s = 0; s < rule.selectors.size(); ++s) {
            const Compound& key = rule.selectors[s].compounds.back();
            const IndexEntry entry{r, s};
            if (!key.id.empty()) {
                data.byId[key.id].push_back(entry);
            } else if (!key.classes.empty()) {
                data.byClass[key.classes.front()].push_back(entry);
            } else if (!key.type.empty()) {
                data.byType[key.type].push_back(entry);
            } else {
                data.universal.push_back(entry);
            }
        }
    }
```

- [ ] **Step 4: Match through the index**

Replace `Stylesheet::collectMatching` with:

```cpp
void Stylesheet::collectMatching(Node& node, std::vector<MatchedDeclaration>& out) const {
    if (!data_ || data_->rules.empty()) {
        return;
    }
    const Data& data = *data_;
    // Reused between calls. Matching never calls back into a stylesheet.
    thread_local std::vector<IndexEntry> candidates;
    candidates.clear();
    auto gather = [&](const IndexBuckets& buckets, std::string_view key) {
        const auto found = buckets.find(key);
        if (found != buckets.end()) {
            candidates.insert(candidates.end(), found->second.begin(), found->second.end());
        }
    };
    if (!node.getElementId().empty()) {
        gather(data.byId, node.getElementId());
    }
    for (const std::string& name : node.getClassList().items()) {
        gather(data.byClass, name);
    }
    gather(data.byType, node.getElementType());
    candidates.insert(candidates.end(), data.universal.begin(), data.universal.end());
    // Source order, as the unindexed scan produced. A class listed twice adds its bucket twice.
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    for (std::size_t i = 0; i < candidates.size();) {
        const std::uint32_t ruleIndex = candidates[i].rule;
        const Rule& rule = data.rules[ruleIndex];
        // A rule with several selectors counts the most specific one that matches.
        int specificity = -1;
        for (; i < candidates.size() && candidates[i].rule == ruleIndex; ++i) {
            const Selector& selector = rule.selectors[candidates[i].selector];
            if (selector.specificity > specificity && Matches(selector, node)) {
                specificity = selector.specificity;
            }
        }
        if (specificity < 0) {
            continue;
        }
        for (const Declaration& declaration : rule.declarations) {
            out.push_back({&declaration, specificity});
        }
    }
}
```

- [ ] **Step 5: Build and run the tests**

Run the JadeFX build and test commands.
Expected: `layout tests passed`, or only the known flakes.

- [ ] **Step 6: Commit**

```bash
cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && git add src/style/Stylesheet.cpp tests/style_perf_tests.cpp && git commit -m "Match only the selectors filed under a node's id, classes, type, or universal

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Parse declarations once, at load

**Files:**
- Modify: `include/jadefx/style/Style.hpp` (new `PropertyId`, `propertyIdOf`, `DeclarationValue`; extend `Declaration`)
- Modify: `src/style/Stylesheet.cpp` (`ParseDeclarations`, `BoxFromLengths`, `AssignRadius`, new `PrepareDeclaration`, `applyDeclarations`)
- Modify: `src/scene/Node.cpp:74-88` (`LastCursor`)
- Test: `tests/style_perf_tests.cpp`

**Interfaces:**
- Produces:
  - `enum class PropertyId : unsigned char { Unknown, Custom, FontSize, FontFamily, BackgroundColor, Background, BackgroundImage, FontSmoothing, Color, ImageColor, Width, Height, MinWidth, MinHeight, MaxWidth, MaxHeight, BorderRadius, BorderWidth, BorderColor, BorderStyle, BoxShadow, Padding, Spacing, Gap, RowGap, ColumnGap, Alignment, Orientation, Opacity, IndeterminateBarLength, IndeterminateBarEscape, IndeterminateBarFlip, IndeterminateBarAnimationTime, Transition, Cursor, All, Count };`
  - `PropertyId propertyIdOf(std::string_view property);`
  - `struct DeclarationValue { enum class Kind : unsigned char { Raw, Color, Size, Lengths, Invalid }; Kind kind; Color color; SizeSpec size; int lengthCount; SizeSpec lengths[4]; };`
  - `Declaration` gains `PropertyId id`, `bool hasVar`, and `DeclarationValue parsed`.

- [ ] **Step 1: Write the failing test**

Add to `tests/style_perf_tests.cpp`:

```cpp
void TestPropertyIds() {
    Expect(jadefx::propertyIdOf("background-color") == jadefx::PropertyId::BackgroundColor, "background-color has its id");
    Expect(jadefx::propertyIdOf("--panel-bg") == jadefx::PropertyId::Custom, "a custom property is custom");
    Expect(jadefx::propertyIdOf("accent-color") == jadefx::PropertyId::Custom, "accent-color is stored as a custom property");
    Expect(jadefx::propertyIdOf("all") == jadefx::PropertyId::All, "all has its id, for transitions");
    Expect(jadefx::propertyIdOf("not-a-property") == jadefx::PropertyId::Unknown, "an unknown property is unknown");
}

void TestDeclarationsParsedOnce() {
    using Kind = jadefx::DeclarationValue::Kind;
    const std::vector<jadefx::Declaration> parsed = jadefx::parseInlineDeclarations(
        "color: #ff0000; width: 50%; padding: 1em 2px; border-color: var(--x); background-color: nonsense");
    Expect(parsed.size() == 5, "five declarations parse");
    if (parsed.size() != 5) {
        return;
    }
    Expect(parsed[0].parsed.kind == Kind::Color && Is(parsed[0].parsed.color, 255, 0, 0), "a color is parsed at load");
    Expect(parsed[1].parsed.kind == Kind::Size && parsed[1].parsed.size.kind == jadefx::SizeKind::Percent &&
               parsed[1].parsed.size.percent == 0.5,
           "a length is parsed at load");
    Expect(parsed[2].parsed.kind == Kind::Lengths && parsed[2].parsed.lengthCount == 2 &&
               parsed[2].parsed.lengths[0].em == 1 && parsed[2].parsed.lengths[1].pixels == 2,
           "a length list keeps em for the node's font size");
    Expect(parsed[3].hasVar && parsed[3].parsed.kind == Kind::Raw, "a value with var() stays raw");
    Expect(parsed[4].parsed.kind == Kind::Invalid, "a value that does not parse is marked invalid");
}

void TestVarResolvedPerNode() {
    auto root = jadefx::make<jadefx::VBox>();
    auto first = jadefx::make<jadefx::StackPane>();
    auto second = jadefx::make<jadefx::StackPane>();
    first->getClassList().add("t");
    second->getClassList().add("t");
    first->setStyle("--tone: #ff0000;");
    second->setStyle("--tone: #00ff00;");
    root->getChildren().add(first);
    root->getChildren().add(second);
    auto scene = jadefx::make<jadefx::Scene>(root, 200, 200);
    scene->setStylesheet(".t { font-size: 20px; padding: 1em; background-color: var(--tone); }"
                         ".t { background-color: nonsense; }");
    scene->layout(200, 200, 0);
    Expect(Is(first->computedStyle().background.color, 255, 0, 0), "var() resolves against the first node's value");
    Expect(Is(second->computedStyle().background.color, 0, 255, 0), "var() resolves against the second node's value");
    Expect(first->computedStyle().padding.left == 20, "an em length uses the node's own font size");
}
```

Add the three calls to `RunStylePerfTests()`.

- [ ] **Step 2: Build to verify it fails**

Run the JadeFX build command.
Expected: compile errors because `propertyIdOf`, `PropertyId`, and `DeclarationValue` are not declared.

- [ ] **Step 3: Declare the types in `Style.hpp`**

Before `struct Declaration`:

```cpp
// Each property applyDeclarations understands. Custom is a --name property, or
// accent-color, caret-color, or outline-color, which are stored as one. All names
// every property, for transition.
enum class PropertyId : unsigned char {
    Unknown,
    Custom,
    FontSize,
    FontFamily,
    BackgroundColor,
    Background,
    BackgroundImage,
    FontSmoothing,
    Color,
    ImageColor,
    Width,
    Height,
    MinWidth,
    MinHeight,
    MaxWidth,
    MaxHeight,
    BorderRadius,
    BorderWidth,
    BorderColor,
    BorderStyle,
    BoxShadow,
    Padding,
    Spacing,
    Gap,
    RowGap,
    ColumnGap,
    Alignment,
    Orientation,
    Opacity,
    IndeterminateBarLength,
    IndeterminateBarEscape,
    IndeterminateBarFlip,
    IndeterminateBarAnimationTime,
    Transition,
    Cursor,
    All,
    Count,
};

inline constexpr std::size_t kPropertyIdCount = static_cast<std::size_t>(PropertyId::Count);

// The id for a lower-case property name, or Custom, or Unknown.
PropertyId propertyIdOf(std::string_view property);

// A declaration's value, parsed when its stylesheet loads. Raw means it is parsed
// as each node applies it, as a value with var() must be. Invalid means it did not
// parse, so applying it changes nothing. Lengths keeps up to four lengths with px
// in pixels, % in percent, and em in em, as padding, border-width, and
// border-radius list them.
struct DeclarationValue {
    enum class Kind : unsigned char { Raw, Color, Size, Lengths, Invalid };
    Kind kind = Kind::Raw;
    Color color;
    SizeSpec size;
    int lengthCount = 0;
    SizeSpec lengths[4];
};
```

Extend `Declaration`:

```cpp
struct Declaration {
    std::string property;
    std::string value;
    // Declared with !important, which wins over normal declarations of any origin.
    bool important = false;
    PropertyId id = PropertyId::Unknown;
    // The value has var() in it and is resolved against each node's custom properties.
    bool hasVar = false;
    DeclarationValue parsed;
};
```

Add `#include <cstddef>` to `Style.hpp`.

- [ ] **Step 4: Name lookup and pre-parsing in `Stylesheet.cpp`**

Change `BoxFromLengths` and `AssignRadius` to take a pointer and a count. The bodies are unchanged except that `values.size()` becomes `count`:

```cpp
Insets BoxFromLengths(const ParsedLength* values, std::size_t count, float emFontSize) {
    double resolved[4] = {};
    const std::size_t used = std::min<std::size_t>(count, 4);
    for (std::size_t i = 0; i < used; ++i) {
        resolved[i] = ResolveLength(values[i], emFontSize);
    }
    if (count == 1) {
        return Insets::uniform(resolved[0]);
    }
    if (count == 2) {
        return Insets::axes(resolved[0], resolved[1]);
    }
    if (count == 3) {
        return {resolved[0], resolved[1], resolved[2], resolved[1]};
    }
    return {resolved[0], resolved[1], resolved[2], resolved[3]};
}
```

In `AssignRadius(ComputedStyle& style, const ParsedLength* values, std::size_t count)`, replace `values.empty()` with `count == 0` and `values.size()` with `count`. Remove `LengthsOk`. Run `grep -n "BoxFromLengths\|AssignRadius\|LengthsOk" src/style/Stylesheet.cpp`, and update every call site outside `applyDeclarations` to pass `list.data(), list.size()`.

Above `ParseDeclarations`, add the forward declaration `void PrepareDeclaration(Declaration& declaration);`. In `ParseDeclarations`, call `PrepareDeclaration(declaration);` just before `declarations.push_back(std::move(declaration));`.

At the end of the anonymous namespace, before the Task 4 `IndexEntry`, add:

```cpp
struct PropertyName {
    std::string_view name;
    PropertyId id;
};

constexpr PropertyName kPropertyNames[] = {
    {"font-size", PropertyId::FontSize},
    {"font-family", PropertyId::FontFamily},
    {"background-color", PropertyId::BackgroundColor},
    {"background", PropertyId::Background},
    {"background-image", PropertyId::BackgroundImage},
    {"font-smoothing", PropertyId::FontSmoothing},
    {"color", PropertyId::Color},
    {"image-color", PropertyId::ImageColor},
    {"width", PropertyId::Width},
    {"height", PropertyId::Height},
    {"min-width", PropertyId::MinWidth},
    {"min-height", PropertyId::MinHeight},
    {"max-width", PropertyId::MaxWidth},
    {"max-height", PropertyId::MaxHeight},
    {"border-radius", PropertyId::BorderRadius},
    {"border-width", PropertyId::BorderWidth},
    {"border-color", PropertyId::BorderColor},
    {"border-style", PropertyId::BorderStyle},
    {"box-shadow", PropertyId::BoxShadow},
    {"padding", PropertyId::Padding},
    {"spacing", PropertyId::Spacing},
    {"gap", PropertyId::Gap},
    {"row-gap", PropertyId::RowGap},
    {"column-gap", PropertyId::ColumnGap},
    {"alignment", PropertyId::Alignment},
    {"orientation", PropertyId::Orientation},
    {"opacity", PropertyId::Opacity},
    {"indeterminate-bar-length", PropertyId::IndeterminateBarLength},
    {"indeterminate-bar-escape", PropertyId::IndeterminateBarEscape},
    {"indeterminate-bar-flip", PropertyId::IndeterminateBarFlip},
    {"indeterminate-bar-animation-time", PropertyId::IndeterminateBarAnimationTime},
    {"transition", PropertyId::Transition},
    {"cursor", PropertyId::Cursor},
    {"all", PropertyId::All},
};

void PrepareDeclaration(Declaration& declaration) {
    declaration.id = propertyIdOf(declaration.property);
    declaration.hasVar = declaration.value.find("var(") != std::string::npos;
    if (declaration.hasVar) {
        return;
    }
    DeclarationValue& parsed = declaration.parsed;
    switch (declaration.id) {
        case PropertyId::Color:
        case PropertyId::BackgroundColor:
        case PropertyId::BorderColor: {
            bool ok = false;
            parsed.color = Color::parse(declaration.value, &ok);
            parsed.kind = ok ? DeclarationValue::Kind::Color : DeclarationValue::Kind::Invalid;
            break;
        }
        case PropertyId::Background: {
            // A gradient is parsed as it is applied. A plain color is parsed now.
            if (lowerCopy(declaration.value).find("gradient") != std::string::npos) {
                break;
            }
            bool ok = false;
            parsed.color = Color::parse(declaration.value, &ok);
            parsed.kind = ok ? DeclarationValue::Kind::Color : DeclarationValue::Kind::Invalid;
            break;
        }
        case PropertyId::Width:
        case PropertyId::Height:
        case PropertyId::MinWidth:
        case PropertyId::MinHeight:
        case PropertyId::MaxWidth:
        case PropertyId::MaxHeight:
            parsed.size = LengthToSpec(declaration.value);
            parsed.kind = parsed.size.set() ? DeclarationValue::Kind::Size : DeclarationValue::Kind::Invalid;
            break;
        case PropertyId::Padding:
        case PropertyId::BorderWidth:
        case PropertyId::BorderRadius: {
            const std::vector<ParsedLength> list = LengthList(declaration.value);
            if (list.empty()) {
                parsed.kind = DeclarationValue::Kind::Invalid;
                break;
            }
            // Past four, only the first four count, as BoxFromLengths and AssignRadius read them.
            parsed.kind = DeclarationValue::Kind::Lengths;
            parsed.lengthCount = static_cast<int>(std::min<std::size_t>(list.size(), 4));
            for (int i = 0; i < parsed.lengthCount; ++i) {
                parsed.lengths[i].pixels = list[static_cast<std::size_t>(i)].px;
                parsed.lengths[i].percent = list[static_cast<std::size_t>(i)].percent;
                parsed.lengths[i].em = list[static_cast<std::size_t>(i)].em;
            }
            break;
        }
        default:
            break;
    }
}

bool ColorOf(const Declaration& declaration, const std::string& value, Color& color) {
    switch (declaration.parsed.kind) {
        case DeclarationValue::Kind::Color:
            color = declaration.parsed.color;
            return true;
        case DeclarationValue::Kind::Invalid:
            return false;
        default: {
            bool ok = false;
            color = Color::parse(value, &ok);
            return ok;
        }
    }
}

bool SizeOf(const Declaration& declaration, const std::string& value, SizeSpec& size) {
    switch (declaration.parsed.kind) {
        case DeclarationValue::Kind::Size:
            size = declaration.parsed.size;
            return true;
        case DeclarationValue::Kind::Invalid:
            return false;
        default:
            size = LengthToSpec(value);
            return size.set();
    }
}

// Up to four lengths into out. Returns how many, or 0 when the value does not parse.
std::size_t LengthsOf(const Declaration& declaration, const std::string& value, ParsedLength (&out)[4]) {
    if (declaration.parsed.kind == DeclarationValue::Kind::Lengths) {
        const std::size_t count = static_cast<std::size_t>(declaration.parsed.lengthCount);
        for (std::size_t i = 0; i < count; ++i) {
            const SizeSpec& length = declaration.parsed.lengths[i];
            out[i] = ParsedLength{true, length.pixels, length.percent, length.em};
        }
        return count;
    }
    if (declaration.parsed.kind == DeclarationValue::Kind::Invalid) {
        return 0;
    }
    const std::vector<ParsedLength> list = LengthList(value);
    const std::size_t count = std::min<std::size_t>(list.size(), 4);
    std::copy_n(list.begin(), count, out);
    return count;
}
```

After the anonymous namespace (next to `resolveCssVariables`), add:

```cpp
PropertyId propertyIdOf(std::string_view property) {
    if (property.rfind("--", 0) == 0 || property == "accent-color" || property == "caret-color" ||
        property == "outline-color") {
        return PropertyId::Custom;
    }
    for (const PropertyName& entry : kPropertyNames) {
        if (entry.name == property) {
            return entry.id;
        }
    }
    return PropertyId::Unknown;
}
```

- [ ] **Step 5: Dispatch on the id in `applyDeclarations`**

In the Variables pass, replace `if (!isCustom(property))` with `if (declaration.id != PropertyId::Custom)`, and delete the `isCustom` lambda. Replace the Fonts/Rest loop, from `for (const Declaration* each : declarations) {` after the Variables block to the end of the function, with:

```cpp
    for (const Declaration* each : declarations) {
        const Declaration& declaration = *each;
        const PropertyId id = declaration.id;
        if (id == PropertyId::Custom) {
            continue;
        }
        const bool fontProperty = id == PropertyId::FontSize || id == PropertyId::FontFamily;
        if ((pass == StylePass::Fonts) != fontProperty) {
            continue;
        }
        // A var() is resolved now, against the custom properties this node has.
        std::string resolvedValue;
        if (declaration.hasVar) {
            resolvedValue = resolveCssVariables(declaration.value, style.variables.get());
            if (resolvedValue.empty()) {
                continue;
            }
        }
        const std::string& value = declaration.hasVar ? resolvedValue : declaration.value;
        switch (id) {
            case PropertyId::FontSize: {
                const ParsedLength length = ParseLength(value);
                if (length.ok) {
                    style.fontSize = static_cast<float>(length.px + (length.em + length.percent) * inheritedFontSize);
                }
                break;
            }
            case PropertyId::FontFamily: {
                std::string family = value;
                const std::size_t comma = family.find(',');
                if (comma != std::string::npos) {
                    family = family.substr(0, comma);
                }
                family = trimCopy(family);
                if (family.size() >= 2 && ((family.front() == '"' && family.back() == '"') ||
                                           (family.front() == '\'' && family.back() == '\''))) {
                    family = family.substr(1, family.size() - 2);
                }
                if (!family.empty()) {
                    style.fontFamily = family;
                }
                break;
            }
            case PropertyId::BackgroundColor: {
                Color color;
                if (!ColorOf(declaration, value, color)) {
                    break;
                }
                style.background.color = color;
                style.background.hasColor = true;
                style.background.visible = true;
                break;
            }
            case PropertyId::Background: {
                Color color;
                if (declaration.parsed.kind == DeclarationValue::Kind::Raw &&
                    lowerCopy(value).find("gradient") != std::string::npos) {
                    style.background.hasColor = false;
                    style.background.color = Color::transparent();
                    ApplyBackgroundImage(style, value);
                    break;
                }
                if (!ColorOf(declaration, value, color)) {
                    break;
                }
                style.background.color = color;
                style.background.hasColor = true;
                style.background.gradient = false;
                style.background.stopCount = 0;
                style.background.visible = color.a > 0.f || style.background.gradient;
                break;
            }
            case PropertyId::BackgroundImage:
                if (lowerCopy(trimCopy(value)) == "none") {
                    style.background.gradient = false;
                    style.background.stopCount = 0;
                    style.background.visible = style.background.hasColor && style.background.color.a > 0.f;
                    break;
                }
                ApplyBackgroundImage(style, value);
                break;
            case PropertyId::FontSmoothing: {
                // subpixel-antialiased keeps stripe coverage. antialiased, none, and grayscale do not.
                // auto is the platform's own choice.
                const std::string mode = lowerCopy(trimCopy(value));
                if (mode == "auto") {
                    style.subpixel = kSubpixelByDefault;
                } else if (mode == "subpixel-antialiased") {
                    style.subpixel = true;
                } else if (mode == "antialiased" || mode == "none" || mode == "grayscale") {
                    style.subpixel = false;
                }
                break;
            }
            case PropertyId::Color: {
                Color color;
                if (ColorOf(declaration, value, color)) {
                    style.color = color;
                }
                break;
            }
            case PropertyId::ImageColor: {
                const std::string mode = lowerCopy(trimCopy(value));
                if (mode == "none") {
                    style.imageColorSet = false;
                    style.imageColorCurrent = false;
                } else if (mode == "currentcolor") {
                    style.imageColorSet = true;
                    style.imageColorCurrent = true;
                } else {
                    bool ok = false;
                    const Color color = Color::parse(value, &ok);
                    if (ok) {
                        style.imageColorSet = true;
                        style.imageColorCurrent = false;
                        style.imageColor = color;
                    }
                }
                break;
            }
            case PropertyId::Width:
            case PropertyId::Height:
            case PropertyId::MinWidth:
            case PropertyId::MinHeight:
            case PropertyId::MaxWidth:
            case PropertyId::MaxHeight: {
                SizeSpec size;
                if (!SizeOf(declaration, value, size)) {
                    break;
                }
                SizeSpec* slot = id == PropertyId::Width       ? &style.width
                                 : id == PropertyId::Height    ? &style.height
                                 : id == PropertyId::MinWidth  ? &style.minWidth
                                 : id == PropertyId::MinHeight ? &style.minHeight
                                 : id == PropertyId::MaxWidth  ? &style.maxWidth
                                                               : &style.maxHeight;
                *slot = size;
                break;
            }
            case PropertyId::BorderRadius: {
                ParsedLength lengths[4];
                const std::size_t count = LengthsOf(declaration, value, lengths);
                if (count > 0) {
                    AssignRadius(style, lengths, count);
                }
                break;
            }
            case PropertyId::BorderWidth: {
                ParsedLength lengths[4];
                const std::size_t count = LengthsOf(declaration, value, lengths);
                if (count > 0) {
                    style.border = BoxFromLengths(lengths, count, emFontSize);
                }
                break;
            }
            case PropertyId::BorderColor: {
                Color color;
                if (ColorOf(declaration, value, color)) {
                    style.borderColor = color;
                }
                break;
            }
            case PropertyId::BorderStyle: {
                const std::string kind = lowerCopy(trimCopy(value));
                style.borderStyle = kind == "solid" ? BorderStyle::Solid : BorderStyle::None;
                break;
            }
            case PropertyId::BoxShadow:
                style.shadows.clear();
                if (lowerCopy(trimCopy(value)) == "none") {
                    break;
                }
                for (const std::string& part : SplitDepth(value, ',')) {
                    if (!trimCopy(part).empty()) {
                        style.shadows.push_back(ParseShadow(part, emFontSize));
                    }
                }
                break;
            case PropertyId::Padding: {
                ParsedLength lengths[4];
                const std::size_t count = LengthsOf(declaration, value, lengths);
                if (count > 0) {
                    style.padding = BoxFromLengths(lengths, count, emFontSize);
                }
                break;
            }
            case PropertyId::Spacing: {
                const ParsedLength length = ParseLength(value);
                if (length.ok && length.percent == 0.0) {
                    style.spacing = static_cast<float>(ResolveLength(length, emFontSize));
                }
                break;
            }
            case PropertyId::Gap:
            case PropertyId::RowGap:
            case PropertyId::ColumnGap: {
                // As in CSS: gap is row-gap then column-gap, and one value sets both.
                // A box's spacing follows gap too.
                std::vector<float> lengths;
                for (const std::string& part : SplitDepth(value, ' ')) {
                    const ParsedLength length = ParseLength(trimCopy(part));
                    if (length.ok && length.percent == 0.0) {
                        lengths.push_back(static_cast<float>(ResolveLength(length, emFontSize)));
                    }
                }
                if (lengths.empty()) {
                    break;
                }
                if (id == PropertyId::RowGap) {
                    style.rowGap = lengths.front();
                } else if (id == PropertyId::ColumnGap) {
                    style.columnGap = lengths.front();
                } else {
                    style.rowGap = lengths.front();
                    style.columnGap = lengths.size() > 1 ? lengths[1] : lengths.front();
                    style.spacing = lengths.front();
                }
                break;
            }
            case PropertyId::Alignment:
                style.alignment = ParseAlignment(value);
                style.alignmentFromCss = true;
                break;
            case PropertyId::Orientation: {
                const std::string kind = lowerCopy(trimCopy(value));
                if (kind == "horizontal" || kind == "vertical") {
                    style.orientationFromCss = true;
                    style.orientation = kind == "vertical" ? Orientation::Vertical : Orientation::Horizontal;
                }
                break;
            }
            case PropertyId::Opacity: {
                double opacity = 1;
                std::size_t consumed = 0;
                if (ParseNumber(value, opacity, consumed)) {
                    style.opacity = static_cast<float>(std::clamp(opacity, 0.0, 1.0));
                }
                break;
            }
            case PropertyId::IndeterminateBarLength: {
                const ParsedLength length = ParseLength(value);
                if (length.ok && length.percent == 0.0) {
                    style.indeterminateBarLengthSet = true;
                    style.indeterminateBarLength = SizeSpec::px(ResolveLength(length, emFontSize));
                }
                break;
            }
            case PropertyId::IndeterminateBarEscape:
            case PropertyId::IndeterminateBarFlip: {
                const std::string kind = lowerCopy(trimCopy(value));
                if (kind == "true" || kind == "false") {
                    const bool enabled = kind == "true";
                    if (id == PropertyId::IndeterminateBarEscape) {
                        style.indeterminateBarEscapeSet = true;
                        style.indeterminateBarEscape = enabled;
                    } else {
                        style.indeterminateBarFlipSet = true;
                        style.indeterminateBarFlip = enabled;
                    }
                }
                break;
            }
            case PropertyId::IndeterminateBarAnimationTime: {
                double seconds = 0;
                std::size_t consumed = 0;
                if (ParseNumber(value, seconds, consumed) && consumed == trimCopy(value).size()) {
                    style.indeterminateBarAnimationTimeSet = true;
                    style.indeterminateBarAnimationTime = std::max(0.0, seconds);
                }
                break;
            }
            case PropertyId::Transition:
                ApplyTransition(style, value);
                break;
            default:
                break;
        }
    }
}
```

In `src/scene/Node.cpp` `LastCursor`, replace `declaration->property != "cursor"` with `declaration->id != PropertyId::Cursor`.

- [ ] **Step 6: Build and run the tests**

Run the JadeFX build and test commands.
Expected: `layout tests passed`, or only the known flakes, and no `/W4` warnings in Stylesheet.cpp.

- [ ] **Step 7: Commit**

```bash
cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && git add include/jadefx/style/Style.hpp src/style/Stylesheet.cpp src/scene/Node.cpp tests/style_perf_tests.cpp && git commit -m "Parse declaration values when a stylesheet loads and dispatch on a property id

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Reuse scratch buffers and share the transition table

**Files:**
- Modify: `include/jadefx/style/Style.hpp` (`TransitionTable`; `ComputedStyle::transitions`)
- Modify: `src/style/Stylesheet.cpp:791-822` (`ApplyTransition`)
- Modify: `src/scene/Node.cpp:40-51` (`TimingOf`), `:514-680` (`applyStyles`), anonymous namespace (scratch pool)
- Test: `tests/style_perf_tests.cpp`

**Interfaces:**
- Consumes: `PropertyId`, `kPropertyIdCount`, `propertyIdOf` (Task 5).
- Produces: `struct TransitionTable { TransitionTiming timing[kPropertyIdCount]; bool set[kPropertyIdCount]; };`, `ComputedStyle::transitions` of type `std::shared_ptr<const TransitionTable>`, and, in `Node.cpp`'s anonymous namespace, `struct StyleScratch` and `class ScratchLease` with `StyleScratch& operator*()`. Tasks 8 and later use `ScratchLease` for child lists.

- [ ] **Step 1: Write the test**

Add to `tests/style_perf_tests.cpp`:

```cpp
void TestTransitionTimingsByProperty() {
    auto root = jadefx::make<jadefx::VBox>();
    auto box = jadefx::make<jadefx::StackPane>();
    box->getClassList().add("t");
    auto edged = jadefx::make<jadefx::StackPane>();
    edged->getClassList().add("u");
    root->getChildren().add(box);
    root->getChildren().add(edged);
    auto scene = jadefx::make<jadefx::Scene>(root, 200, 200);
    scene->setStylesheet(".t { background-color: #000000; color: #000000; transition: background-color 1s, color 0s; }"
                         ".t.on { background-color: #ffffff; color: #ffffff; }"
                         ".u { border-color: #000000; transition: all 1s; }"
                         ".u.on { border-color: #ffffff; }");
    scene->layout(200, 200, 0);
    box->getClassList().add("on");
    edged->getClassList().add("on");
    scene->layout(200, 200, 0);
    scene->layout(200, 200, 0.5);
    const float background = box->computedStyle().background.color.r;
    Expect(background > 0.35f && background < 0.65f, "a timed property is halfway through its transition");
    Expect(Is(box->computedStyle().color, 255, 255, 255), "a property with a zero-second transition jumps");
    const float border = edged->computedStyle().borderColor.r;
    Expect(border > 0.35f && border < 0.65f, "all covers a property it does not name");
}

void TestDeepTreeInherits() {
    auto top = jadefx::make<jadefx::StackPane>();
    top->setStyle("color: #405060;");
    std::shared_ptr<jadefx::StackPane> parent = top;
    for (int i = 0; i < 40; ++i) {
        auto child = jadefx::make<jadefx::StackPane>();
        parent->getChildren().add(child);
        parent = child;
    }
    auto leaf = jadefx::make<jadefx::Label>("leaf");
    parent->getChildren().add(leaf);
    auto scene = jadefx::make<jadefx::Scene>(top, 200, 200);
    scene->layout(200, 200, 0);
    scene->layout(200, 200, 0.1);
    Expect(Is(leaf->computedStyle().color, 64, 80, 96), "a color inherits through forty levels");
}
```

Add both calls to `RunStylePerfTests()`.

- [ ] **Step 2: Run on the current code**

Run the JadeFX build and test commands.
Expected: PASS. These tests pin behavior that the refactor must keep.

- [ ] **Step 3: Replace the transitions map**

In `Style.hpp`, after `struct TransitionTiming`:

```cpp
// transition durations and delays by property. All covers properties not named.
// Immutable once a style holds it, so styles share one table.
struct TransitionTable {
    TransitionTiming timing[kPropertyIdCount] = {};
    bool set[kPropertyIdCount] = {};
};
```

Move `TransitionTiming` and `TransitionTable` below the `PropertyId` declarations, so that `kPropertyIdCount` is visible. In `ComputedStyle`, replace the `transitions` map and its comment with:

```cpp
    // transition timings. Null when no transition applies.
    std::shared_ptr<const TransitionTable> transitions;
```

Rewrite `ApplyTransition` in `Stylesheet.cpp`:

```cpp
void ApplyTransition(ComputedStyle& style, std::string_view text) {
    // Copied before it changes, since other styles may share the table.
    auto table = style.transitions ? std::make_shared<TransitionTable>(*style.transitions)
                                   : std::make_shared<TransitionTable>();
    for (const std::string& part : SplitDepth(text, ',')) {
        PropertyId property = PropertyId::All;
        double duration = 0;
        double delay = 0;
        int times = 0;
        bool sawProperty = false;
        for (const std::string& token : ShadowTokens(part)) {
            double seconds = 0;
            if (IsTime(token, seconds)) {
                if (times == 0) {
                    duration = seconds;
                } else if (times == 1) {
                    delay = seconds;
                }
                ++times;
            } else if (IsEasing(token)) {
                continue;
            } else if (!sawProperty) {
                property = propertyIdOf(lowerCopy(token));
                sawProperty = true;
            }
        }
        // A property nothing animates, or one this engine does not know, has no slot.
        if (property == PropertyId::Unknown || property == PropertyId::Custom) {
            continue;
        }
        const auto at = static_cast<std::size_t>(property);
        table->timing[at] = {std::max(0.0, duration), std::max(0.0, delay)};
        table->set[at] = true;
    }
    style.transitions = std::move(table);
}
```

`ApplyTransition` is declared before `propertyIdOf`'s definition, so add the declaration `PropertyId propertyIdOf(std::string_view property);` from `Style.hpp`. That header is already included, so no change is needed there.

In `Node.cpp`, replace `TimingOf`:

```cpp
TransitionTiming TimingOf(const ComputedStyle& style, PropertyId property) {
    if (!style.transitions) {
        return {};
    }
    const TransitionTable& table = *style.transitions;
    const auto at = static_cast<std::size_t>(property);
    if (table.set[at]) {
        return table.timing[at];
    }
    const auto all = static_cast<std::size_t>(PropertyId::All);
    return table.set[all] ? table.timing[all] : TransitionTiming{};
}
```

Change the six callers in `applyStyles` to pass `PropertyId::BackgroundColor`, `PropertyId::BackgroundImage`, `PropertyId::Color`, `PropertyId::BorderColor`, `PropertyId::BorderWidth`, and `PropertyId::BoxShadow`.

- [ ] **Step 4: Pool the scratch vectors**

In `Node.cpp`'s anonymous namespace, add `#include <memory>` at the top of the file and then:

```cpp
// The lists one restyle or one child walk needs. Kept between frames so a pass
// allocates nothing once the lists have grown.
struct StyleScratch {
    std::vector<MatchedDeclaration> agentMatches;
    std::vector<MatchedDeclaration> authorMatches;
    std::vector<const Node*> chain;
    std::vector<const Declaration*> agent;
    std::vector<const Declaration*> author;
    std::vector<const Declaration*> agentImportant;
    std::vector<const Declaration*> authorImportant;
    std::vector<const Declaration*> inlineImportant;
    std::vector<const Declaration*> variables;
    std::vector<Node*> kids;

    void clear() {
        agentMatches.clear();
        authorMatches.clear();
        chain.clear();
        agent.clear();
        author.clear();
        agentImportant.clear();
        authorImportant.clear();
        inlineImportant.clear();
        variables.clear();
        kids.clear();
    }
};

// One StyleScratch per nesting level. A restyle holds one while its children
// restyle, and a control may call applyCss from styleDidApply, so leases nest.
class ScratchLease {
public:
    ScratchLease() {
        Pool& pool = ThePool();
        if (pool.used == pool.items.size()) {
            pool.items.push_back(std::make_unique<StyleScratch>());
        }
        scratch_ = pool.items[pool.used++].get();
        scratch_->clear();
    }
    ~ScratchLease() { --ThePool().used; }
    ScratchLease(const ScratchLease&) = delete;
    ScratchLease& operator=(const ScratchLease&) = delete;

    StyleScratch& operator*() { return *scratch_; }

private:
    struct Pool {
        std::vector<std::unique_ptr<StyleScratch>> items;
        std::size_t used = 0;
    };
    static Pool& ThePool() {
        thread_local Pool pool;
        return pool;
    }
    StyleScratch* scratch_ = nullptr;
};
```

In `applyStyles`, add `ScratchLease lease;` and `StyleScratch& scratch = *lease;` as the first statements. Then replace each local vector with its scratch member: `agentMatches` → `scratch.agentMatches`, `authorMatches` → `scratch.authorMatches`, `chain` → `scratch.chain`, `agent` → `scratch.agent`, `author` → `scratch.author`, `agentImportant`, `authorImportant`, and `inlineImportant` likewise. For `variables`, use `scratch.variables.assign(scratch.agent.begin(), scratch.agent.end());` then `scratch.variables.insert(scratch.variables.end(), scratch.author.begin(), scratch.author.end());`. Pass `scratch.variables`, `scratch.agent`, and `scratch.author` to `applyDeclarations` and `ResolvedNodeCursor`. Replace the final child walk with:

```cpp
    visitChildren([&](Node* child) { scratch.kids.push_back(child); });
    for (Node* child : scratch.kids) {
        child->applyStyles(pass, timeSeconds);
    }
```

- [ ] **Step 5: Build and run the tests**

Run the JadeFX build and test commands.
Expected: `layout tests passed`, or only the known flakes.

- [ ] **Step 6: Commit**

```bash
cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && git add include/jadefx/style/Style.hpp src/style/Stylesheet.cpp src/scene/Node.cpp tests/style_perf_tests.cpp && git commit -m "Reuse restyle scratch lists between frames and share transition tables by property id

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Measure cache, theme-color cache, one popup restyle, and the Phase A checkpoint

**Files:**
- Modify: `include/jadefx/scene/Node.hpp` (private `MeasureCache`, `beginLayoutPass`, `endLayoutPass`, `measureCacheUsable`; `#include <cstdint>`)
- Modify: `src/scene/Node.cpp` (`measuredWidth`, `measuredHeight`, `themeColor`, `applyStyles`)
- Modify: `include/jadefx/style/Theme.hpp` (`kThemeColorCount`), `src/style/Theme.cpp:295-299`
- Modify: `include/jadefx/scene/Scene.hpp:247`, `src/scene/Scene.cpp` (`layout`, `layoutPopup`, `setUserAgentStylesheet`)
- Test: `tests/style_perf_tests.cpp`

**Interfaces:**
- Produces: `static void Node::beginLayoutPass()` and `static void Node::endLayoutPass()` (private; Scene is a friend); `bool Node::measureCacheUsable() const`; `inline constexpr std::size_t jadefx::kThemeColorCount`; `namespace jadefx::detail { void clearThemeColorCache(); }`; `void Scene::layoutPopup(PopupRecord& popup, bool restyle = true)`.

- [ ] **Step 1: Write the failing tests**

Add to `tests/style_perf_tests.cpp`:

```cpp
void TestMeasureFollowsTextBetweenFrames() {
    auto root = jadefx::make<jadefx::HBox>();
    auto label = jadefx::make<jadefx::Label>("short");
    root->getChildren().add(label);
    auto scene = jadefx::make<jadefx::Scene>(root, 600, 100);
    scene->layout(600, 100, 0);
    const double before = label->getWidth();
    label->setText("a much longer piece of text than before");
    scene->layout(600, 100, 0.1);
    Expect(label->getWidth() > before + 20, "a label laid out again after new text is wider");
}

void TestMeasureOutsideLayoutIsFresh() {
    auto root = jadefx::make<jadefx::HBox>();
    auto label = jadefx::make<jadefx::Label>("short");
    root->getChildren().add(label);
    auto scene = jadefx::make<jadefx::Scene>(root, 600, 100);
    scene->layout(600, 100, 0);
    const double before = label->measuredWidth(600);
    label->setText("a much longer piece of text than before");
    Expect(label->measuredWidth(600) > before + 20, "a measure between frames sees the new text");
}

void TestThemeColorFollowsStylesheet() {
    auto box = jadefx::make<jadefx::StackPane>();
    box->getClassList().add("x");
    auto scene = jadefx::make<jadefx::Scene>(box, 100, 100);
    scene->setStylesheet(".x { --accent-color: #ff0000; }");
    scene->layout(100, 100, 0);
    Expect(Is(box->themeColor(jadefx::ThemeColor::Accent), 255, 0, 0), "a theme color reads the custom property");
    Expect(Is(box->themeColor(jadefx::ThemeColor::Accent), 255, 0, 0), "and reads it again the same");
    scene->setStylesheet(".x { --accent-color: #00ff00; }");
    scene->layout(100, 100, 0.1);
    Expect(Is(box->themeColor(jadefx::ThemeColor::Accent), 0, 255, 0), "a changed custom property changes the color");
    scene->setStylesheet(".x { --accent-color: currentColor; color: #0000ff; }");
    scene->layout(100, 100, 0.2);
    Expect(Is(box->themeColor(jadefx::ThemeColor::Accent), 0, 0, 255), "currentColor is the node's text color");
}
```

Add the three calls to `RunStylePerfTests()`.

- [ ] **Step 2: Run on the current code**

Run the JadeFX build and test commands.
Expected: PASS. These tests pin behavior that the caches must keep.

- [ ] **Step 3: Add the measure cache to `Node.hpp`**

Add `#include <cstdint>`. In the private section, after `void drawChrome(...)`:

```cpp
    // Preferred sizes measured during a layout pass, two of each by their inputs.
    // Outside a pass nothing is cached, so a measure between frames sees every change.
    struct MeasureCache {
        struct Width {
            double available = 0;
            double result = 0;
        };
        struct Height {
            double width = 0;
            double available = 0;
            double result = 0;
        };
        Width widths[2];
        Height heights[2];
        int widthCount = 0;
        int widthNext = 0;
        int heightCount = 0;
        int heightNext = 0;
        std::uint64_t epoch = 0;
        void clear() {
            widthCount = 0;
            widthNext = 0;
            heightCount = 0;
            heightNext = 0;
        }
    };
    mutable MeasureCache measure_;
    // Scene::layout brackets its passes with these. Passes nest.
    static void beginLayoutPass();
    static void endLayoutPass();
    // True inside a pass. Clears entries left from an earlier pass.
    bool measureCacheUsable() const;
```

- [ ] **Step 4: Implement the cache in `Node.cpp`**

In the anonymous namespace:

```cpp
thread_local int gLayoutPassDepth = 0;
thread_local std::uint64_t gLayoutPassEpoch = 0;
```

Then:

```cpp
void Node::beginLayoutPass() {
    if (gLayoutPassDepth++ == 0) {
        ++gLayoutPassEpoch;
    }
}

void Node::endLayoutPass() { --gLayoutPassDepth; }

bool Node::measureCacheUsable() const {
    if (gLayoutPassDepth == 0) {
        return false;
    }
    if (measure_.epoch != gLayoutPassEpoch) {
        measure_.clear();
        measure_.epoch = gLayoutPassEpoch;
    }
    return true;
}

double Node::measuredWidth(double available) const {
    const bool cached = measureCacheUsable();
    if (cached) {
        for (int i = 0; i < measure_.widthCount; ++i) {
            if (measure_.widths[i].available == available) {
                return measure_.widths[i].result;
            }
        }
    }
    double width = 0;
    if (computed_.width.set()) {
        width = resolveSize(computed_.width, available, computed_.fontSize);
    } else {
        const double pad = computed_.padding.width() + computed_.border.width();
        const double inner = std::max(0.0, available - pad);
        width = preferredContentWidth(inner) + pad;
    }
    const double result = ClampSpec(width, computed_.minWidth, computed_.maxWidth, available, computed_.fontSize);
    if (cached) {
        measure_.widths[measure_.widthNext] = {available, result};
        measure_.widthNext = (measure_.widthNext + 1) % 2;
        measure_.widthCount = std::min(measure_.widthCount + 1, 2);
    }
    return result;
}

double Node::measuredHeight(double width, double availableHeight) const {
    const bool cached = measureCacheUsable();
    if (cached) {
        for (int i = 0; i < measure_.heightCount; ++i) {
            const MeasureCache::Height& entry = measure_.heights[i];
            if (entry.width == width && entry.available == availableHeight) {
                return entry.result;
            }
        }
    }
    const bool percent = computed_.height.kind == SizeKind::Percent || computed_.height.kind == SizeKind::Calc;
    double height = 0;
    if (computed_.height.set() && !(percent && availableHeight < 0)) {
        const double available = availableHeight < 0 ? 0 : availableHeight;
        height = resolveSize(computed_.height, available, computed_.fontSize);
    } else {
        const double pad = computed_.padding.height() + computed_.border.height();
        const double innerWidth = std::max(0.0, width - computed_.padding.width() - computed_.border.width());
        height = preferredContentHeight(innerWidth) + pad;
    }
    const double available = availableHeight < 0 ? height : availableHeight;
    const double result = ClampSpec(height, computed_.minHeight, computed_.maxHeight, available, computed_.fontSize);
    if (cached) {
        measure_.heights[measure_.heightNext] = {width, availableHeight, result};
        measure_.heightNext = (measure_.heightNext + 1) % 2;
        measure_.heightCount = std::min(measure_.heightCount + 1, 2);
    }
    return result;
}
```

In `applyStyles`, add `measure_.clear();` right after `computed_ = style;`. A node restyled mid-pass with `applyCss`, such as a rebound cell, then measures fresh.

- [ ] **Step 5: Bracket the scene's passes**

In `Scene::layout(double, double, double)`, call `beginLayoutPass();` as the first statement. Call `endLayoutPass();` just before `if (pointerValid_)`, and also just before the early `return;` in `if (!internal_)`.

- [ ] **Step 6: Restyle popups once per frame**

In `Scene.hpp`, change `void layoutPopup(PopupRecord& popup);` to:

```cpp
    // restyle is false inside layout, where the scene's own pass already styled the popup.
    void layoutPopup(PopupRecord& popup, bool restyle = true);
```

In `Scene.cpp`, make `layoutPopup` take `bool restyle`, and guard its first styling line:

```cpp
    if (restyle) {
        popup.node->applyStyles(inheritableStyle(), lastTime_);
    }
```

In `Scene::layout`'s Popups pass, call `layoutPopup(popups_[i], false);`.

- [ ] **Step 7: Cache theme colors**

In `Theme.hpp`, after the `ThemeColor` enum:

```cpp
// How many ThemeColor values there are. Success is last.
inline constexpr std::size_t kThemeColorCount = static_cast<std::size_t>(ThemeColor::Success) + 1;

namespace detail {
// Forgets the theme colors parsed from custom properties. A theme change calls this.
void clearThemeColorCache();
}  // namespace detail
```

Add `#include <cstddef>` there if it is missing. In `Node.cpp`'s anonymous namespace:

```cpp
// Theme colors parsed from one set of custom properties. Nodes share sets, so a
// few entries serve the whole tree. Each holds its set alive, so its address
// cannot be reused by another set while the entry exists.
struct ThemeCacheEntry {
    std::shared_ptr<const CssVariables> variables;
    // 0 not looked up yet, 1 a color, 2 currentColor.
    unsigned char state[kThemeColorCount] = {};
    Color colors[kThemeColorCount] = {};
};

constexpr std::size_t kThemeCacheEntries = 8;
thread_local std::vector<ThemeCacheEntry> gThemeCache;

ThemeCacheEntry& ThemeCacheFor(const std::shared_ptr<const CssVariables>& variables) {
    for (ThemeCacheEntry& entry : gThemeCache) {
        if (entry.variables == variables) {
            return entry;
        }
    }
    if (gThemeCache.size() >= kThemeCacheEntries) {
        gThemeCache.clear();
    }
    gThemeCache.emplace_back();
    gThemeCache.back().variables = variables;
    return gThemeCache.back();
}
```

After the anonymous namespace:

```cpp
namespace detail {
void clearThemeColorCache() { gThemeCache.clear(); }
}  // namespace detail
```

Replace `Node::themeColor`:

```cpp
Color Node::themeColor(ThemeColor color) const {
    const auto at = static_cast<std::size_t>(color);
    ThemeCacheEntry& entry = ThemeCacheFor(computed_.variables);
    if (entry.state[at] == 0) {
        const std::string value = computed_.variable(Theme::variableName(color));
        constexpr std::string_view kCurrentColor = "currentcolor";
        if (std::equal(value.begin(), value.end(), kCurrentColor.begin(), kCurrentColor.end(), [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == b;
            })) {
            entry.state[at] = 2;
        } else {
            bool ok = false;
            const Color parsed = value.empty() ? Color() : Color::parse(value, &ok);
            entry.colors[at] = ok ? parsed : Theme::defaultColor(color);
            entry.state[at] = 1;
        }
    }
    return entry.state[at] == 2 ? computed_.color : entry.colors[at];
}
```

At the end of `Theme::setUserAgentStylesheet` and `Scene::setUserAgentStylesheet`, call `detail::clearThemeColorCache();`.

- [ ] **Step 8: Build and run the tests**

Run the JadeFX build and test commands.
Expected: `layout tests passed`, or only the known flakes.

- [ ] **Step 9: Phase A checkpoint: engine suites**

Run the engine build and engine test commands.
Expected: all suites pass, apart from the known `six frames recorded` flake.

- [ ] **Step 10: Phase A numbers**

Run the benchmark three times (Task 3 Step 3), then `bash C:/Users/Andrew/Documents/GitHub/JadeFX_CPP/build/studio_profile.sh`. Take medians.

- [ ] **Step 11: Commit with the Phase A numbers**

```bash
cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && git add include/jadefx/scene/Node.hpp src/scene/Node.cpp include/jadefx/style/Theme.hpp src/style/Theme.cpp include/jadefx/scene/Scene.hpp src/scene/Scene.cpp tests/style_perf_tests.cpp && git commit -m "Cache measured sizes within a layout pass and parsed theme colors, and style popups once a frame

Phase A, same full pass every frame (Release, median of three):
  first frame  <F0> -> <F1> ms
  steady frame <S0> -> <S1> ms (styles <s1>, layout <l1>, popups <p1>)
  hover frame  <H0> -> <H1> ms
Studio editor at rest: Styles and layout <Z0> -> <Z1> ms

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: Dirty flags and incremental passes (opt-in per scene)

**Files:**
- Modify: `include/jadefx/scene/Node.hpp`, `src/scene/Node.cpp` (flags, marks, `applyStyles` split into `resolveStyle`, `performLayout`, `setParent`, `childrenChanged`, `measureCacheUsable`)
- Modify: `include/jadefx/scene/Scene.hpp`, `src/scene/Scene.cpp` (`setIncrementalUpdates`, clearing the scene's own layout flags)
- Create: `tests/incremental_tests.cpp`
- Modify: `CMakeLists.txt`, `tests/layout_tests.cpp`

**Interfaces:**
- Consumes: `ScratchLease` (Task 6), `MeasureCache`, `gLayoutPassEpoch` (Task 7).
- Produces (public on `Node`): `enum class StyleDirt { Self, Subtree };`, `enum class LayoutDirt { Size, Arrange };`, `void markStyleDirty(StyleDirt dirt = StyleDirt::Self);`, `void markLayoutDirty(LayoutDirt dirt = LayoutDirt::Size);`, `bool isStyleDirty() const;`, `bool isLayoutDirty() const;`, `std::uint32_t debugRestyleCount() const;`, `std::uint32_t debugLayoutCount() const;`.
- Produces (private on `Node`): `enum class StyleForce { None, Self, Subtree };`, `void applyStyles(const ComputedStyle&, double, StyleForce = StyleForce::None);`, `bool resolveStyle(const ComputedStyle&, double);`, `bool incrementalActive() const;`, `void markSubtreeLayoutDirty();`, `void childrenChanged();`, `static void setFullPass(bool full);`, and the flags `styleDirty_`, `styleSubtreeDirty_`, `childStyleDirty_`, `layoutDirty_`, `childLayoutDirty_`, `hasBounds_`.
- Produces (public on `Scene`): `void setIncrementalUpdates(bool enabled);`, `bool incrementalUpdates() const;`. Default `false` until Task 14.
- Produces (tests): `int RunIncrementalTests()`, plus the `Fixture`, `MakeFixture()`, `Seen`, `Of`, `Restyled`, `LaidOut`, and `Is` helpers in `tests/incremental_tests.cpp`. Later tasks add tests to this file.

- [ ] **Step 1: Write the failing tests `tests/incremental_tests.cpp`**

```cpp
#include "jadefx/jadefx.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Incremental passes: a frame restyles and lays out only what changed.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

bool Is(const jadefx::Color& color, int r, int g, int b) {
    auto close = [](float channel, int value) { return std::fabs(channel * 255.f - static_cast<float>(value)) < 1.5f; };
    return close(color.r, r) && close(color.g, g) && close(color.b, b);
}

const char* const kFixtureCss = R"css(
.item { padding: 2px; }
.item.on { background-color: #ff0000; }
.item.wide { padding: 12px; }
#special { background-color: #00ff00; }
.item:hover .inner { color: #0000ff; }
.item:active { background-color: #111111; }
.item:selected { background-color: #222222; }
.item:disabled .inner { color: #333333; }
.item:open { background-color: #444444; }
.panel:focus-within { background-color: #00ffff; }
.item:focus { background-color: #ff00ff; }
.fade { background-color: #000000; transition: background-color 0.5s; }
.fade.on { background-color: #ffffff; }
)css";

// Two panels in a column, each holding an item with a label inside. A test
// changes panel A's side and checks that panel B's side was not touched.
struct Fixture {
    std::shared_ptr<jadefx::Scene> scene;
    std::shared_ptr<jadefx::VBox> root;
    std::shared_ptr<jadefx::StackPane> panelA;
    std::shared_ptr<jadefx::StackPane> target;
    std::shared_ptr<jadefx::Label> targetLabel;
    std::shared_ptr<jadefx::StackPane> panelB;
    std::shared_ptr<jadefx::StackPane> sibling;
    std::shared_ptr<jadefx::Label> siblingLabel;
    double time = 0;

    void frame() {
        time += 1.0 / 60.0;
        scene->layout(300, 200, time);
    }
};

std::shared_ptr<jadefx::StackPane> Item(const char* text, std::shared_ptr<jadefx::Label>& label) {
    auto item = jadefx::make<jadefx::StackPane>();
    item->getClassList().add("item");
    item->setPrefSize(120, 40);
    label = jadefx::make<jadefx::Label>(text);
    label->getClassList().add("inner");
    item->getChildren().add(label);
    return item;
}

std::shared_ptr<jadefx::StackPane> Panel(const std::shared_ptr<jadefx::StackPane>& item) {
    auto panel = jadefx::make<jadefx::StackPane>();
    panel->getClassList().add("panel");
    panel->setPrefSize(300, 90);
    panel->getChildren().add(item);
    return panel;
}

Fixture MakeFixture() {
    Fixture f;
    f.root = jadefx::make<jadefx::VBox>();
    f.target = Item("target", f.targetLabel);
    f.sibling = Item("sibling", f.siblingLabel);
    f.panelA = Panel(f.target);
    f.panelB = Panel(f.sibling);
    f.root->getChildren().add(f.panelA);
    f.root->getChildren().add(f.panelB);
    f.scene = jadefx::make<jadefx::Scene>(f.root, 300, 200);
    f.scene->setStylesheet(kFixtureCss);
    f.scene->setIncrementalUpdates(true);
    f.scene->layout(300, 200, 0);
    return f;
}

struct Seen {
    std::uint32_t restyles = 0;
    std::uint32_t layouts = 0;
};

Seen Of(const jadefx::Node& node) { return {node.debugRestyleCount(), node.debugLayoutCount()}; }
bool Restyled(const jadefx::Node& node, const Seen& before) { return node.debugRestyleCount() > before.restyles; }
bool LaidOut(const jadefx::Node& node, const Seen& before) { return node.debugLayoutCount() > before.layouts; }

void TestIdleFrameDoesNothing() {
    Fixture f = MakeFixture();
    f.frame();
    std::vector<std::pair<const jadefx::Node*, Seen>> before;
    for (const jadefx::Node* node : std::vector<const jadefx::Node*>{
             f.scene.get(), f.root.get(), f.panelA.get(), f.target.get(), f.targetLabel.get(), f.panelB.get(),
             f.sibling.get(), f.siblingLabel.get()}) {
        before.emplace_back(node, Of(*node));
    }
    f.frame();
    f.frame();
    bool idle = true;
    for (const auto& entry : before) {
        idle = idle && !Restyled(*entry.first, entry.second) && !LaidOut(*entry.first, entry.second);
    }
    Expect(idle, "an idle frame restyles and lays out nothing");
}

void TestNewNodesStyleAndLayout() {
    Fixture f = MakeFixture();
    Expect(f.target->debugRestyleCount() == 1 && f.target->debugLayoutCount() == 1,
           "the first frame styles and lays out every new node once");
}

void TestChildAddTouchesOnlyItsParent() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen target = Of(*f.target);
    const Seen targetLabel = Of(*f.targetLabel);
    const Seen panelA = Of(*f.panelA);
    const Seen panelB = Of(*f.panelB);
    const Seen sibling = Of(*f.sibling);
    auto added = jadefx::make<jadefx::StackPane>();
    f.panelA->getChildren().add(added);
    f.frame();
    Expect(added->debugRestyleCount() == 1 && added->debugLayoutCount() == 1, "an added child is styled and laid out");
    Expect(Restyled(*f.target, target) && Restyled(*f.targetLabel, targetLabel),
           "an added child restyles its siblings' subtrees, since :nth-child may change");
    Expect(LaidOut(*f.panelA, panelA), "an added child lays out its parent");
    Expect(!Restyled(*f.sibling, sibling) && !LaidOut(*f.panelB, panelB), "an added child leaves the other panel alone");
}

void TestChildRemoveTouchesOnlyItsParent() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen target = Of(*f.target);
    const Seen sibling = Of(*f.sibling);
    f.target->getChildren().clear();
    f.frame();
    Expect(LaidOut(*f.target, target), "removing a child lays out its parent");
    Expect(!LaidOut(*f.sibling, sibling) && !Restyled(*f.sibling, sibling), "removing a child leaves the other panel alone");
}

void TestFullPassWhenIncrementalOff() {
    Fixture f = MakeFixture();
    f.scene->setIncrementalUpdates(false);
    f.target->setPrefSize(150, 40);
    f.frame();
    Expect(f.target->computedStyle().width.pixels == 150, "with incremental passes off every node restyles each frame");
}

}  // namespace

int RunIncrementalTests() {
    TestIdleFrameDoesNothing();
    TestNewNodesStyleAndLayout();
    TestChildAddTouchesOnlyItsParent();
    TestChildRemoveTouchesOnlyItsParent();
    TestFullPassWhenIncrementalOff();
    return gFailures;
}
```

Register the file: add `tests/incremental_tests.cpp` to `add_executable(jadefx-tests ...)` after `tests/style_perf_tests.cpp`. In `tests/layout_tests.cpp`, add `int RunIncrementalTests();` and `gFailures += RunIncrementalTests();` after the `RunStylePerfTests` lines.

- [ ] **Step 2: Build to verify it fails**

Run the JadeFX build command.
Expected: compile errors `'setIncrementalUpdates': is not a member of 'jadefx::Scene'` and `'debugRestyleCount': is not a member`.

- [ ] **Step 3: Declare the flags and marks in `Node.hpp`**

Public, after `const ComputedStyle& computedStyle() const`:

```cpp
    // How far a style change reaches. Self restyles this node, and its children too
    // when what they inherit changes. Subtree restyles every node under it as well,
    // for a change a descendant selector can see, such as a class or :hover.
    enum class StyleDirt { Self, Subtree };
    // Size: this node's preferred size may change, so its ancestors lay out again.
    // Arrange: its size stays the same, and only its children are placed again.
    enum class LayoutDirt { Size, Arrange };
    // Restyles this node at the next layout. A control calls this when it changes
    // something its style depends on.
    void markStyleDirty(StyleDirt dirt = StyleDirt::Self);
    // Lays this node out again at the next layout. A control calls this when it
    // changes something its layoutChildren or preferred size depends on. Size is
    // always correct; Arrange is cheaper when the preferred size cannot change.
    void markLayoutDirty(LayoutDirt dirt = LayoutDirt::Size);
    bool isStyleDirty() const { return styleDirty_; }
    bool isLayoutDirty() const { return layoutDirty_; }
    // How many times an incremental pass restyled this node and ran its
    // layoutChildren. For tests.
    std::uint32_t debugRestyleCount() const { return restyleCount_; }
    std::uint32_t debugLayoutCount() const { return layoutCount_; }
```

Private: replace `void applyStyles(const ComputedStyle& inherited, double timeSeconds);` with:

```cpp
    // None styles this node only if it is dirty. Self restyles it. Subtree restyles
    // it and every node under it.
    enum class StyleForce { None, Self, Subtree };
    void applyStyles(const ComputedStyle& inherited, double timeSeconds, StyleForce force = StyleForce::None);
    // Resolves this node's own style. Returns true when what its children inherit changed.
    bool resolveStyle(const ComputedStyle& inherited, double timeSeconds);
    // True when this node's scene skips clean nodes and no full pass is running.
    bool incrementalActive() const;
    // Lays out this node and every node under it again, as a subtree that moved needs.
    void markSubtreeLayoutDirty();
    // The child list changed: :nth-child may match differently and the layout moves.
    void childrenChanged();
    // While set, every pass ignores dirty flags, as with incremental passes off.
    static void setFullPass(bool full);
```

and, with the other private members:

```cpp
    // Dirty flags. A new node starts dirty. An ancestor of a dirty node has its
    // child flag set, so a pass can walk down to it and skip clean branches.
    bool styleDirty_ = true;
    bool styleSubtreeDirty_ = false;
    bool childStyleDirty_ = false;
    bool layoutDirty_ = true;
    bool childLayoutDirty_ = false;
    bool hasBounds_ = false;
    std::uint32_t restyleCount_ = 0;
    std::uint32_t layoutCount_ = 0;
```

- [ ] **Step 4: Implement the marks in `Node.cpp`**

In the anonymous namespace, add `thread_local bool gFullPass = false;` and:

```cpp
bool SameColor(const Color& a, const Color& b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

bool SameVariables(const std::shared_ptr<const CssVariables>& a, const std::shared_ptr<const CssVariables>& b) {
    if (a == b) {
        return true;
    }
    if (!a || !b) {
        return (!a || a->empty()) && (!b || b->empty());
    }
    return *a == *b;
}

// What inheritableStyle passes down: text color, font, smoothing, cursor, and custom properties.
bool SameInheritable(const ComputedStyle& a, const ComputedStyle& b) {
    return SameColor(a.color, b.color) && a.fontSize == b.fontSize && a.fontFamily == b.fontFamily &&
           a.subpixel == b.subpixel && a.cursor == b.cursor && SameVariables(a.variables, b.variables);
}
```

Then the members:

```cpp
void Node::setFullPass(bool full) { gFullPass = full; }

bool Node::incrementalActive() const { return !gFullPass && scene_ != nullptr && scene_->incremental_; }

void Node::markStyleDirty(StyleDirt dirt) {
    styleDirty_ = true;
    if (dirt == StyleDirt::Subtree) {
        styleSubtreeDirty_ = true;
    }
    for (Node* node = parent_; node != nullptr && !node->childStyleDirty_; node = node->parent_) {
        node->childStyleDirty_ = true;
    }
}

void Node::markLayoutDirty(LayoutDirt dirt) {
    layoutDirty_ = true;
    if (dirt == LayoutDirt::Arrange) {
        for (Node* node = parent_; node != nullptr && !node->childLayoutDirty_ && !node->layoutDirty_;
             node = node->parent_) {
            node->childLayoutDirty_ = true;
        }
        return;
    }
    // A preferred size feeds every ancestor's, so the whole chain measures and lays
    // out again. The walk always reaches the root: it is short, and stopping early
    // could leave an ancestor's measure cached from earlier in this pass.
    measure_.clear();
    for (Node* node = parent_; node != nullptr; node = node->parent_) {
        node->measure_.clear();
        node->layoutDirty_ = true;
    }
}

void Node::markSubtreeLayoutDirty() {
    std::vector<Node*> stack{this};
    while (!stack.empty()) {
        Node* node = stack.back();
        stack.pop_back();
        node->layoutDirty_ = true;
        node->measure_.clear();
        node->visitChildren([&](Node* child) { stack.push_back(child); });
    }
    markLayoutDirty(LayoutDirt::Size);
}

void Node::childrenChanged() {
    if (tearingDown_) {
        return;
    }
    visitChildren([](Node* child) { child->markStyleDirty(StyleDirt::Subtree); });
    markLayoutDirty(LayoutDirt::Size);
}
```

Change `measureCacheUsable` so that only full passes start fresh:

```cpp
bool Node::measureCacheUsable() const {
    if (gLayoutPassDepth == 0) {
        return false;
    }
    // An incremental pass keeps measures until a mark clears them. A full pass
    // starts each pass fresh, as every node is styled again.
    if (!incrementalActive() && measure_.epoch != gLayoutPassEpoch) {
        measure_.clear();
        measure_.epoch = gLayoutPassEpoch;
    }
    return true;
}
```

- [ ] **Step 5: Split `applyStyles` and skip clean nodes**

Rename the current `Node::applyStyles` to `bool Node::resolveStyle(const ComputedStyle& inherited, double timeSeconds)`. Delete its child walk, from `const ComputedStyle pass = ...` to the end. Replace `computed_ = style; measure_.clear(); styleDidApply();` with:

```cpp
    const bool inheritChanged = !SameInheritable(computed_, style);
    computed_ = style;
    measure_.clear();
    if (incrementalActive()) {
        ++restyleCount_;
        // Every restyle may move the layout. Only layout-affecting changes will, once
        // the restyle compares what changed.
        markLayoutDirty(LayoutDirt::Size);
    }
    styleDidApply();
    return inheritChanged;
```

Add the new `applyStyles`:

```cpp
void Node::applyStyles(const ComputedStyle& inherited, double timeSeconds, StyleForce force) {
    if (!incrementalActive()) {
        force = StyleForce::Subtree;
    }
    const bool restyle = force != StyleForce::None || styleDirty_;
    if (!restyle && !childStyleDirty_) {
        return;
    }
    StyleForce childForce =
        force == StyleForce::Subtree || styleSubtreeDirty_ ? StyleForce::Subtree : StyleForce::None;
    // Cleared before the work, so a mark made while styling holds for the next frame.
    styleDirty_ = false;
    styleSubtreeDirty_ = false;
    childStyleDirty_ = false;
    if (restyle && resolveStyle(inherited, timeSeconds) && childForce == StyleForce::None) {
        childForce = StyleForce::Self;
    }
    const ComputedStyle pass = asSubScene() != nullptr ? rootInheritance() : inheritableStyle();
    ScratchLease lease;
    std::vector<Node*>& kids = (*lease).kids;
    visitChildren([&](Node* child) { kids.push_back(child); });
    for (Node* child : kids) {
        child->applyStyles(pass, timeSeconds, childForce);
    }
}
```

`Node::applyCss` keeps its full-subtree behavior:

```cpp
void Node::applyCss() {
    const double time = scene_ != nullptr ? scene_->timeSeconds() : 0.0;
    applyStyles(inheritedFromParent(), time, StyleForce::Subtree);
}
```

- [ ] **Step 6: Skip clean layouts in `performLayout`**

```cpp
void Node::performLayout(double x, double y, double width, double height) {
    width = std::max(0.0, width);
    height = std::max(0.0, height);
    const bool resized = !hasBounds_ || width != width_ || height != height_;
    x_ = x;
    y_ = y;
    width_ = width;
    height_ = height;
    hasBounds_ = true;
    if (!incrementalActive()) {
        layoutDirty_ = false;
        childLayoutDirty_ = false;
        layoutChildren();
        return;
    }
    // A clean node keeps its children where they are. Its position is relative to its
    // parent, so moving it moves them too.
    if (!resized && !layoutDirty_ && !childLayoutDirty_) {
        return;
    }
    const bool whole = resized || layoutDirty_;
    layoutDirty_ = false;
    childLayoutDirty_ = false;
    if (whole) {
        ++layoutCount_;
        layoutChildren();
        return;
    }
    // Only children below need it: lay each dirty one out again where it is.
    ScratchLease lease;
    std::vector<Node*>& kids = (*lease).kids;
    visitChildren([&](Node* child) { kids.push_back(child); });
    for (Node* child : kids) {
        if (child->layoutDirty_ || child->childLayoutDirty_) {
            child->performLayout(child->x_, child->y_, child->width_, child->height_);
        }
    }
}
```

- [ ] **Step 7: Mark on reparenting in `setParent`**

At the top of `Node::setParent`, add `Node* const previousParent = parent_;`. Before the final `std::vector<Node*> kids;` walk, add:

```cpp
    if (previousParent != parent_) {
        // A node that moves is styled and placed again where it lands, and the lists
        // it left and joined change order.
        if (previousParent != nullptr) {
            previousParent->childrenChanged();
        }
        if (parent_ != nullptr) {
            parent_->childrenChanged();
        }
        markStyleDirty(StyleDirt::Subtree);
        markSubtreeLayoutDirty();
    }
```

- [ ] **Step 8: The scene switch**

In `Scene.hpp` public:

```cpp
    // Skip nodes whose style and layout did not change since the last frame. Off
    // restyles and lays out every node every frame.
    void setIncrementalUpdates(bool enabled);
    bool incrementalUpdates() const { return incremental_; }
```

Private: `bool incremental_ = false;`. In `Scene.cpp`:

```cpp
void Scene::setIncrementalUpdates(bool enabled) {
    if (incremental_ == enabled) {
        return;
    }
    incremental_ = enabled;
    markStyleDirty(StyleDirt::Subtree);
    markSubtreeLayoutDirty();
}
```

In `Scene::layout`'s Layout pass, before `internal_->performLayout(...)`, add the following. The scene places its root itself every frame.

```cpp
        layoutDirty_ = false;
        childLayoutDirty_ = false;
```

- [ ] **Step 9: Build and run the tests**

Run the JadeFX build and test commands.
Expected: `layout tests passed`, or only the known flakes. Existing tests run with incremental passes off, so their behavior is unchanged.

- [ ] **Step 10: Commit**

```bash
cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && git add include/jadefx/scene/Node.hpp src/scene/Node.cpp include/jadefx/scene/Scene.hpp src/scene/Scene.cpp tests/incremental_tests.cpp tests/layout_tests.cpp CMakeLists.txt && git commit -m "Add style and layout dirty flags so a scene can skip clean nodes, off by default

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: Verification mode

**Files:**
- Create: `src/scene/IncrementalCheck.hpp`, `src/scene/IncrementalCheck.cpp`
- Modify: `CMakeLists.txt` (add `src/scene/IncrementalCheck.cpp` to the jadefx library sources, next to `src/scene/Node.cpp`)
- Modify: `include/jadefx/scene/Node.hpp` (`friend class IncrementalCheck;`), `include/jadefx/scene/Scene.hpp` (friend; private `stylePass`, `placePass`), `src/scene/Scene.cpp`
- Test: `tests/incremental_tests.cpp`

**Interfaces:**
- Consumes: `Node::setFullPass`, `beginLayoutPass`, `endLayoutPass` (Tasks 7–8).
- Produces: `struct NodeSnapshot { const Node* node; std::string path; ComputedStyle style; double x, y, width, height; bool animating; };` and `class IncrementalCheck { static bool requested(); static std::vector<NodeSnapshot> snapshot(Node& root); static std::string compare(const std::vector<NodeSnapshot>&, const std::vector<NodeSnapshot>&); static const char* firstStyleDifference(const ComputedStyle&, const ComputedStyle&); static std::string verify(Scene& scene); };`. Also `void Scene::stylePass(double timeSeconds)` and `void Scene::placePass()`, both private. Task 11 adds focus fields to the snapshot.

- [ ] **Step 1: Write the failing tests**

Add `#include "scene/IncrementalCheck.hpp"` to `tests/incremental_tests.cpp`. Add:

```cpp
void TestVerifyAgreesOnCleanScene() {
    Fixture f = MakeFixture();
    f.frame();
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "a full pass agrees with an incremental one");
}

void TestCompareNamesPathAndField() {
    Fixture f = MakeFixture();
    const std::vector<jadefx::NodeSnapshot> shots = jadefx::IncrementalCheck::snapshot(*f.scene);
    std::vector<jadefx::NodeSnapshot> changed = shots;
    for (jadefx::NodeSnapshot& shot : changed) {
        if (shot.node == f.targetLabel.get()) {
            shot.style.padding.left += 3;
        }
    }
    const std::string padding = jadefx::IncrementalCheck::compare(shots, changed);
    Expect(padding.find("padding") != std::string::npos, "a style difference names the field");
    Expect(padding.find("label.inner") != std::string::npos, "a style difference names the node's path");
    changed = shots;
    for (jadefx::NodeSnapshot& shot : changed) {
        if (shot.node == f.target.get()) {
            shot.width += 1;
        }
    }
    Expect(jadefx::IncrementalCheck::compare(shots, changed).find("bounds") != std::string::npos,
           "a bounds difference says so");
}
```

Add both to `RunIncrementalTests()`.

- [ ] **Step 2: Build to verify it fails**

Run the JadeFX build command.
Expected: `cannot open include file 'scene/IncrementalCheck.hpp'`.

- [ ] **Step 3: Write `src/scene/IncrementalCheck.hpp`**

```cpp
#pragma once

#include "jadefx/scene/Scene.hpp"

#include <string>
#include <vector>

namespace jadefx {

// One node's resolved style and bounds, as a pass left them.
struct NodeSnapshot {
    const Node* node = nullptr;
    std::string path;
    ComputedStyle style;
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
    // The node asked to be laid out again next frame, as an animation driven by the
    // clock does. A second pass in the same frame may move it, so its subtree's
    // bounds are not compared.
    bool animating = false;
};

// The check behind JADEFX_VERIFY_INCREMENTAL: a full pass over a scene that an
// incremental pass just styled and laid out must change nothing.
class IncrementalCheck {
public:
    // True in a debug build when JADEFX_VERIFY_INCREMENTAL is 1.
    static bool requested();
    // Every node under root, root first, in visitChildren order.
    static std::vector<NodeSnapshot> snapshot(Node& root);
    // Empty when the two agree. Otherwise "<path>: <field>" for the first difference.
    static std::string compare(const std::vector<NodeSnapshot>& incremental, const std::vector<NodeSnapshot>& full);
    // The first ComputedStyle field that differs, or null.
    static const char* firstStyleDifference(const ComputedStyle& a, const ComputedStyle& b);
    // Styles and lays out scene again ignoring dirty flags, and compares with before.
    static std::string verify(Scene& scene);

private:
    static void collect(Node& node, const std::string& path, std::vector<NodeSnapshot>& out);
};

}  // namespace jadefx
```

- [ ] **Step 4: Write `src/scene/IncrementalCheck.cpp`**

```cpp
#include "scene/IncrementalCheck.hpp"

#include <algorithm>
#include <cstdlib>

namespace jadefx {
namespace {

bool SameColor(const Color& a, const Color& b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

bool SameSize(const SizeSpec& a, const SizeSpec& b) {
    return a.kind == b.kind && a.pixels == b.pixels && a.percent == b.percent && a.em == b.em;
}

bool SameInsets(const Insets& a, const Insets& b) {
    return a.top == b.top && a.right == b.right && a.bottom == b.bottom && a.left == b.left;
}

bool SameBackground(const Background& a, const Background& b) {
    if (!SameColor(a.color, b.color) || a.hasColor != b.hasColor || a.stopCount != b.stopCount ||
        a.angleDeg != b.angleDeg || a.gradient != b.gradient || a.visible != b.visible) {
        return false;
    }
    const int stops = std::min(a.stopCount, kMaxGradientStops);
    for (int i = 0; i < stops; ++i) {
        if (!SameColor(a.stops[i], b.stops[i]) || a.stopAt[i] != b.stopAt[i]) {
            return false;
        }
    }
    return true;
}

bool SameShadows(const std::vector<BoxShadow>& a, const std::vector<BoxShadow>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].offsetX != b[i].offsetX || a[i].offsetY != b[i].offsetY || a[i].blur != b[i].blur ||
            a[i].spread != b[i].spread || a[i].inset != b[i].inset || !SameColor(a[i].color, b[i].color)) {
            return false;
        }
    }
    return true;
}

bool SameTransitions(const std::shared_ptr<const TransitionTable>& a, const std::shared_ptr<const TransitionTable>& b) {
    if (a == b) {
        return true;
    }
    const TransitionTable empty;
    const TransitionTable& x = a ? *a : empty;
    const TransitionTable& y = b ? *b : empty;
    for (std::size_t i = 0; i < kPropertyIdCount; ++i) {
        if (x.set[i] != y.set[i]) {
            return false;
        }
        if (x.set[i] && (x.timing[i].duration != y.timing[i].duration || x.timing[i].delay != y.timing[i].delay)) {
            return false;
        }
    }
    return true;
}

bool SameVariables(const std::shared_ptr<const CssVariables>& a, const std::shared_ptr<const CssVariables>& b) {
    if (a == b) {
        return true;
    }
    if (!a || !b) {
        return (!a || a->empty()) && (!b || b->empty());
    }
    return *a == *b;
}

std::string Segment(const Node& node, int index) {
    std::string text = node.getElementType();
    if (!node.getElementId().empty()) {
        text += "#" + node.getElementId();
    }
    for (const std::string& name : node.getClassList().items()) {
        text += "." + name;
    }
    return text + "[" + std::to_string(index) + "]";
}

}  // namespace

bool IncrementalCheck::requested() {
#ifdef NDEBUG
    return false;
#else
    static const bool on = [] {
        const char* value = std::getenv("JADEFX_VERIFY_INCREMENTAL");
        return value != nullptr && std::string(value) == "1";
    }();
    return on;
#endif
}

void IncrementalCheck::collect(Node& node, const std::string& path, std::vector<NodeSnapshot>& out) {
    NodeSnapshot shot;
    shot.node = &node;
    shot.path = path;
    shot.style = node.computed_;
    shot.x = node.x_;
    shot.y = node.y_;
    shot.width = node.width_;
    shot.height = node.height_;
    shot.animating = node.layoutDirty_;
    out.push_back(std::move(shot));
    int index = 0;
    node.visitChildren([&](Node* child) { collect(*child, path + " > " + Segment(*child, index++), out); });
}

std::vector<NodeSnapshot> IncrementalCheck::snapshot(Node& root) {
    std::vector<NodeSnapshot> out;
    collect(root, Segment(root, 0), out);
    return out;
}

const char* IncrementalCheck::firstStyleDifference(const ComputedStyle& a, const ComputedStyle& b) {
    if (!SameBackground(a.background, b.background)) return "background";
    if (!SameColor(a.color, b.color)) return "color";
    if (!SameColor(a.borderColor, b.borderColor)) return "border-color";
    if (a.fontSize != b.fontSize) return "font-size";
    if (a.fontFamily != b.fontFamily) return "font-family";
    if (a.subpixel != b.subpixel) return "font-smoothing";
    for (int i = 0; i < 4; ++i) {
        if (!SameSize(a.radius[i], b.radius[i])) return "border-radius";
    }
    if (!SameInsets(a.padding, b.padding)) return "padding";
    if (!SameInsets(a.border, b.border)) return "border-width";
    if (a.borderStyle != b.borderStyle) return "border-style";
    if (!SameShadows(a.shadows, b.shadows)) return "box-shadow";
    if (a.spacing != b.spacing) return "spacing";
    if (a.rowGap != b.rowGap) return "row-gap";
    if (a.columnGap != b.columnGap) return "column-gap";
    if (a.alignment != b.alignment || a.alignmentFromCss != b.alignmentFromCss) return "alignment";
    if (a.opacity != b.opacity) return "opacity";
    if (a.imageColorSet != b.imageColorSet || a.imageColorCurrent != b.imageColorCurrent ||
        !SameColor(a.imageColor, b.imageColor)) return "image-color";
    if (a.cursor != b.cursor) return "cursor";
    if (!SameSize(a.width, b.width)) return "width";
    if (!SameSize(a.height, b.height)) return "height";
    if (!SameSize(a.minWidth, b.minWidth)) return "min-width";
    if (!SameSize(a.minHeight, b.minHeight)) return "min-height";
    if (!SameSize(a.maxWidth, b.maxWidth)) return "max-width";
    if (!SameSize(a.maxHeight, b.maxHeight)) return "max-height";
    if (a.orientationFromCss != b.orientationFromCss || a.orientation != b.orientation) return "orientation";
    if (a.indeterminateBarLengthSet != b.indeterminateBarLengthSet ||
        !SameSize(a.indeterminateBarLength, b.indeterminateBarLength) ||
        a.indeterminateBarEscapeSet != b.indeterminateBarEscapeSet ||
        a.indeterminateBarEscape != b.indeterminateBarEscape || a.indeterminateBarFlipSet != b.indeterminateBarFlipSet ||
        a.indeterminateBarFlip != b.indeterminateBarFlip ||
        a.indeterminateBarAnimationTimeSet != b.indeterminateBarAnimationTimeSet ||
        a.indeterminateBarAnimationTime != b.indeterminateBarAnimationTime) return "indeterminate-bar";
    if (!SameTransitions(a.transitions, b.transitions)) return "transition";
    if (!SameVariables(a.variables, b.variables)) return "custom properties";
    return nullptr;
}

std::string IncrementalCheck::compare(const std::vector<NodeSnapshot>& incremental,
                                      const std::vector<NodeSnapshot>& full) {
    std::string skipBelow;
    const std::size_t count = std::min(incremental.size(), full.size());
    for (std::size_t i = 0; i < count; ++i) {
        const NodeSnapshot& a = incremental[i];
        const NodeSnapshot& b = full[i];
        if (a.node != b.node) {
            return b.path + ": the tree changed during the full pass";
        }
        if (const char* field = firstStyleDifference(a.style, b.style)) {
            return a.path + ": " + field;
        }
        const bool skipped = !skipBelow.empty() && a.path.rfind(skipBelow, 0) == 0;
        if (!skipped) {
            skipBelow.clear();
            if (a.animating) {
                skipBelow = a.path;
                continue;
            }
            if (a.x != b.x || a.y != b.y || a.width != b.width || a.height != b.height) {
                return a.path + ": bounds";
            }
        }
    }
    if (incremental.size() != full.size()) {
        return "scene: the node count changed during the full pass";
    }
    return {};
}

std::string IncrementalCheck::verify(Scene& scene) {
    const std::vector<NodeSnapshot> incremental = snapshot(scene);
    Node::beginLayoutPass();
    Node::setFullPass(true);
    scene.stylePass(scene.lastTime_);
    scene.placePass();
    Node::setFullPass(false);
    Node::endLayoutPass();
    return compare(incremental, snapshot(scene));
}

}  // namespace jadefx
```

Add `friend class IncrementalCheck;` to `Node` next to `friend class Scene;`, and to `Scene`'s private section next to `friend class Node;`. Add `src/scene/IncrementalCheck.cpp` to the jadefx library's source list in `CMakeLists.txt`. Find it with `grep -n "src/scene/Node.cpp" CMakeLists.txt`.

- [ ] **Step 5: Split `Scene::layout` into passes and call the check**

In `Scene.hpp` private:

```cpp
    // Styles the scene, inside the Styles pass report.
    void stylePass(double timeSeconds);
    // Places the root and the popups, inside the Layout and Popups pass reports.
    void placePass();
```

In `Scene.cpp`, add `#include "scene/IncrementalCheck.hpp"`, `#include <cstdio>`, and `#include <cstdlib>`, then:

```cpp
void Scene::stylePass(double timeSeconds) {
    PassScope pass(passHook_, LayoutPass::Styles);
    applyStyles(rootInheritance(), timeSeconds);
}

void Scene::placePass() {
    if (!internal_) {
        return;
    }
    {
        PassScope pass(passHook_, LayoutPass::Layout);
        const double right = computed_.padding.right + computed_.border.right;
        const double bottom = computed_.padding.bottom + computed_.border.bottom;
        const double x = safe_.left + contentLeft();
        const double y = safe_.top + contentTop();
        const double innerWidth = std::max(0.0, width_ - safe_.left - safe_.right - contentLeft() - right);
        const double innerHeight = std::max(0.0, height_ - safe_.top - safe_.bottom - contentTop() - bottom);
        // The scene places its root itself every frame.
        layoutDirty_ = false;
        childLayoutDirty_ = false;
        internal_->performLayout(x, y, innerWidth, innerHeight);
    }
    PassScope pass(passHook_, LayoutPass::Popups);
    for (std::size_t i = 0; i < popups_.size(); ++i) {
        layoutPopup(popups_[i], false);
    }
}

void Scene::layout(double width, double height, double timeSeconds) {
    beginLayoutPass();
    stylePass(timeSeconds);
    x_ = 0;
    y_ = 0;
    width_ = std::max(0.0, width);
    height_ = std::max(0.0, height);
    lastWidth_ = width_;
    lastHeight_ = height_;
    lastTime_ = timeSeconds;
    laidOut_ = true;
    placePass();
    endLayoutPass();
    if (incremental_ && IncrementalCheck::requested()) {
        const std::string difference = IncrementalCheck::verify(*this);
        if (!difference.empty()) {
            std::fprintf(stderr, "JADEFX_VERIFY_INCREMENTAL: %s\n", difference.c_str());
            std::abort();
        }
    }
    if (pointerValid_) {
        updateHoverPopup(pick(pointerX_, pointerY_));
    }
    // A copy, since a listener may add or remove listeners.
    const auto listeners = pulseListeners_;
    for (const auto& listener : listeners) {
        if (listener.second) {
            listener.second();
        }
    }
}
```

- [ ] **Step 6: Build and run the tests**

Run the JadeFX build and test commands.
Expected: `layout tests passed`, or only the known flakes.

- [ ] **Step 7: Commit**

```bash
cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && git add src/scene/IncrementalCheck.hpp src/scene/IncrementalCheck.cpp include/jadefx/scene/Node.hpp include/jadefx/scene/Scene.hpp src/scene/Scene.cpp tests/incremental_tests.cpp CMakeLists.txt && git commit -m "Check incremental passes against a full pass under JADEFX_VERIFY_INCREMENTAL in debug builds

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 10: Style invalidation sources

**Files:**
- Modify: `include/jadefx/scene/Node.hpp` (move inline setters out of the header: `setAlignment`, `setPadding`, `setBorder`, `setOpacity`, `setPressed`, `setSelected`, `setElementId`, `setSpacingValue`, `setDefaultCursor`)
- Modify: `src/scene/Node.cpp` (constructor class-list listener, setters, `setPseudoState`, `setDisable`, `syncHover`, `setPressedChain`, `setStyle`, `setStylesheet`, `setCursor`, `setBackground`, the `setPref*`/`setMin*`/`setMax*` family, `setFontInternal`, `setTextFillInternal`, `setSubpixelRenderingInternal`)
- Modify: `include/jadefx/style/Theme.hpp`, `src/style/Theme.cpp` (`Theme::generation()`)
- Modify: `src/scene/Scene.cpp` (`setUserAgentStylesheet`, theme generation check), `include/jadefx/scene/Scene.hpp` (`themeGeneration_`)
- Modify: `src/scene/SubScene.cpp` (`setUserAgentStylesheet`)
- Test: `tests/incremental_tests.cpp`

**Interfaces:**
- Consumes: `markStyleDirty`, `StyleDirt`, the fixture helpers (Task 8), and `IncrementalCheck::verify` (Task 9).
- Produces: `static std::uint64_t Theme::generation();`. Every Node setter that feeds `resolveStyle` or selector matching now marks the node dirty.

- [ ] **Step 1: Write the failing tests**

Add to `tests/incremental_tests.cpp`:

```cpp
// Runs mutate between two frames and checks that only panel A's side restyled.
void ExpectRestyle(const char* what, const std::function<void(Fixture&)>& mutate, bool labelToo) {
    Fixture f = MakeFixture();
    f.frame();
    const Seen target = Of(*f.target);
    const Seen label = Of(*f.targetLabel);
    const Seen sibling = Of(*f.sibling);
    const Seen siblingLabel = Of(*f.siblingLabel);
    mutate(f);
    f.frame();
    const std::string name(what);
    Expect(Restyled(*f.target, target), (name + " restyles the node").c_str());
    if (labelToo) {
        Expect(Restyled(*f.targetLabel, label), (name + " restyles the node's subtree").c_str());
    }
    Expect(!Restyled(*f.sibling, sibling) && !Restyled(*f.siblingLabel, siblingLabel),
           (name + " leaves the other panel alone").c_str());
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), (name + " matches a full pass").c_str());
}

void TestStyleSources() {
    ExpectRestyle("adding a class", [](Fixture& f) { f.target->getClassList().add("on"); }, true);
    ExpectRestyle("removing a class",
                  [](Fixture& f) { f.target->getClassList().removeIf([](const std::string& n) { return n == "item"; }); },
                  true);
    ExpectRestyle("changing the id", [](Fixture& f) { f.target->setElementId("special"); }, true);
    ExpectRestyle("an inline style", [](Fixture& f) { f.target->setStyle("background-color: #123456;"); }, false);
    ExpectRestyle("a node's stylesheet", [](Fixture& f) { f.target->setStylesheet(".inner { color: #654321; }"); }, true);
    ExpectRestyle("pressing", [](Fixture& f) { f.target->setPressed(true); }, true);
    ExpectRestyle("selecting", [](Fixture& f) { f.target->setSelected(true); }, true);
    ExpectRestyle("disabling", [](Fixture& f) { f.target->setDisable(true); }, true);
    ExpectRestyle("a pseudo-class state", [](Fixture& f) { f.target->setPseudoState("open", true); }, true);
    ExpectRestyle("a size set from code", [](Fixture& f) { f.target->setPrefSize(150, 40); }, false);
    ExpectRestyle("a background set from code", [](Fixture& f) { f.target->setBackground(jadefx::Color::black()); }, false);
}

void TestStyleSourceResults() {
    Fixture f = MakeFixture();
    f.target->setDisable(true);
    f.frame();
    Expect(Is(f.targetLabel->computedStyle().color, 51, 51, 51), "a descendant of a disabled node matches :disabled");
    f.target->setPressed(true);
    f.frame();
    Expect(Is(f.target->computedStyle().background.color, 17, 17, 17), "a pressed node matches :active");
}

void TestHoverRestylesDescendants() {
    Fixture f = MakeFixture();
    // Inside panel A but outside the item, so the root and the panel are already hovered.
    f.scene->noteMove(f.panelA->getAbsoluteX() + 2, f.panelA->getAbsoluteY() + 2);
    f.frame();
    const Seen label = Of(*f.targetLabel);
    const Seen sibling = Of(*f.sibling);
    f.scene->noteMove(f.target->getAbsoluteX() + 4, f.target->getAbsoluteY() + 4);
    f.frame();
    Expect(Restyled(*f.targetLabel, label), "hovering an item restyles the label inside it");
    Expect(Is(f.targetLabel->computedStyle().color, 0, 0, 255), "the label matches .item:hover .inner");
    Expect(!Restyled(*f.sibling, sibling), "hovering one item leaves the other alone");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "hover matches a full pass");
}

void TestInheritedChangePropagates() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen label = Of(*f.targetLabel);
    const Seen siblingLabel = Of(*f.siblingLabel);
    f.panelA->setStyle("color: #00aa00;");
    f.frame();
    Expect(Restyled(*f.targetLabel, label) && Is(f.targetLabel->computedStyle().color, 0, 170, 0),
           "a changed inherited color restyles the descendants that inherit it");
    Expect(!Restyled(*f.siblingLabel, siblingLabel), "and leaves the other panel alone");
}

void TestUserAgentSheetRestylesEverything() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen sibling = Of(*f.sibling);
    const jadefx::Color before = f.scene->themeColor(jadefx::ThemeColor::Background);
    f.scene->setUserAgentStylesheet(jadefx::Theme::DARK);
    f.frame();
    Expect(Restyled(*f.sibling, sibling), "a scene's user-agent stylesheet restyles every node");
    Expect(f.scene->themeColor(jadefx::ThemeColor::Background).r != before.r, "the dark theme applies");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "a user-agent change matches a full pass");
}

void TestThemeSwitchRestylesEverything() {
    const std::string previous = jadefx::Theme::getUserAgentStylesheet();
    Fixture f = MakeFixture();
    f.frame();
    const Seen sibling = Of(*f.sibling);
    jadefx::Theme::setUserAgentStylesheet(jadefx::Theme::DARK);
    f.frame();
    Expect(Restyled(*f.sibling, sibling), "switching the application theme restyles every node");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "a theme switch matches a full pass");
    jadefx::Theme::setUserAgentStylesheet(previous);
}
```

Add all six test functions to `RunIncrementalTests()`, after the Task 8 and Task 9 tests.

- [ ] **Step 2: Build and run to verify they fail**

Run the JadeFX build and test commands.
Expected: FAIL lines such as `adding a class restyles the node` and `hovering an item restyles the label inside it`, because nothing marks the nodes yet. The `leaves the other panel alone` lines pass.

- [ ] **Step 3: Move the header setters into `Node.cpp` and mark**

In `Node.hpp`, replace the inline bodies with declarations: `void setAlignment(Pos pos);`, `void setPadding(const Insets& insets);`, `void setBorder(const Insets& insets);`, `void setOpacity(float opacity);`, `void setPressed(bool pressed);`, `void setSelected(bool selected);`, `void setElementId(std::string id);`, `void setSpacingValue(double spacing);` (protected), and `void setDefaultCursor(Cursor cursor);` (protected). Keep their comments.

In `Node.cpp`'s anonymous namespace, add:

```cpp
bool SameSize(const SizeSpec& a, const SizeSpec& b) {
    return a.kind == b.kind && a.pixels == b.pixels && a.percent == b.percent && a.em == b.em;
}
```

Then:

```cpp
void Node::setAlignment(Pos pos) {
    if (alignment_ == pos) {
        return;
    }
    alignment_ = pos;
    markStyleDirty();
}

void Node::setPadding(const Insets& insets) {
    if (SameInsets(padding_, insets)) {
        return;
    }
    padding_ = insets;
    markStyleDirty();
}

void Node::setBorder(const Insets& insets) {
    if (SameInsets(border_, insets)) {
        return;
    }
    border_ = insets;
    markStyleDirty();
}

void Node::setOpacity(float opacity) {
    if (opacity_ == opacity) {
        return;
    }
    opacity_ = opacity;
    markStyleDirty();
}

void Node::setPressed(bool pressed) {
    if (pressed_ == pressed) {
        return;
    }
    pressed_ = pressed;
    markStyleDirty(StyleDirt::Subtree);
}

void Node::setSelected(bool selected) {
    if (selected_ == selected) {
        return;
    }
    selected_ = selected;
    markStyleDirty(StyleDirt::Subtree);
}

void Node::setElementId(std::string id) {
    if (id_ == id) {
        return;
    }
    id_ = std::move(id);
    markStyleDirty(StyleDirt::Subtree);
}

void Node::setSpacingValue(double spacing) {
    const float value = static_cast<float>(spacing);
    if (spacing_ == value) {
        return;
    }
    spacing_ = value;
    markStyleDirty();
}

void Node::setDefaultCursor(Cursor cursor) {
    if (defaultCursor_ == cursor) {
        return;
    }
    defaultCursor_ = cursor;
    markStyleDirty();
}
```

Add a compare-and-mark to each existing out-of-line setter:
- `setCursor`: return early if `cursorExplicit_ && cursor_ == cursor`, otherwise set both and call `markStyleDirty();`.
- `setPseudoState`: call `markStyleDirty(StyleDirt::Subtree);` only on the two branches that actually insert or erase.
- `setDisable`: after `disable_ = value;`, add `markStyleDirty(StyleDirt::Subtree);`. A descendant's `:disabled` and its cursor follow an ancestor's flag.
- `setBackground`: return early if `backgroundExplicit_ && SameColor(background_, color)`, then call `markStyleDirty();`.
- `setStyle`: return early if `styleText_ == css`, then call `markStyleDirty();`.
- `setStylesheet(std::string css)`: always call `markStyleDirty(StyleDirt::Subtree);`. A sheet covers its subtree.
- `setPrefWidth`, `setPrefHeight`, `setPrefWidthRatio`, `setPrefHeightRatio`, `setMinSize`, `setMaxSize`: compute the new `SizeSpec` or specs, return early if every one is `SameSize` as the old, otherwise assign and call `markStyleDirty();`.
- `setFontInternal`: return early if `fontExplicit_ == explicitSize && font_.family() == font.family() && font_.size() == font.size()`, otherwise assign and call `markStyleDirty();`.
- `setTextFillInternal`: return early if `fillExplicit_ == explicitColor && SameColor(textFill_, color)`, then call `markStyleDirty();`.
- `setSubpixelRenderingInternal`: return early if `subpixelExplicit_ && subpixel_ == enabled`, then call `markStyleDirty();`.

In the `Node::Node()` constructor, add:

```cpp
    // A class can appear in any compound of a descendant selector.
    classList_.addListener([this](const ObservableList<std::string>::Change&) { markStyleDirty(StyleDirt::Subtree); });
```

- [ ] **Step 4: Mark hover and pressed changes**

In `syncHover`'s last loop, add `node->markStyleDirty(StyleDirt::Subtree);` just before `node->handleHoverChanged();`. That line runs only for nodes whose hover state changed. Replace the body of `setPressedChain`:

```cpp
void Node::setPressedChain(Node* hit) {
    std::vector<Node*> chain;
    for (Node* node = hit; node != nullptr; node = node->parent_) {
        chain.push_back(node);
    }
    std::vector<Node*> stack{this};
    while (!stack.empty()) {
        Node* node = stack.back();
        stack.pop_back();
        node->setPressed(std::find(chain.begin(), chain.end(), node) != chain.end());
        node->visitChildren([&](Node* child) { stack.push_back(child); });
    }
}
```

- [ ] **Step 5: User-agent sheets and theme generation**

In `Theme.hpp`, inside `class Theme`, add:

```cpp
    // Counts setUserAgentStylesheet calls, so a scene can see that the theme changed.
    static std::uint64_t generation();
```

Add `#include <cstdint>`. In `Theme.cpp`, add `std::uint64_t generation = 0;` to `Global`, add `++global.generation;` in `setUserAgentStylesheet`, and define `std::uint64_t Theme::generation() { return TheGlobal().generation; }`.

In `Scene.hpp` private, add `std::uint64_t themeGeneration_ = 0;`. In the `Scene` constructor, set `themeGeneration_ = Theme::generation();`. At the end of `Scene::setUserAgentStylesheet`, add `markStyleDirty(StyleDirt::Subtree);`. At the start of `Scene::layout`, after `beginLayoutPass();`, add:

```cpp
    // A scene without its own user-agent sheet follows the application theme.
    if (themeGeneration_ != Theme::generation()) {
        themeGeneration_ = Theme::generation();
        if (userAgentSource_.empty()) {
            markStyleDirty(StyleDirt::Subtree);
        }
    }
```

At the end of `SubScene::setUserAgentStylesheet` in `src/scene/SubScene.cpp`, add `markStyleDirty(StyleDirt::Subtree);`.

- [ ] **Step 6: Build and run the tests**

Run the JadeFX build and test commands.
Expected: `layout tests passed`, or only the known flakes. `TestIdleFrameDoesNothing` still passes, which proves the setters don't mark when nothing changes.

- [ ] **Step 7: Commit**

```bash
cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && git add include/jadefx/scene/Node.hpp src/scene/Node.cpp include/jadefx/style/Theme.hpp src/style/Theme.cpp include/jadefx/scene/Scene.hpp src/scene/Scene.cpp src/scene/SubScene.cpp tests/incremental_tests.cpp && git commit -m "Mark nodes for restyle when classes, ids, styles, sheets, pseudo-states, code-set values, or the theme change

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 11: Focus and `:focus-within` as a flag

**Files:**
- Modify: `include/jadefx/scene/Node.hpp` (`isFocusWithin`, `setFocusedFlag`, `addFocusWithin`, `focusWithinCount_`)
- Modify: `src/scene/Node.cpp` (`isFocusWithin`, `clearFocus`, `markFocused`, `setParent`)
- Modify: `src/scene/Scene.cpp:515` (`moveFocus`)
- Modify: `src/scene/IncrementalCheck.hpp/.cpp` (snapshot focus fields)
- Test: `tests/incremental_tests.cpp`

**Interfaces:**
- Produces: `bool Node::isFocusWithin() const` (O(1)); private `void setFocusedFlag(bool focused)`, `void addFocusWithin(int delta)`, `int focusWithinCount_`; and `NodeSnapshot::focusWithin` plus `NodeSnapshot::focusWithinWalk`, which `compare` checks.

- [ ] **Step 1: Write the failing tests**

Add to `tests/incremental_tests.cpp`:

```cpp
void TestFocusWithinFollowsFocus() {
    Fixture f = MakeFixture();
    std::shared_ptr<jadefx::Label> secondLabel;
    auto second = Item("second", secondLabel);
    f.panelA->getChildren().add(second);
    f.frame();
    const Seen panelA = Of(*f.panelA);
    const Seen panelB = Of(*f.panelB);
    f.scene->requestFocus(f.target.get());
    f.frame();
    Expect(Restyled(*f.panelA, panelA), "focusing inside a panel restyles the panel");
    Expect(Is(f.panelA->computedStyle().background.color, 0, 255, 255), "the panel matches :focus-within");
    Expect(Is(f.target->computedStyle().background.color, 255, 0, 255), "the focused item matches :focus");
    Expect(!Restyled(*f.panelB, panelB), "focus leaves the other panel alone");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "focus matches a full pass");

    const Seen panelAgain = Of(*f.panelA);
    const Seen secondSeen = Of(*second);
    f.scene->requestFocus(second.get());
    f.frame();
    Expect(Restyled(*second, secondSeen), "the newly focused item restyles");
    Expect(!Restyled(*f.panelA, panelAgain), "focus moving within a panel leaves the panel's :focus-within as it was");

    f.panelA->getChildren().removeIf([&](const std::shared_ptr<jadefx::Node>& n) { return n == second; });
    f.frame();
    Expect(!f.panelA->isFocusWithin(), "removing the focused node clears :focus-within above it");
    Expect(!Is(f.panelA->computedStyle().background.color, 0, 255, 255), "and the panel's :focus-within style goes");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "removing the focused node matches a full pass");
}

void TestFocusWithinWithoutWindowFocus() {
    Fixture f = MakeFixture();
    f.scene->requestFocus(f.target.get());
    f.frame();
    f.scene->noteWindowFocus(false);
    f.frame();
    Expect(!f.panelA->isFocusWithin(), "a window without the system focus has nothing focused within");
    f.scene->noteWindowFocus(true);
    f.frame();
    Expect(f.panelA->isFocusWithin(), "the focus returns with the window");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "window focus matches a full pass");
}
```

Add both to `RunIncrementalTests()`.

- [ ] **Step 2: Build and run to verify they fail**

Run the JadeFX build and test commands.
Expected: `FAIL focusing inside a panel restyles the panel`, because focus marks nothing yet.

- [ ] **Step 3: Keep a count per node**

In `Node.hpp`, change `bool isFocusWithin();` to:

```cpp
    // True when this node or a descendant is focused. Matches the :focus-within pseudo.
    bool isFocusWithin() const { return focusWithinCount_ > 0; }
```

Private:

```cpp
    // Sets focused_ and keeps every ancestor's focus-within count and style in step.
    void setFocusedFlag(bool focused);
    // Adds delta to this node's and each ancestor's count of focused nodes inside it.
    void addFocusWithin(int delta);
    int focusWithinCount_ = 0;
```

In `Node.cpp`, delete the old `Node::isFocusWithin` walk and add:

```cpp
void Node::addFocusWithin(int delta) {
    for (Node* node = this; node != nullptr; node = node->parent_) {
        const bool was = node->focusWithinCount_ > 0;
        node->focusWithinCount_ += delta;
        if (was != (node->focusWithinCount_ > 0)) {
            node->markStyleDirty(StyleDirt::Subtree);
        }
    }
}

void Node::setFocusedFlag(bool focused) {
    if (focused_ == focused) {
        return;
    }
    focused_ = focused;
    markStyleDirty(StyleDirt::Subtree);
    addFocusWithin(focused ? 1 : -1);
}
```

In `clearFocus`, replace `node->focused_ = false;` with `node->setFocusedFlag(false);`. In `markFocused`, replace `hit->focused_ = true;` with `hit->setFocusedFlag(true);`. In `Scene::moveFocus`, replace `previous->focused_ = false;` with `previous->setFocusedFlag(false);`.

In `Node::setParent`, around the `parent_ = parent;` assignment:

```cpp
    // A subtree holding the focus takes its count from the old ancestors to the new.
    const int focusCount = focusWithinCount_;
    if (focusCount > 0 && previousParent != parent && previousParent != nullptr) {
        previousParent->addFocusWithin(-focusCount);
    }
    parent_ = parent;
    if (focusCount > 0 && previousParent != parent && parent_ != nullptr) {
        parent_->addFocusWithin(focusCount);
    }
```

`previousParent` is the variable Task 8 Step 7 declared at the top of `setParent`.

- [ ] **Step 4: Check the flag in verification**

In `IncrementalCheck.hpp`, add to `NodeSnapshot`:

```cpp
    // The flag :focus-within reads, and the same answer found by walking the subtree.
    bool focusWithin = false;
    bool focusWithinWalk = false;
```

In `IncrementalCheck`, add `static bool walkFocusWithin(Node& node);` to the private section. In the `.cpp`:

```cpp
bool IncrementalCheck::walkFocusWithin(Node& node) {
    if (node.focused_) {
        return true;
    }
    bool found = false;
    node.visitChildren([&](Node* child) { found = found || walkFocusWithin(*child); });
    return found;
}
```

In `collect`, set `shot.focusWithin = node.isFocusWithin();` and `shot.focusWithinWalk = walkFocusWithin(node);`. In `compare`, after the style check, add:

```cpp
        if (a.focusWithin != a.focusWithinWalk) {
            return a.path + ": focus-within flag";
        }
```

- [ ] **Step 5: Build and run the tests**

Run the JadeFX build and test commands.
Expected: `layout tests passed`, or only the known flakes.

- [ ] **Step 6: Commit**

```bash
cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && git add include/jadefx/scene/Node.hpp src/scene/Node.cpp src/scene/Scene.cpp src/scene/IncrementalCheck.hpp src/scene/IncrementalCheck.cpp tests/incremental_tests.cpp && git commit -m "Keep :focus-within as a per-node count and restyle only nodes whose focus state changed

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 12: Layout invalidation sources

**Files:**
- Modify: `src/scene/Node.cpp` (`resolveStyle`: layout-affecting comparison; `setVisible`)
- Modify: `include/jadefx/scene/Node.hpp` (`setVisible` declaration)
- Test: `tests/incremental_tests.cpp`

**Interfaces:**
- Consumes: `markLayoutDirty`, `markSubtreeLayoutDirty` (Task 8), and `SameSize`, `SameInsets` (Node.cpp).
- Produces: a restyle marks layout only for layout-affecting changes, and an alignment change marks the whole subtree. `Node::setVisible(bool)` moves out of line and marks layout.

- [ ] **Step 1: Write the failing tests**

Add to `tests/incremental_tests.cpp`:

```cpp
void TestPaintOnlyChangeKeepsLayout() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen target = Of(*f.target);
    const Seen root = Of(*f.root);
    f.target->getClassList().add("on");
    f.frame();
    Expect(Restyled(*f.target, target), "a color class restyles");
    Expect(!LaidOut(*f.target, target) && !LaidOut(*f.root, root), "a color change lays nothing out");
}

void TestPaddingChangeLaysOutUpward() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen target = Of(*f.target);
    const Seen panelA = Of(*f.panelA);
    const Seen root = Of(*f.root);
    const Seen panelB = Of(*f.panelB);
    f.target->getClassList().add("wide");
    f.frame();
    Expect(LaidOut(*f.target, target) && LaidOut(*f.panelA, panelA) && LaidOut(*f.root, root),
           "a padding change lays out the node and its ancestors");
    Expect(!LaidOut(*f.panelB, panelB), "a padding change leaves the other panel's layout alone");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "a padding change matches a full pass");
}

void TestResizeLaysOut() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen root = Of(*f.root);
    f.scene->layout(400, 200, f.time + 0.1);
    Expect(LaidOut(*f.root, root), "resizing the scene lays out the root");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "a resize matches a full pass");
}

void TestAlignmentReachesDescendants() {
    Fixture f = MakeFixture();
    f.frame();
    f.root->setAlignment(jadefx::Pos::BottomRight);
    f.frame();
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(),
           "an ancestor's alignment change places descendants that inherit it");
}

void TestVisibilityLaysOut() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen panelA = Of(*f.panelA);
    f.target->setVisible(false);
    f.frame();
    Expect(LaidOut(*f.panelA, panelA), "hiding a node lays out its parent");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "hiding matches a full pass");
}

void TestPopupFollowsItsContent() {
    Fixture f = MakeFixture();
    auto popup = jadefx::make<jadefx::StackPane>();
    popup->getClassList().add("item");
    auto text = jadefx::make<jadefx::Label>("popup");
    popup->getChildren().add(text);
    f.scene->showPopup(popup, 10, 10, -1, -1);
    f.frame();
    const double before = popup->getWidth();
    text->setText("a popup with much longer text in it");
    f.frame();
    Expect(popup->getWidth() > before + 20, "a measured popup grows with its content");
    f.scene->movePopup(popup.get(), 30, 40, -1, -1);
    f.frame();
    Expect(popup->getX() == 30 && popup->getY() == 40, "a moved popup is placed where it was moved");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "popups match a full pass");
}
```

Add all six to `RunIncrementalTests()`. Task 14 makes `TestPopupFollowsItsContent` fully pass by marking `Labeled::setText`. Until then its first expectation fails. Note that in this task's Step 2 output.

- [ ] **Step 2: Build and run to verify they fail**

Run the JadeFX build and test commands.
Expected: `FAIL a color change lays nothing out`, because Task 8 marks layout on every restyle. Also `FAIL hiding a node lays out its parent` and `FAIL a measured popup grows with its content`.

- [ ] **Step 3: Compare layout-affecting fields**

In `Node.cpp`'s anonymous namespace:

```cpp
// Fields that change a node's preferred size or how it places its children.
// Colors, shadows, radii, opacity, and the cursor only change how it paints.
bool LayoutAffectingChange(const ComputedStyle& a, const ComputedStyle& b) {
    return !SameInsets(a.padding, b.padding) || !SameInsets(a.border, b.border) || a.fontSize != b.fontSize ||
           a.fontFamily != b.fontFamily || a.subpixel != b.subpixel || !SameSize(a.width, b.width) ||
           !SameSize(a.height, b.height) || !SameSize(a.minWidth, b.minWidth) || !SameSize(a.minHeight, b.minHeight) ||
           !SameSize(a.maxWidth, b.maxWidth) || !SameSize(a.maxHeight, b.maxHeight) || a.spacing != b.spacing ||
           a.rowGap != b.rowGap || a.columnGap != b.columnGap || a.alignment != b.alignment ||
           a.alignmentFromCss != b.alignmentFromCss || a.orientationFromCss != b.orientationFromCss ||
           a.orientation != b.orientation || a.indeterminateBarLengthSet != b.indeterminateBarLengthSet ||
           !SameSize(a.indeterminateBarLength, b.indeterminateBarLength) ||
           a.indeterminateBarEscapeSet != b.indeterminateBarEscapeSet ||
           a.indeterminateBarEscape != b.indeterminateBarEscape ||
           a.indeterminateBarFlipSet != b.indeterminateBarFlipSet || a.indeterminateBarFlip != b.indeterminateBarFlip ||
           a.indeterminateBarAnimationTimeSet != b.indeterminateBarAnimationTimeSet ||
           a.indeterminateBarAnimationTime != b.indeterminateBarAnimationTime;
}
```

In `resolveStyle`, replace the Task 8 block, from `const bool inheritChanged` through the closing brace of `if (incrementalActive())`, with:

```cpp
    const bool inheritChanged = !SameInheritable(computed_, style);
    const bool layoutChanged = LayoutAffectingChange(computed_, style);
    // usingAlignment reads ancestors' alignment, so descendants may move as well.
    const bool alignmentChanged =
        computed_.alignment != style.alignment || computed_.alignmentFromCss != style.alignmentFromCss;
    computed_ = style;
    if (incrementalActive()) {
        ++restyleCount_;
        if (alignmentChanged) {
            markSubtreeLayoutDirty();
        } else if (layoutChanged) {
            markLayoutDirty(LayoutDirt::Size);
        }
    }
```

This also removes the `measure_.clear();` that Task 7 put after `computed_ = style;`. `markLayoutDirty(Size)` now clears the cache whenever a layout-affecting field changes, and a full pass starts measures fresh by epoch.

- [ ] **Step 4: `setVisible` marks layout**

In `Node.hpp`, replace the inline `setVisible` with `void setVisible(bool visible);`. In `Node.cpp`:

```cpp
void Node::setVisible(bool visible) {
    if (visible_ == visible) {
        return;
    }
    visible_ = visible;
    // Containers may skip hidden children when they measure and place them.
    markLayoutDirty();
}
```

- [ ] **Step 5: Build and run the tests**

Run the JadeFX build and test commands.
Expected: every new test passes except `a measured popup grows with its content`, which waits for Task 14's `Labeled::setText` mark. Everything else prints no new FAIL lines.

- [ ] **Step 6: Commit**

```bash
cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && git add include/jadefx/scene/Node.hpp src/scene/Node.cpp tests/incremental_tests.cpp && git commit -m "Lay out again only for layout-affecting style changes, visibility changes, and alignment changes below

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 13: Transitions keep restyling until they finish

**Files:**
- Modify: `include/jadefx/scene/Node.hpp` (`end` field on `ColorAnim`, `InsetAnim`, `ShadowAnim`; private `transitionRunning`)
- Modify: `src/scene/Node.cpp` (`animateColor`, `animateInsets`, shadow block in `resolveStyle`)
- Modify: `include/jadefx/scene/Scene.hpp`, `src/scene/Scene.cpp` (`animating_`, `noteAnimating`, `layout`, `forgetNode`)
- Test: `tests/incremental_tests.cpp`

**Interfaces:**
- Produces: `bool Node::transitionRunning(double time) const` (private), `void Scene::noteAnimating(Node* node)` (private; Node is a friend), and `std::vector<Node*> Scene::animating_`.

- [ ] **Step 1: Write the failing test**

Add to `tests/incremental_tests.cpp`:

```cpp
void TestOnlyTheAnimatingNodeRestyles() {
    Fixture f = MakeFixture();
    f.target->getClassList().add("fade");
    f.frame();
    const Seen sibling = Of(*f.sibling);
    const Seen panelA = Of(*f.panelA);
    f.target->getClassList().add("on");
    f.frame();
    std::uint32_t restyles = f.target->debugRestyleCount();
    bool everyFrame = true;
    for (int i = 0; i < 10; ++i) {
        f.frame();
        everyFrame = everyFrame && f.target->debugRestyleCount() == restyles + 1;
        restyles = f.target->debugRestyleCount();
    }
    Expect(everyFrame, "a node in a transition restyles every frame");
    const float mid = f.target->computedStyle().background.color.r;
    Expect(mid > 0.05f && mid < 0.95f, "the background is part way through its transition");
    Expect(!Restyled(*f.sibling, sibling) && !Restyled(*f.panelA, panelA), "only the animating node restyles");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "a transition matches a full pass");
    for (int i = 0; i < 40; ++i) {
        f.frame();
    }
    Expect(Is(f.target->computedStyle().background.color, 255, 255, 255), "the transition ends on its target");
    const Seen settled = Of(*f.target);
    f.frame();
    Expect(!Restyled(*f.target, settled), "a finished transition stops restyling");
}
```

Add it to `RunIncrementalTests()`.

- [ ] **Step 2: Build and run to verify it fails**

Run the JadeFX build and test commands.
Expected: `FAIL a node in a transition restyles every frame`.

- [ ] **Step 3: Record when each animation ends**

In `Node.hpp`, add `double end = 0;` after `double duration = 0;` in `ColorAnim`, `InsetAnim`, and `ShadowAnim`. In the private section, add:

```cpp
    // True while any of this node's transitions has not reached its target at time.
    bool transitionRunning(double time) const;
```

In `animateColor` and `animateInsets`, in the reset branch (`!anim.ready || (duration <= 0 && delay <= 0)`), add `anim.end = 0;`. In the restart branch (target changed), add `anim.end = time + delay + duration;`. In `resolveStyle`'s shadow block, add `shadowAnim_.end = 0;` in the reset branch and `shadowAnim_.end = timeSeconds + shadowTiming.delay + shadowTiming.duration;` where `changed` restarts it.

```cpp
bool Node::transitionRunning(double time) const {
    if (backgroundAnim_.end > time || colorAnim_.end > time || borderColorAnim_.end > time ||
        borderAnim_.end > time || shadowAnim_.end > time) {
        return true;
    }
    for (const ColorAnim& stop : stopAnim_) {
        if (stop.end > time) {
            return true;
        }
    }
    return false;
}
```

At the end of `resolveStyle`, before `styleDidApply();`, add:

```cpp
    if (incrementalActive() && scene_ != nullptr && transitionRunning(timeSeconds)) {
        scene_->noteAnimating(this);
    }
```

- [ ] **Step 4: The scene's animating set**

In `Scene.hpp` private:

```cpp
    // Nodes with a transition still running. Each is restyled again next frame.
    std::vector<Node*> animating_;
    void noteAnimating(Node* node);
```

In `Scene.cpp`:

```cpp
void Scene::noteAnimating(Node* node) {
    if (std::find(animating_.begin(), animating_.end(), node) == animating_.end()) {
        animating_.push_back(node);
    }
}
```

In `Scene::layout`, after the theme-generation check:

```cpp
    // A node still in a transition registers again when it restyles.
    if (!animating_.empty()) {
        std::vector<Node*> nodes;
        nodes.swap(animating_);
        for (Node* node : nodes) {
            node->markStyleDirty();
        }
    }
```

In `Scene::forgetNode`, add `animating_.erase(std::remove(animating_.begin(), animating_.end(), node), animating_.end());`. In `~Scene`, add `animating_.clear();` next to `popups_.clear();`.

- [ ] **Step 5: Build and run the tests**

Run the JadeFX build and test commands.
Expected: no new FAIL lines, apart from the known `a measured popup grows with its content`, which waits for Task 14.

- [ ] **Step 6: Commit**

```bash
cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && git add include/jadefx/scene/Node.hpp src/scene/Node.cpp include/jadefx/scene/Scene.hpp src/scene/Scene.cpp tests/incremental_tests.cpp && git commit -m "Restyle a node every frame while one of its transitions runs, and only that node

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 14: Control audit, time-driven layouts, and incremental passes on by default

**Files:**
- Modify: `src/scene/controls/*.cpp`, `include/jadefx/scene/controls/*.hpp`, `src/scene/image/*.cpp`, `include/jadefx/scene/image/*.hpp`, `src/scene/layout/*.cpp`. Named edits are below, followed by the checklist.
- Modify: `include/jadefx/scene/Scene.hpp` (default `incremental_ = true`)
- Test: `tests/incremental_tests.cpp`

**Interfaces:**
- Consumes: `markStyleDirty`, `markLayoutDirty`, `StyleDirt`, `LayoutDirt` (Task 8).
- Produces: every JadeFX control marks itself when its state changes. `Scene::incrementalUpdates()` defaults to `true`.

- [ ] **Step 1: Write the failing tests**

Add to `tests/incremental_tests.cpp`:

```cpp
void TestLabelTextLaysOut() {
    Fixture f = MakeFixture();
    f.frame();
    const Seen target = Of(*f.target);
    f.targetLabel->setText("a much longer label than before");
    f.frame();
    Expect(LaidOut(*f.target, target), "new label text lays out the label's parent");
    Expect(jadefx::IncrementalCheck::verify(*f.scene).empty(), "label text matches a full pass");
}

void TestIndeterminateBarKeepsMoving() {
    auto root = jadefx::make<jadefx::VBox>();
    auto bar = jadefx::make<jadefx::ProgressBar>(-1.0);
    bar->setPrefSize(200, 12);
    root->getChildren().add(bar);
    auto scene = jadefx::make<jadefx::Scene>(root, 300, 100);
    scene->setIncrementalUpdates(true);
    scene->layout(300, 100, 0);
    const std::uint32_t before = bar->debugLayoutCount();
    for (int i = 1; i <= 5; ++i) {
        scene->layout(300, 100, i / 60.0);
    }
    Expect(bar->debugLayoutCount() >= before + 5, "an indeterminate bar lays out every frame with nothing else changing");
    Expect(jadefx::IncrementalCheck::verify(*scene).empty(), "an indeterminate bar matches a full pass");
}

void TestListViewFollowsItemChanges() {
    auto list = jadefx::make<jadefx::ListView<std::string>>();
    list->getItems().add("one");
    list->getItems().add("two");
    list->setPrefSize(200, 120);
    auto scene = jadefx::make<jadefx::Scene>(list, 200, 120);
    scene->setIncrementalUpdates(true);
    scene->layout(200, 120, 0);
    list->getItems().set(0, "a much longer first item");
    list->getItems().add("three");
    scene->layout(200, 120, 0.1);
    Expect(jadefx::IncrementalCheck::verify(*scene).empty(), "a list view follows its items");
    list->getSelectionModel().select(1);
    scene->layout(200, 120, 0.2);
    Expect(jadefx::IncrementalCheck::verify(*scene).empty(), "a list view follows its selection");
}
```

Add all three to `RunIncrementalTests()`. If `ItemSelectionModel<T>` names its single-select method differently, use the method `grep -n "void select" include/jadefx/scene/controls/SelectionModel.hpp` shows.

- [ ] **Step 2: Build and run to verify they fail**

Run the JadeFX build and test commands.
Expected: `FAIL new label text lays out the label's parent`, `FAIL an indeterminate bar lays out every frame...`, and `FAIL a measured popup grows with its content` (from Task 12).

- [ ] **Step 3: Named edits**

Each one follows the rule: compare, assign, mark.

`src/scene/controls/Labeled.cpp`:

```cpp
void Labeled::setText(std::string text) {
    if (text == text_) {
        return;
    }
    text_ = std::move(text);
    markLayoutDirty();
}
```

In `Labeled.hpp`, move the inline `setTextScaled`, `setContentDisplay`, and `setGraphicTextGap` into `Labeled.cpp` in the same shape, each ending with `markLayoutDirty();`. In `Labeled::setGraphic`, add `markLayoutDirty();` after the graphic is replaced. The new child's `setParent` already marks it.

`src/scene/controls/IndexedCell.cpp`:

```cpp
void IndexedCell::updateIndex(int index) {
    if (index_ == index) {
        return;
    }
    index_ = index;
    // :nth-child reads the item's place, and a descendant selector may use it.
    markStyleDirty(StyleDirt::Subtree);
    markLayoutDirty();
}
```

`src/scene/controls/ProgressBar.cpp`:
- In `setProgress`, return early when the value is unchanged, then call `markLayoutDirty(LayoutDirt::Arrange);`. The bar length changes but the control's preferred size does not.
- In `setIndeterminateBarLength`, `setIndeterminateBarEscape`, `setIndeterminateBarFlip`, and `setIndeterminateBarAnimationTime`, add `markLayoutDirty(LayoutDirt::Arrange);` after the assignment.
- At the end of `ProgressBar::layoutChildren`, add:

```cpp
    // The indeterminate bar moves with the clock, so it is placed again next frame.
    if (isIndeterminate() && getScene() != nullptr) {
        markLayoutDirty(LayoutDirt::Arrange);
    }
```

`src/scene/controls/Spinner.cpp`, at the end of `Spinner::layoutChildren` after `advanceSpin();`:

```cpp
    // A held arrow repeats on the clock.
    if (spinning_) {
        markLayoutDirty(LayoutDirt::Arrange);
    }
```

`src/scene/controls/StyledTextArea.cpp`, right after the `advanceSmoothScroll();` call at line ~2698:

```cpp
    if (smoothScrolling_) {
        markLayoutDirty(LayoutDirt::Arrange);
    }
```

`src/scene/controls/TreeView.cpp`, at the end of `TreeView::layoutChildren`. In the row loop that compares `now` against an appear time with `kAppearSeconds`, set `appearing = true` whenever a row's appear progress is below 1. Then:

```cpp
    // Smooth scrolling and rows fading in move with the clock.
    if (impl_->smoothing || appearing) {
        markLayoutDirty(LayoutDirt::Arrange);
    }
```

Declare `bool appearing = false;` before that loop.

Scroll offsets: in `ScrollPane`, `ScrollBar`, `Slider`, `SplitPane` (divider positions), `TabPane` (selected tab), and `VirtualFlow` (scroll position), every setter that changes a value read by `layoutChildren` ends with `markLayoutDirty(LayoutDirt::Arrange);`, and with `markLayoutDirty();` if `preferredContent*` reads it.

Models: everywhere a control holds or observes a model, the existing listener or callback also calls `markLayoutDirty();`. The models are an `ObservableList` member (`ComboBox::items_`, `Menu::items_`, `MenuBar::menus_`, `ColorChooser::presets_`/`recent_`, `Alert` button types), the list a `ListView`/`TableView` shows through `items_`, the `TreeItem` structure listener (`setStructureListener`), and `SelectionModel` changes. Selection changes also call `markStyleDirty(StyleDirt::Subtree);`, because cells match `:selected`. Where a model has no listener yet, add one in the control's constructor:

```cpp
    items_.addListener([this](const ObservableList<std::string>::Change&) { markLayoutDirty(); });
```

When `setItems` swaps a list, register the listener on the new list and remove it from the old one with `removeListener(id)`. Then mark.

- [ ] **Step 4: Run the checklist over every remaining setter**

```bash
cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && grep -n "^void [A-Za-z]*::set[A-Z]\|^void [A-Za-z]*::update[A-Z]\|^void [A-Za-z]*::refresh" src/scene/controls/*.cpp src/scene/layout/*.cpp src/scene/image/*.cpp src/scene/text/*.cpp src/scene/SubScene.cpp && grep -n "void set[A-Z][A-Za-z]*(.*) {" include/jadefx/scene/controls/*.hpp include/jadefx/scene/layout/*.hpp include/jadefx/scene/image/*.hpp
```

For each result, decide which case applies:
- **It changes a member that `layoutChildren`, `preferredContentWidth`, or `preferredContentHeight` reads:** move it out of the header if it is inline. Return early when the value is unchanged, assign, then call `markLayoutDirty();`. Use `markLayoutDirty(LayoutDirt::Arrange)` only when you can show the preferred size does not read it.
- **It changes something selector matching reads (classes, id, pseudo-state, `getNthChildIndex`):** call `markStyleDirty(StyleDirt::Subtree);`.
- **It changes something only `styleDidApply` reads:** call `markStyleDirty();`.
- **It changes only what `render`/`renderContent` reads (colors drawn directly, carets, washes):** leave it as is.

An `ImageView` whose image changes size, including when a load finishes later, calls `markLayoutDirty();` where the new size is stored.

- [ ] **Step 5: Turn incremental passes on by default**

In `Scene.hpp`, change `bool incremental_ = false;` to `bool incremental_ = true;`, and update `setIncrementalUpdates`'s comment to say that on is the default.

- [ ] **Step 6: Build and run the full JadeFX suite**

Run the JadeFX build and test commands.
Expected: `layout tests passed`, or only the six known tree-view flakes. Every existing test now runs incrementally. A new failure means a setter the test uses is not marked. Find the setter the failing test calls, apply the Step 4 rule, and rerun.

- [ ] **Step 7: Run the JadeFX suite under verification in Debug**

```bash
MSYS_NO_PATHCONV=1 "C:/Program Files/CMake/bin/cmake.exe" --build C:/Users/Andrew/Documents/GitHub/JadeFX_CPP/build --config Debug --target jadefx-tests --parallel && cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && JADEFX_VERIFY_INCREMENTAL=1 ./build/Debug/jadefx-tests.exe
```

Expected: `layout tests passed`, with no `JADEFX_VERIFY_INCREMENTAL:` abort. An abort prints `<path>: <field>`. Find the control at the end of that path, find the state change in its setters that the field depends on, and add the missing mark by the Step 4 rule.

- [ ] **Step 8: Re-run the benchmark**

Run Task 3 Step 3. The steady frame should now be far below the Phase A number, because it is an idle frame. Note it for Task 16.

- [ ] **Step 9: Commit**

```bash
cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && git add -A src include tests && git commit -m "Mark controls dirty when their state changes, keep clock-driven layouts moving, and make incremental passes the default

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 15: AnarchyEngine node subclasses mark their changes

**Files:**
- Modify (as the audit finds): `C:/Users/Andrew/Documents/GitHub/AnarchyEngine-CPP/src/ide/{AssetPicker.hpp,CompletionPopup.cpp,FindBar.hpp,IdeAssets.cpp,IdeAssets.hpp,IdeConflicts.cpp,IdeConsole.hpp,IdeCssEditor.hpp,IdeDock.hpp,IdeExplorer.hpp,IdeLayoutInternal.hpp,IdePrefabEditor.hpp,IdeProblems.hpp,IdeScriptEditor.hpp,IdeSearch.hpp,InsertPopup.cpp,PreferencesPanel.hpp,PropertiesPanel.cpp,TerminalView.hpp}` and `src/runner/{GameView.hpp,GuiLayer.hpp}`, plus their `.cpp` partners
- Test: the existing engine suites (studio-tests, explorer-tests, properties-tests, console-tests, terminal-tests, assets-tests, prefab-editor-tests, shell-tests)

**Interfaces:**
- Consumes: `Node::markLayoutDirty`, `Node::markStyleDirty` (JadeFX Tasks 8 and 14).
- Produces: no new API. Engine nodes mark themselves the way JadeFX controls do.

- [ ] **Step 1: List the overrides**

```bash
cd C:/Users/Andrew/Documents/GitHub/AnarchyEngine-CPP && grep -rn "layoutChildren() override\|preferredContentWidth(.*override\|preferredContentHeight(.*override\|styleDidApply() override" src
```

Expected: about 21 files, the ones listed above.

- [ ] **Step 2: Run the engine suites to find the gaps**

Run the engine build and engine test commands.
Expected: some failures are possible. Each one shows a state change in an engine node that JadeFX does not know about.

- [ ] **Step 3: Apply the marking rule to each override**

For each class from Step 1, read its `layoutChildren`, `preferredContentWidth`/`preferredContentHeight`, and `styleDidApply`, and list the members they read. For every method that writes one of those members (setters, event handlers, model callbacks, appends, rebuilds), add a compare-and-mark at the end. For example, in `TerminalView`, after output is appended:

```cpp
    markLayoutDirty();
```

For a member only `styleDidApply` reads, call `markStyleDirty();`. Anything that changes with the clock inside `layoutChildren`, such as a caret blink, a smooth scroll, or an animated panel, ends its `layoutChildren` with:

```cpp
    if (animating_) {
        markLayoutDirty(jadefx::Node::LayoutDirt::Arrange);
    }
```

Replace `animating_` with the class's own flag for that animation. If the engine sets a JadeFX node's protected state that the public setters don't cover, use the public setter instead.

- [ ] **Step 4: Run the engine suites**

Run the engine build and engine test commands.
Expected: all suites pass, apart from the known `six frames recorded` flake.

- [ ] **Step 5: Commit**

```bash
cd C:/Users/Andrew/Documents/GitHub/AnarchyEngine-CPP && git add -A src && git commit -m "Mark Studio's own JadeFX nodes dirty when their content changes, for incremental passes

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 16: Verify both suites incrementally and record the final numbers

**Files:**
- Modify: whatever the verification runs show to be missing a mark (JadeFX controls or engine nodes), using the Task 14 Step 4 rule
- No new files

**Interfaces:**
- Consumes: everything above.
- Produces: final numbers in the commit messages.

- [ ] **Step 1: Run JadeFX under verification (Debug)**

```bash
MSYS_NO_PATHCONV=1 "C:/Program Files/CMake/bin/cmake.exe" --build C:/Users/Andrew/Documents/GitHub/JadeFX_CPP/build --config Debug --target jadefx-tests --parallel && cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && JADEFX_VERIFY_INCREMENTAL=1 ./build/Debug/jadefx-tests.exe
```

Expected: `layout tests passed`, with no `JADEFX_VERIFY_INCREMENTAL:` line.

- [ ] **Step 2: Run the engine suites under verification (Debug)**

```bash
MSYS_NO_PATHCONV=1 "C:/Program Files/CMake/bin/cmake.exe" --build C:/Users/Andrew/Documents/GitHub/AnarchyEngine-CPP/build --config Debug --parallel && cd C:/Users/Andrew/Documents/GitHub/AnarchyEngine-CPP/build && JADEFX_VERIFY_INCREMENTAL=1 "C:/Program Files/CMake/bin/ctest.exe" -C Debug --output-on-failure
```

Expected: all suites pass, apart from the known Debug flakes (sandbox `[decomposition]` Q1–Q6 under load, and DS1/DS3 CloudCover). An abort printing `JADEFX_VERIFY_INCREMENTAL: <path>: <field>` is a missing mark. Fix it with the Task 14 Step 4 rule in the repo that owns the node at the end of the path, then rerun Steps 1 and 2 until both are clean.

- [ ] **Step 3: Run the Release suites once more**

Run the JadeFX build and test commands, then the engine build and test commands.
Expected: only the known flakes.

- [ ] **Step 4: Measure the final numbers**

Run the benchmark three times (Task 3 Step 3) and `bash C:/Users/Andrew/Documents/GitHub/JadeFX_CPP/build/studio_profile.sh`. Take medians. The steady frame is now an idle frame, and the hover frame restyles two buttons and their contents.

- [ ] **Step 5: Commit the engine fixes (if any)**

```bash
cd C:/Users/Andrew/Documents/GitHub/AnarchyEngine-CPP && git add -A src && git commit -m "Mark the remaining Studio nodes that incremental verification found

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

Skip this commit if `git status` shows no engine changes.

- [ ] **Step 6: Commit JadeFX with the before and after numbers**

Use `--allow-empty` if verification needed no JadeFX fixes.

```bash
cd C:/Users/Andrew/Documents/GitHub/JadeFX_CPP && git add -A src include tests && git commit --allow-empty -m "Verify incremental passes against full passes across the JadeFX and AnarchyEngine suites

Both suites pass in Debug with JADEFX_VERIFY_INCREMENTAL=1.

Benchmark (Release, median of three, 100 frames, 2045 nodes):
                baseline   phase A   final
  first frame   <F0>       <F1>      <F2> ms
  steady frame  <S0>       <S1>      <S2> ms (idle once incremental)
  hover frame   <H0>       <H1>      <H2> ms
Studio editor at rest, Styles and layout: <Z0> -> <Z1> -> <Z2> ms

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

## Self-review notes

- **Spec coverage:**
  - Phase A1: Tasks 1–3. A2: Task 4. A3: Task 5. A4: Task 6, see the shadows/font note below. A5: Task 7. A6: Task 7.
  - Phase B flags: Task 8.
  - Style sources: Task 10 covers class, id, inline, sheets, UA, theme, pseudo-states, and inherited values. Child add/remove/reorder is in Task 8 via `setParent`. Focus is Task 11.
  - Layout sources: Task 12 covers layout-affecting restyles, visibility, resize, popups, and alignment. Text, content, image, and child-list changes are in Tasks 8 and 14.
  - Transitions: Task 13. Frame pass skipping: Task 8. Popups using the same flags: Tasks 8 and 12, where `layoutPopup` relies on `applyStyles` and `performLayout` skipping clean nodes.
  - Verification mode: Task 9, run across both suites in Task 16.
  - Tests: the per-source tests sit in their owning tasks. The idle test is in Task 8 and the transition test in Task 13. Idle and hover benchmark numbers are in Task 16.
- **Ambiguities resolved:** see the report that accompanies this plan. They are also restated inline where they affect code: the `LayoutPass` hook instead of new FramePhase values, the scene default switch, Size marks walking to the root, the measure cache used only inside passes, the theme cache keyed by custom-property set, shadows and font family left as they are, and the animating-subtree exemption in verification.

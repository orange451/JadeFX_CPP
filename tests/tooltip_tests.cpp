#include "jadefx/jadefx.hpp"
#include "jadefx/scene/controls/Tooltip.hpp"

#include <cstdio>
#include <string>

namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

void Click(jadefx::Scene& scene, double x, double y) {
    scene.noteButton(0, true, x, y);
    scene.noteButton(0, false, x, y);
}

void TestTooltipChrome() {
    auto plain = jadefx::make<jadefx::Tooltip>();
    auto named = jadefx::make<jadefx::Tooltip>("Hint");
    Expect(plain->getText().empty(), "the default tooltip has no text");
    Expect(named->getText() == "Hint", "the tooltip stores its text");
    Expect(std::string(plain->getElementType()) == "tooltip", "the element type is tooltip");
    Expect(plain->getShowDelay() == 1 && plain->getHideDelay() == 0.2 && plain->getShowDuration() == 5,
           "tooltip delays match a hover popup");
    Expect(plain->getAlignment() == jadefx::Pos::Center, "tooltip text is centered");
    Expect(plain->getStyle() == "border-radius: 4px;", "tooltip corner radius is inline");
    const jadefx::Insets pad = named->getPadding();
    Expect(pad.top == 6 && pad.bottom == 6 && pad.left == 8 && pad.right == 8, "tooltip padding is 6 by 8");
    Expect(jadefx::near(named->getTextFill(), jadefx::Color::white()), "tooltip text is white");
    named->setShowDelay(0.25);
    named->setHideDelay(0);
    named->setShowDuration(0);
    Expect(named->getShowDelay() == 0.25 && named->getHideDelay() == 0 && named->getShowDuration() == 0,
           "delay setters store the new values");
}

void TestTooltipHover() {
    auto tag = jadefx::make<jadefx::Label>("Term");
    tag->setPrefSize(80, 24);
    auto tip = jadefx::make<jadefx::Tooltip>("Definition");
    jadefx::Tooltip::install(tag.get(), tip);
    const jadefx::HoverPopup* popup = tag->getHoverPopup();
    Expect(popup != nullptr && popup->content.get() == tip.get(), "install attaches the tooltip");
    Expect(popup != nullptr && popup->showDelay == 1 && popup->hideDelay == 0.2 && popup->showDuration == 5,
           "install copies the default delays");

    auto scene = jadefx::make<jadefx::Scene>(tag, 200, 120);
    scene->layout(200, 120, 0);
    const double x = tag->getAbsoluteX() + 4;
    const double y = tag->getAbsoluteY() + 4;
    scene->noteMove(x, y);
    scene->layout(200, 120, 0.4);
    Expect(!scene->isPopupShowing(tip.get()), "a tooltip waits out its show delay");
    scene->layout(200, 120, 1.1);
    Expect(scene->isPopupShowing(tip.get()), "a tooltip shows after the delay");
    Expect(jadefx::near(tip->computedStyle().background.color, jadefx::Color::rgb8(60, 64, 67)),
           "tooltip background is the dark chrome color");
    Expect(tip->computedStyle().radius[0].kind == jadefx::SizeKind::Pixels && tip->computedStyle().radius[0].pixels == 4,
           "tooltip corner radius resolves to 4px");
    Expect(jadefx::near(tip->computedStyle().color, jadefx::Color::white()), "shown tooltip text stays white");

    scene->noteMove(180, 100);
    scene->layout(200, 120, 1.15);
    Expect(scene->isPopupShowing(tip.get()), "the tooltip stays during the hide delay");
    scene->layout(200, 120, 1.5);
    Expect(!scene->isPopupShowing(tip.get()), "the tooltip hides after the hide delay");

    scene->noteMove(x, y);
    scene->layout(200, 120, 3);
    Expect(scene->isPopupShowing(tip.get()), "hovering again shows the tooltip");
    Click(*scene, x, y);
    Expect(!scene->isPopupShowing(tip.get()), "a press hides the tooltip");

    jadefx::Tooltip::uninstall(tag.get());
    Expect(tag->getHoverPopup() == nullptr, "uninstall clears the hover popup");
    scene->noteMove(180, 100);
    scene->layout(200, 120, 3.4);
    scene->noteMove(x, y);
    scene->layout(200, 120, 5);
    Expect(!scene->isPopupShowing(tip.get()), "a hover after uninstall stays hidden");
}

void TestZeroShowDelay() {
    auto tag = jadefx::make<jadefx::Label>("Term");
    tag->setPrefSize(80, 24);
    auto tip = jadefx::make<jadefx::Tooltip>("Now");
    tip->setShowDelay(0);
    jadefx::Tooltip::install(tag.get(), tip);
    auto scene = jadefx::make<jadefx::Scene>(tag, 200, 120);
    scene->layout(200, 120, 2);
    const double x = tag->getAbsoluteX() + 4;
    const double y = tag->getAbsoluteY() + 4;
    scene->noteMove(x, y);
    Expect(scene->isPopupShowing(tip.get()), "a zero show delay opens the tooltip on enter");
}

void TestUpdatedDelays() {
    auto tag = jadefx::make<jadefx::Label>("Term");
    tag->setPrefSize(80, 24);
    auto tip = jadefx::make<jadefx::Tooltip>("Wait");
    jadefx::Tooltip::install(tag.get(), tip);
    tip->setShowDelay(2);
    tip->setHideDelay(0.4);
    tip->setShowDuration(8);
    const jadefx::HoverPopup* popup = tag->getHoverPopup();
    Expect(popup != nullptr && popup->showDelay == 2 && popup->hideDelay == 0.4 && popup->showDuration == 8,
           "delay setters refresh the installed hover popup");

    auto scene = jadefx::make<jadefx::Scene>(tag, 200, 120);
    scene->layout(200, 120, 0);
    const double x = tag->getAbsoluteX() + 4;
    const double y = tag->getAbsoluteY() + 4;
    scene->noteMove(x, y);
    scene->layout(200, 120, 1.1);
    Expect(!scene->isPopupShowing(tip.get()), "the scene waits for the updated show delay");
    scene->layout(200, 120, 2.05);
    Expect(scene->isPopupShowing(tip.get()), "the tooltip shows after the updated delay");
    scene->layout(200, 120, 10.2);
    Expect(!scene->isPopupShowing(tip.get()), "the tooltip hides when its show duration elapses");
}

void TestInstallMovesHost() {
    auto first = jadefx::make<jadefx::Label>("A");
    auto second = jadefx::make<jadefx::Label>("B");
    auto tip = jadefx::make<jadefx::Tooltip>("Move");
    auto other = jadefx::make<jadefx::Label>("Other");
    auto replaced = jadefx::make<jadefx::Tooltip>("Old");
    jadefx::Tooltip::install(first.get(), tip);
    jadefx::HoverPopup foreign;
    foreign.content = other;
    first->setHoverPopup(foreign);
    jadefx::Tooltip::install(second.get(), tip);
    Expect(first->getHoverPopup() != nullptr && first->getHoverPopup()->content.get() == other.get(),
           "moving a tooltip leaves a host whose popup is something else");
    Expect(second->getHoverPopup() != nullptr && second->getHoverPopup()->content.get() == tip.get(),
           "install attaches the new host");

    jadefx::Tooltip::install(second.get(), replaced);
    tip->setShowDelay(4);
    Expect(second->getHoverPopup() != nullptr && second->getHoverPopup()->content.get() == replaced.get(),
           "a detached tooltip does not overwrite the new host");
    Expect(second->getHoverPopup()->showDelay == 1, "the replacement keeps its own delay");
    replaced->setShowDelay(3);
    Expect(second->getHoverPopup()->showDelay == 3, "the installed tooltip publishes delay changes");

    jadefx::Tooltip::install(nullptr, tip);
    jadefx::Tooltip::uninstall(nullptr);
    Expect(second->getHoverPopup() != nullptr && second->getHoverPopup()->content.get() == replaced.get(),
           "a null node is ignored");
    jadefx::Tooltip::install(second.get(), nullptr);
    Expect(second->getHoverPopup() == nullptr, "a null tooltip clears the node");

    jadefx::Tooltip::install(first.get(), tip);
    jadefx::Tooltip::uninstall(first.get());
    Expect(first->getHoverPopup() == nullptr, "uninstall removes the tooltip");
}

void TestTooltipLifetime() {
    auto tag = jadefx::make<jadefx::Label>("T");
    tag->setPrefSize(40, 20);
    {
        auto tip = jadefx::make<jadefx::Tooltip>("X");
        jadefx::Tooltip::install(tag.get(), tip);
        jadefx::Tooltip::uninstall(tag.get());
    }
    {
        auto tip = jadefx::make<jadefx::Tooltip>("Y");
        jadefx::Tooltip::install(tag.get(), tip);
    }
    Expect(tag->getHoverPopup() != nullptr, "the host keeps the tooltip alive");
    tag->clearHoverPopup();
    Expect(tag->getHoverPopup() == nullptr, "clearing the popup drops the tooltip");
}

}  // namespace

int RunTooltipTests() {
    TestTooltipChrome();
    TestTooltipHover();
    TestZeroShowDelay();
    TestUpdatedDelays();
    TestInstallMovesHost();
    TestTooltipLifetime();
    return gFailures;
}

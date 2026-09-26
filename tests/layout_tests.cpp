#include "jadefx/jadefx.hpp"

#include "internal/Subpixel.hpp"
#include "platform/DesktopWindows.hpp"
#include "platform/GlfwHost.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

bool Near(double a, double b, double epsilon = 0.75) { return std::fabs(a - b) <= epsilon; }

bool EndsWith(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void TestColors() {
    bool ok = false;
    const jadefx::Color blue = jadefx::Color::parse("#40C5FA", &ok);
    Expect(ok, "parse #40C5FA");
    Expect(Near(blue.r, 64.f / 255.f, 0.01), "red channel");
    Expect(Near(blue.g, 197.f / 255.f, 0.01), "green channel");
    Expect(Near(blue.b, 250.f / 255.f, 0.01), "blue channel");

    const jadefx::Color fade = jadefx::Color::parse("rgba(0, 0, 0, .4)", &ok);
    Expect(ok && Near(fade.a, 0.4f, 0.02), "rgba alpha with a leading dot");

    const jadefx::Color hexAlpha = jadefx::Color::parse("#FFFFFF50", &ok);
    Expect(ok && Near(hexAlpha.a, 0x50 / 255.f, 0.02), "8 digit hex alpha");
    Expect(jadefx::Color::parse("transparent").a == 0.f, "transparent");
}

void TestStylesheetHover() {
    auto button = jadefx::make<jadefx::StackPane>();
    button->setPrefSize(120, 40);
    button->getClassList().add("test-button");
    auto scene = jadefx::make<jadefx::Scene>(button, 200, 120);
    scene->setStylesheet(
        ".test-button { background-color: #ffffff; transition: background-color 0.1s; }"
        ".test-button:hover { background-color: #F6F9FE; }");
    scene->layout(200, 120, 0);
    const jadefx::Color resting = button->computedStyle().background.color;
    Expect(resting.r > 0.95f && resting.g > 0.95f && resting.b > 0.95f, "button starts white");

    scene->noteMove(button->getAbsoluteX() + 4, button->getAbsoluteY() + 4);
    scene->layout(200, 120, 0);
    scene->layout(200, 120, 1);
    const jadefx::Color hovered = button->computedStyle().background.color;
    const jadefx::Color expected = jadefx::Color::parse("#F6F9FE");
    Expect(Near(hovered.r, expected.r, 0.02) && Near(hovered.g, expected.g, 0.02) &&
               Near(hovered.b, expected.b, 0.02),
           "hover background");
}

void TestVBox() {
    auto box = jadefx::make<jadefx::VBox>();
    box->setSpacing(8);
    jadefx::Node* panes[3] = {};
    for (int i = 0; i < 3; ++i) {
        auto pane = jadefx::make<jadefx::Pane>();
        pane->setPrefSize(10, 10);
        panes[i] = pane.get();
        box->getChildren().add(std::move(pane));
    }
    auto scene = jadefx::make<jadefx::Scene>(box, 200, 200);
    scene->layout(200, 200, 0);
    Expect(Near(box->getWidth(), 10), "vbox width");
    Expect(Near(box->getHeight(), 46), "vbox height");
    Expect(Near(panes[0]->getAbsoluteY(), box->getAbsoluteY()), "first child at the top of the box");
    Expect(Near(panes[1]->getAbsoluteY() - panes[0]->getAbsoluteY(), 18), "spacing between children");
    Expect(Near(panes[2]->getAbsoluteY() - panes[1]->getAbsoluteY(), 18), "spacing between later children");
}

void TestLayoutImprovements() {
    {
        auto filled = jadefx::make<jadefx::Pane>();
        filled->setStyle("width: 80px; height: 100%;");
        auto calculated = jadefx::make<jadefx::Pane>();
        calculated->setStyle("width: 50px; height: calc(100% - 20px);");
        jadefx::Node* filledNode = filled.get();
        jadefx::Node* calculatedNode = calculated.get();
        auto tall = jadefx::make<jadefx::VBox>();
        tall->setPrefSize(200, 180);
        tall->getChildren().add(std::move(filled));
        auto shortBox = jadefx::make<jadefx::VBox>();
        shortBox->setPrefSize(200, 180);
        shortBox->getChildren().add(std::move(calculated));
        auto raised = jadefx::make<jadefx::Pane>();
        raised->setStyle("width: 10px; height: 10px; min-height: 50%;");
        jadefx::Node* raisedNode = raised.get();
        auto minBox = jadefx::make<jadefx::VBox>();
        minBox->setPrefSize(100, 100);
        minBox->getChildren().add(std::move(raised));
        auto tallScene = jadefx::make<jadefx::Scene>(tall, 200, 180);
        tallScene->layout(200, 180, 0);
        Expect(Near(filledNode->getWidth(), 80), "vbox percent width");
        Expect(Near(filledNode->getHeight(), 180), "vbox percent height");
        auto calcScene = jadefx::make<jadefx::Scene>(shortBox, 200, 180);
        calcScene->layout(200, 180, 0);
        Expect(Near(calculatedNode->getHeight(), 160), "vbox calc height");
        auto minScene = jadefx::make<jadefx::Scene>(minBox, 100, 100);
        minScene->layout(100, 100, 0);
        Expect(Near(raisedNode->getHeight(), 50), "vbox percent min-height");
    }
    {
        auto pane = jadefx::make<jadefx::Pane>();
        pane->setPrefSize(120, 80);
        pane->setPadding(jadefx::Insets::uniform(8));
        auto child = jadefx::make<jadefx::Pane>();
        child->setPrefSize(40, 30);
        jadefx::Node* raw = child.get();
        pane->getChildren().add(std::move(child));
        auto scene = jadefx::make<jadefx::Scene>(pane, 120, 80);
        scene->layout(120, 80, 0);
        Expect(Near(raw->getX(), 8) && Near(raw->getY(), 8), "pane child sits inside padding");
        Expect(Near(raw->getWidth(), 40) && Near(raw->getHeight(), 30), "pane child keeps its preferred size");
    }
    {
        auto box = jadefx::make<jadefx::VBox>();
        box->setPrefSize(200, 80);
        auto stack = jadefx::make<jadefx::StackPane>();
        stack->setPrefSize(40, 20);
        auto label = jadefx::make<jadefx::Label>("Hi");
        label->setPrefSize(40, 20);
        jadefx::Node* stackNode = stack.get();
        jadefx::Node* labelNode = label.get();
        box->getChildren().add(std::move(stack));
        box->getChildren().add(std::move(label));
        auto scene = jadefx::make<jadefx::Scene>(box, 200, 80);
        scene->layout(200, 80, 0);
        Expect(Near(stackNode->getX(), 0), "stack pane follows the vbox alignment");
        Expect(Near(labelNode->getX(), 0), "label follows the vbox alignment");
    }
    {
        auto box = jadefx::make<jadefx::HBox>();
        box->setPrefSize(200, 100);
        box->setAlignment(jadefx::Pos::Center);
        auto nested = jadefx::make<jadefx::VBox>();
        nested->setPrefSize(40, 20);
        jadefx::Node* nestedNode = nested.get();
        box->getChildren().add(std::move(nested));
        auto scene = jadefx::make<jadefx::Scene>(box, 200, 100);
        scene->layout(200, 100, 0);
        Expect(Near(nestedNode->getY(), 40), "vbox is vertically centered in the hbox");
        Expect(Near(nestedNode->getX(), 80), "single hbox child is centered on the row");
    }
    {
        auto root = jadefx::make<jadefx::Pane>();
        root->setPrefSize(10, 10);
        auto scene = jadefx::make<jadefx::Scene>(root, 200, 100);
        scene->setStylesheet("scene { alignment: top-left; }");
        scene->layout(200, 100, 0);
        Expect(Near(root->getAbsoluteX(), 0) && Near(root->getAbsoluteY(), 0), "scene alignment positions the root");
    }
    {
        auto border = jadefx::make<jadefx::BorderPane>();
        border->setPrefSize(300, 200);
        border->setSpacing(16);
        auto top = jadefx::make<jadefx::Pane>();
        auto bottom = jadefx::make<jadefx::Pane>();
        auto left = jadefx::make<jadefx::Pane>();
        auto right = jadefx::make<jadefx::Pane>();
        auto center = jadefx::make<jadefx::Pane>();
        top->setPrefHeight(30);
        bottom->setPrefHeight(20);
        left->setPrefWidth(40);
        right->setPrefWidth(50);
        jadefx::Node* topNode = top.get();
        jadefx::Node* bottomNode = bottom.get();
        jadefx::Node* leftNode = left.get();
        jadefx::Node* rightNode = right.get();
        jadefx::Node* centerNode = center.get();
        border->setTop(std::move(top));
        border->setBottom(std::move(bottom));
        border->setLeft(std::move(left));
        border->setRight(std::move(right));
        border->setCenter(std::move(center));
        auto scene = jadefx::make<jadefx::Scene>(border, 300, 200);
        scene->layout(300, 200, 0);
        Expect(Near(centerNode->getX() - (leftNode->getX() + leftNode->getWidth()), 16), "spacing before center");
        Expect(Near(rightNode->getX() - (centerNode->getX() + centerNode->getWidth()), 16), "spacing after center");
        Expect(Near(leftNode->getY() - (topNode->getY() + topNode->getHeight()), 16), "spacing under the top");
        Expect(Near(bottomNode->getY() - (leftNode->getY() + leftNode->getHeight()), 16), "spacing above the bottom");
    }
    {
        auto border = jadefx::make<jadefx::BorderPane>();
        border->setPrefSize(300, 200);
        auto center = jadefx::make<jadefx::Pane>();
        center->setPrefSize(40, 30);
        center->setMaxSize(40, 30);
        jadefx::Node* centerNode = center.get();
        border->setCenter(std::move(center));
        auto scene = jadefx::make<jadefx::Scene>(border, 300, 200);
        scene->layout(300, 200, 0);
        Expect(Near(centerNode->getWidth(), 40) && Near(centerNode->getHeight(), 30), "center keeps its maximum size");
        Expect(Near(centerNode->getX(), 130) && Near(centerNode->getY(), 85), "smaller center is aligned in the slot");
    }
    {
        auto border = jadefx::make<jadefx::BorderPane>();
        border->setPrefSize(300, 160);
        auto left = jadefx::make<jadefx::Pane>();
        left->setPrefWidth(40);
        jadefx::Node* leftNode = left.get();
        border->setLeft(left);
        auto scene = jadefx::make<jadefx::Scene>(border, 300, 160);
        scene->layout(300, 160, 0);
        Expect(Near(leftNode->getWidth(), 40) && Near(leftNode->getHeight(), 160), "left region fills the middle height");
        left->setPrefHeight(20);
        left->setMaxSize(40, 20);
        scene->layout(300, 160, 0);
        Expect(Near(leftNode->getHeight(), 20), "left region keeps its maximum height");
    }
    {
        auto border = jadefx::make<jadefx::BorderPane>();
        border->setPrefSize(220, 80);
        auto top = jadefx::make<jadefx::Pane>();
        auto bottom = jadefx::make<jadefx::Pane>();
        top->setPrefSize(10, 50);
        bottom->setPrefSize(10, 50);
        jadefx::Node* topNode = top.get();
        jadefx::Node* bottomNode = bottom.get();
        border->setTop(std::move(top));
        border->setBottom(std::move(bottom));
        auto scene = jadefx::make<jadefx::Scene>(border, 220, 80);
        scene->layout(220, 80, 0);
        Expect(Near(topNode->getHeight(), 40) && Near(bottomNode->getHeight(), 40), "top and bottom shrink to fit");
        Expect(Near(bottomNode->getY() + bottomNode->getHeight(), 80), "bottom stays inside the pane");
    }
    {
        auto node = jadefx::make<jadefx::Pane>();
        node->setPrefSize(30, 16);
        auto hold = jadefx::make<jadefx::VBox>();
        hold->getChildren().add(node);
        auto border = jadefx::make<jadefx::BorderPane>();
        border->setPrefSize(220, 140);
        border->setCenter(node);
        Expect(hold->getChildren().size() == 0, "center slot leaves the old child list");
        Expect(node->getParent() == border.get(), "center slot becomes the parent");
        auto scene = jadefx::make<jadefx::Scene>(border, 220, 140);
        scene->layout(220, 140, 0);
        Expect(Near(node->getWidth(), 30) && Near(node->getHeight(), 16), "center keeps its explicit size");
        Expect(Near(node->getX(), 95) && Near(node->getY(), 62), "explicit center is aligned in the pane");

        auto next = jadefx::make<jadefx::VBox>();
        next->getChildren().add(node);
        Expect(border->getCenter() == nullptr, "adding the center elsewhere clears the slot");
        Expect(node->getParent() == next.get(), "the new child list owns the node");
    }
    {
        auto box = jadefx::make<jadefx::HBox>();
        box->setPrefSize(200, 40);
        box->setSpacing(20);
        auto first = jadefx::make<jadefx::Pane>();
        auto second = jadefx::make<jadefx::Pane>();
        first->setStyle("width: 50%; height: 40px;");
        second->setStyle("width: 50%; height: 40px;");
        jadefx::Node* firstNode = first.get();
        jadefx::Node* secondNode = second.get();
        box->getChildren().add(std::move(first));
        box->getChildren().add(std::move(second));
        auto scene = jadefx::make<jadefx::Scene>(box, 200, 40);
        scene->layout(200, 40, 0);
        Expect(Near(firstNode->getWidth(), 90) && Near(secondNode->getWidth(), 90), "percentage widths leave room for spacing");
        Expect(Near(secondNode->getX() + secondNode->getWidth(), 200), "hbox row ends at the box edge");
    }
}

void TestCalcAndBorder() {
    auto scene = jadefx::make<jadefx::Scene>(jadefx::make<jadefx::StackPane>(), 375, 667);
    scene->setStylesheet(
        ".main { width: calc(100% - 48px); height: calc(100% - 48px); background-color: transparent; }");
    auto layout = jadefx::make<jadefx::BorderPane>();
    layout->getClassList().add("main");
    auto bottom = jadefx::make<jadefx::VBox>();
    bottom->setSpacing(16);
    auto button = jadefx::make<jadefx::StackPane>();
    button->setStyle("width: 100%; height: 40px; background-color: white;");
    bool clicked = false;
    button->setOnMouseClicked([&](const jadefx::MouseEvent&) { clicked = true; });
    bottom->getChildren().add(button);
    layout->setBottom(bottom);
    auto center = jadefx::make<jadefx::Label>("Hello");
    layout->setCenter(center);
    scene->setRoot(layout);
    scene->layout(375, 667, 0);

    Expect(Near(layout->getWidth(), 327), "calc width");
    Expect(Near(layout->getHeight(), 619), "calc height");
    Expect(Near(layout->getAbsoluteX(), 24), "centered x");
    Expect(Near(layout->getAbsoluteY(), 24), "centered y");
    Expect(Near(button->getWidth(), layout->getWidth()), "stretched button");
    Expect(button->getAbsoluteY() > center->getAbsoluteY(), "button sits below the center");
    Expect(center->getWidth() > 20, "label has a measured width");

    const double x = button->getAbsoluteX() + button->getWidth() * 0.5;
    const double y = button->getAbsoluteY() + button->getHeight() * 0.5;
    scene->noteButton(0, true, x, y);
    scene->noteButton(0, false, x, y);
    Expect(clicked, "click reaches the button");
}

void TestLabelEllipsis() {
    const std::string mark = "\u2026";
    Expect(mark.size() == 3 && static_cast<unsigned char>(mark[0]) == 0xE2 &&
               static_cast<unsigned char>(mark[1]) == 0x80 && static_cast<unsigned char>(mark[2]) == 0xA6,
           "the ellipsis is U+2026");
    const jadefx::Font font("Open Sans", 16.f);
    const float full = font.measureWidth("Center");
    const float markWidth = font.measureWidth(mark);
    Expect(full > markWidth + 4.f, "Center is wider than the ellipsis");
    const double target = (static_cast<double>(markWidth) + static_cast<double>(full)) * 0.5;

    {
        auto label = jadefx::make<jadefx::Label>("Center");
        jadefx::Label* raw = label.get();
        auto scene = jadefx::make<jadefx::Scene>(label, 400, 80);
        scene->layout(400, 80, 0);
        Expect(raw->displayedText() == "Center", "a wide label keeps its text");
        Expect(Near(raw->getWidth(), full), "a wide label stays at the text width");
        Expect(raw->getText() == "Center", "the original text stays on the label");
    }
    {
        auto label = jadefx::make<jadefx::Label>("Center");
        jadefx::Label* raw = label.get();
        auto scene = jadefx::make<jadefx::Scene>(label, target, 80);
        scene->layout(target, 80, 0);
        const std::string shown = raw->displayedText();
        const std::string body = shown.size() >= mark.size() ? shown.substr(0, shown.size() - mark.size()) : shown;
        Expect(Near(raw->getWidth(), target), "a narrow window forces the label down to the window");
        Expect(raw->getText() == "Center", "narrowing the window keeps the original text");
        Expect(EndsWith(shown, mark), "a narrow label ends with an ellipsis");
        Expect(shown != "Center", "a narrow label does not draw the whole word");
        Expect(font.measureWidth(shown) <= raw->getWidth() + 0.05f, "the drawn line fits the label");
        Expect(std::string("Center").compare(0, body.size(), body) == 0, "the drawn line keeps the start of the word");
    }
    {
        const std::string word = std::string("Caf") + "\u00e9";
        auto label = jadefx::make<jadefx::Label>(word);
        jadefx::Label* raw = label.get();
        const double cafeWidth = font.measureWidth(word);
        const double cafeTarget = (static_cast<double>(markWidth) + cafeWidth) * 0.5;
        auto scene = jadefx::make<jadefx::Scene>(label, cafeTarget, 80);
        scene->layout(cafeTarget, 80, 0);
        const std::string shown = raw->displayedText();
        const std::string body = shown.size() >= mark.size() ? shown.substr(0, shown.size() - mark.size()) : shown;
        Expect(EndsWith(shown, mark), "a word with an accent keeps the ellipsis");
        Expect(word.compare(0, body.size(), body) == 0, "the accented prefix is taken from the word");
        Expect(body.size() == word.size() || (static_cast<unsigned char>(word[body.size()]) & 0xC0) != 0x80,
               "the ellipsis does not split a character");
    }
    {
        auto border = jadefx::make<jadefx::BorderPane>();
        border->setPrefSize(320, 200);
        auto wide = jadefx::make<jadefx::Label>("Center");
        jadefx::Label* wideLabel = wide.get();
        border->setCenter(wide);
        auto wideScene = jadefx::make<jadefx::Scene>(border, 320, 200);
        wideScene->layout(320, 200, 0);
        Expect(wideLabel->getWidth() > full, "a wide center slot is bigger than the word");
        Expect(wideLabel->displayedText() == "Center", "extra room in the center slot does not add an ellipsis");

        auto narrowBorder = jadefx::make<jadefx::BorderPane>();
        narrowBorder->setPrefSize(target, 80);
        auto narrow = jadefx::make<jadefx::Label>("Center");
        jadefx::Label* narrowLabel = narrow.get();
        narrowBorder->setCenter(narrow);
        auto narrowScene = jadefx::make<jadefx::Scene>(narrowBorder, target, 80);
        narrowScene->layout(target, 80, 0);
        Expect(Near(narrowLabel->getWidth(), target), "the center slot forces the label width");
        Expect(EndsWith(narrowLabel->displayedText(), mark), "a narrow center slot ends with an ellipsis");
    }
    {
        // Same structure as the border sample: the word sits in a padded region between two minimum-width sides.
        constexpr const char* css = R"CSS(
.box { width: calc(100% - 64px); height: calc(100% - 64px); padding: 8px; }
.region { padding: 12px 20px; }
.left, .right { min-width: 96px; }
)CSS";
        const double sceneWidth = target + 64.0 + 16.0 + 96.0 + 96.0 + 40.0;
        auto layout = jadefx::make<jadefx::BorderPane>();
        layout->getClassList().add("box");
        auto centerLabel = jadefx::make<jadefx::Label>("Center");
        jadefx::Label* raw = centerLabel.get();
        auto region = [](std::shared_ptr<jadefx::Node> child, const char* slot) {
            auto pane = jadefx::make<jadefx::StackPane>(std::move(child));
            pane->getClassList().add("region");
            pane->getClassList().add(slot);
            return pane;
        };
        layout->setTop(region(jadefx::make<jadefx::Label>("Top"), "top"));
        layout->setLeft(region(jadefx::make<jadefx::Label>("Left"), "left"));
        layout->setRight(region(jadefx::make<jadefx::Label>("Right"), "right"));
        layout->setBottom(region(jadefx::make<jadefx::Label>("Bottom"), "bottom"));
        layout->setCenter(region(centerLabel, "center"));
        auto scene = jadefx::make<jadefx::Scene>(layout, sceneWidth, 360);
        scene->setStylesheet(css);
        scene->layout(sceneWidth, 360, 0);
        const std::string shown = raw->displayedText();
        Expect(raw->getWidth() + 1.0 < full, "the center region forces Center down");
        Expect(raw->getWidth() > markWidth, "the center region still has room for an ellipsis");
        Expect(raw->getText() == "Center", "the center label keeps its text");
        Expect(EndsWith(shown, mark), "the center label in the border sample ends with an ellipsis");
        Expect(font.measureWidth(shown) <= raw->getWidth() + 0.05f, "the center line fits in the region");
    }
}

void TestCssFixes() {
    auto root = jadefx::make<jadefx::VBox>();
    auto box = jadefx::make<jadefx::VBox>();
    box->getClassList().add("box");
    auto child = jadefx::make<jadefx::Label>("Child");
    auto mid = jadefx::make<jadefx::VBox>();
    auto grand = jadefx::make<jadefx::Label>("Grand");
    mid->getChildren().add(grand);
    box->getChildren().add(child);
    box->getChildren().add(mid);
    auto decoy = jadefx::make<jadefx::Label>("Decoy");
    decoy->getClassList().add("box");
    auto sized = jadefx::make<jadefx::Pane>();
    sized->getClassList().add("sized");
    sized->setPrefSize(80, 30);
    auto gradient = jadefx::make<jadefx::Pane>();
    gradient->getClassList().add("fade");
    gradient->setPrefSize(40, 20);
    auto button = jadefx::make<jadefx::StackPane>();
    button->getClassList().add("btn");
    button->setPrefSize(120, 36);
    auto buttonLabel = jadefx::make<jadefx::Label>("Go");
    button->getChildren().add(buttonLabel);
    root->getChildren().add(box);
    root->getChildren().add(decoy);
    root->getChildren().add(sized);
    root->getChildren().add(gradient);
    root->getChildren().add(button);

    auto scene = jadefx::make<jadefx::Scene>(root, 320, 400);
    scene->setStylesheet(
        "* { color: #010203; }"
        "[href], div >, label:focus-visible { color: #ff00ff; }"
        ".box label { color: #ff0000; }"
        ".box > label { color: #00ff00; }"
        ".sized { font-size: 20px; width: 2em; }"
        ".sized, .keep { width: 1.2foo; }"
        ".fade { background-image: linear-gradient(to bottom, #ff0000, #00ff00, #0000ff);"
        " background-color: white; }"
        ".btn { background-color: white; border-style: solid; border-color: black; border-width: 0px;"
        " transition: background-color 0.1s ease, border-width 0.1s ease; }"
        ".btn:hover { background-color: black; border-width: 10px; }"
        ".btn:focus-within { background-color: #00ff00; }"
        ".btn:focus { background-color: #ff0000; }");
    scene->layout(320, 400, 0);

    Expect(Near(child->computedStyle().color.g, 1.f, 0.02), "child combinator wins over descendant");
    Expect(Near(grand->computedStyle().color.r, 1.f, 0.02) && grand->computedStyle().color.g < 0.05f,
           "descendant matches a grandchild");
    Expect(decoy->computedStyle().color.r < 0.05f && Near(decoy->computedStyle().color.b, 3.f / 255.f, 0.02),
           "a label with the ancestor class is not its own descendant");
    Expect(Near(sized->getWidth(), 40), "2em uses the cascaded font size");
    Expect(sized->computedStyle().background.gradient == false, "bad unit does not invent a background");
    Expect(gradient->computedStyle().background.gradient, "background-color keeps the gradient");
    Expect(gradient->computedStyle().background.hasColor, "background-color is still recorded");
    Expect(Near(gradient->computedStyle().background.color.r, 1.f, 0.02), "recorded background is white");
    Expect(Near(gradient->computedStyle().background.angleDeg, 180.f, 0.1f), "to bottom is 180 degrees");
    Expect(gradient->computedStyle().background.stopCount == 3, "middle gradient stop is kept");
    Expect(Near(gradient->computedStyle().background.stops[1].g, 1.f, 0.02), "middle stop is green");
    Expect(Near(gradient->computedStyle().background.stopAt[1], 0.5f, 0.02f), "middle stop sits halfway");

    scene->noteMove(button->getAbsoluteX() + 4, button->getAbsoluteY() + 4);
    scene->layout(320, 400, 0);
    Expect(button->computedStyle().background.color.r > 0.9f, "ease does not skip the transition");
    Expect(button->computedStyle().border.top < 1.0, "border-width transition starts at the old width");
    scene->layout(320, 400, 1);
    Expect(button->computedStyle().background.color.r < 0.05f, "background transition finishes");
    Expect(Near(button->computedStyle().border.top, 10), "border-width transition finishes");

    scene->noteMove(-10, -10);
    const double labelX = buttonLabel->getAbsoluteX() + buttonLabel->getWidth() * 0.5;
    const double labelY = buttonLabel->getAbsoluteY() + buttonLabel->getHeight() * 0.5;
    scene->noteButton(0, true, labelX, labelY);
    scene->layout(320, 400, 2);
    scene->layout(320, 400, 3);
    Expect(Near(button->computedStyle().background.color.g, 1.f, 0.05f), ":focus-within matches the button");
    Expect(button->computedStyle().background.color.r < 0.2f, ":focus does not match an unfocused parent");
}

void TestFont() {
    const jadefx::Font font("Open Sans", 18.f);
    Expect(font.measureWidth("Hello") > 20.f, "text has width");
    Expect(font.lineHeight() > 10.f, "line height");
    Expect(font.measureWidth("Hello") < font.measureWidth("Hello World"), "longer text is wider");
}

const unsigned char* PixelAt(const jadefx::SubpixelBitmap& image, int pixelX) {
    const int column = pixelX - image.xoff;
    if (column < 0 || column >= image.width || image.rgb.empty()) {
        return nullptr;
    }
    return image.rgb.data() + static_cast<std::size_t>(column * 3);
}

void TestSubpixelCoverage() {
    int pixel = -1;
    Expect(jadefx::SubpixelPhase(10.f, pixel) == 0 && pixel == 10, "exact pixel stays on phase 0");
    Expect(jadefx::SubpixelPhase(10.2f, pixel) == 1 && pixel == 10, "0.2 snaps to the one-third stripe");
    Expect(jadefx::SubpixelPhase(10.9f, pixel) == 0 && pixel == 11, "0.9 carries onto the next pixel");
    Expect(jadefx::SubpixelPhase(-1.2f, pixel) == 2 && pixel == -2, "negative positions snap to a stripe");

    unsigned char blank[6] = {};
    const jadefx::SubpixelBitmap empty = jadefx::PackSubpixelCoverage(blank, 6, 1, 6, 0, 4);
    Expect(empty.width == 0 && empty.height == 0, "blank coverage stays blank");

    // One lit green stripe becomes a nearly neutral pixel, a little stronger in green.
    unsigned char green[3] = {0, 255, 0};
    const jadefx::SubpixelBitmap impulse = jadefx::PackSubpixelCoverage(green, 3, 1, 3, 0, 0);
    const unsigned char* impulsePixel = PixelAt(impulse, 0);
    Expect(impulsePixel != nullptr && impulsePixel[0] == 85 && impulsePixel[1] == 86 && impulsePixel[2] == 85,
           "a single stripe spreads across red, green, and blue");

    // Four solid pixels. Interior stays gray; the leading edge keeps blue and the trailing edge keeps red.
    unsigned char solid[12];
    for (unsigned char& sample : solid) {
        sample = 255;
    }
    const jadefx::SubpixelBitmap run = jadefx::PackSubpixelCoverage(solid, 12, 1, 12, 0, -3);
    const unsigned char* leading = PixelAt(run, run.xoff);
    const unsigned char* trailing = PixelAt(run, run.xoff + run.width - 1);
    Expect(run.yoff == -3, "vertical offset is preserved");
    Expect(leading != nullptr && leading[0] == 0 && leading[1] == 0 && leading[2] == 85, "leading edge is the blue stripe");
    Expect(trailing != nullptr && trailing[0] == 85 && trailing[1] == 0 && trailing[2] == 0,
           "trailing edge is the red stripe");
    bool neutral = false;
    for (int x = 0; x < run.width; ++x) {
        const unsigned char* pixel = run.rgb.data() + static_cast<std::size_t>(x * 3);
        if (pixel[0] == 255 && pixel[1] == 255 && pixel[2] == 255) {
            neutral = true;
        }
    }
    Expect(neutral, "a solid run stays neutral gray");

    const jadefx::SubpixelBitmap shifted = jadefx::PackSubpixelCoverage(solid, 12, 1, 12, 3, 0);
    const unsigned char* shiftedLeading = PixelAt(shifted, shifted.xoff);
    Expect(shifted.xoff == run.xoff + 1, "three stripes later is one pixel later");
    Expect(shiftedLeading != nullptr && shiftedLeading[0] == 0 && shiftedLeading[1] == 0 && shiftedLeading[2] == 85,
           "the leading fringe moves with the stripe grid");
}

void TestFontSmoothing() {
    auto label = jadefx::make<jadefx::Label>("H");
    auto scene = jadefx::make<jadefx::Scene>(label, 200, 80);
    scene->layout(200, 80, 0);
    Expect(label->isSubpixelRendering(), "subpixel rendering starts on");

    label->setSubpixelRendering(false);
    Expect(!label->isSubpixelRendering(), "the setter turns subpixel rendering off");
    scene->layout(200, 80, 0);
    Expect(!label->isSubpixelRendering(), "the setter survives layout");

    label->setSubpixelRendering(true);
    label->setStyle("font-smoothing: antialiased;");
    scene->layout(200, 80, 0);
    Expect(!label->isSubpixelRendering(), "antialiased is grayscale");

    label->setStyle("font-smoothing: subpixel-antialiased;");
    scene->layout(200, 80, 0);
    Expect(label->isSubpixelRendering(), "subpixel-antialiased turns stripes back on");

    label->setStyle("font-smoothing: AUTO;");
    scene->layout(200, 80, 0);
    Expect(label->isSubpixelRendering(), "auto keeps subpixel rendering");

    label->setStyle("font-smoothing: none;");
    scene->layout(200, 80, 0);
    Expect(!label->isSubpixelRendering(), "none turns subpixel rendering off");

    label->setStyle("font-smoothing: grayscale;");
    scene->layout(200, 80, 0);
    Expect(!label->isSubpixelRendering(), "grayscale turns subpixel rendering off");

    auto box = jadefx::make<jadefx::VBox>();
    box->getClassList().add("smooth");
    auto inherited = jadefx::make<jadefx::Label>("A");
    auto overridden = jadefx::make<jadefx::Label>("B");
    overridden->setStyle("font-smoothing: subpixel-antialiased;");
    auto unknown = jadefx::make<jadefx::Label>("C");
    unknown->setStyle("font-smoothing: crispy;");
    box->getChildren().add(inherited);
    box->getChildren().add(overridden);
    box->getChildren().add(unknown);
    auto parentScene = jadefx::make<jadefx::Scene>(box, 200, 120);
    parentScene->setStylesheet(".smooth { font-smoothing: antialiased; }");
    parentScene->layout(200, 120, 0);
    Expect(!inherited->isSubpixelRendering(), "font smoothing inherits");
    Expect(overridden->isSubpixelRendering(), "a child can turn subpixel rendering back on");
    Expect(!unknown->isSubpixelRendering(), "an unknown smoothing value does not reset the inherited mode");
}

struct GlyphShot {
    int fringe = 0;
    int dark = 0;
    int light = 0;
    bool ok = false;
};

GlyphShot CaptureGlyph(const char* path, const char* style) {
    GlyphShot shot;
    jadefx::GlfwHost host;
    if (!host.create(240, 160, "subpixel")) {
        return shot;
    }
    jadefx::Stage stage;
    if (!stage.initializeGraphics(&jadefx::GlfwHost::proc)) {
        host.destroy();
        return shot;
    }

    auto label = jadefx::make<jadefx::Label>("H");
    label->setFont(jadefx::Font("Open Sans", 64.f));
    if (style != nullptr) {
        label->setStyle(style);
    }
    stage.setScene(jadefx::make<jadefx::Scene>(label, 240, 160));

#if defined(_WIN32)
    _putenv_s("JADEFX_DUMP_PPM", path);
#else
    setenv("JADEFX_DUMP_PPM", path, 1);
#endif
    int pointWidth = 0;
    int pointHeight = 0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    host.windowSize(pointWidth, pointHeight);
    host.framebufferSize(framebufferWidth, framebufferHeight);
    const bool first = stage.frame(pointWidth, pointHeight, framebufferWidth, framebufferHeight);
    const bool second = stage.frame(pointWidth, pointHeight, framebufferWidth, framebufferHeight);
#if defined(_WIN32)
    _putenv_s("JADEFX_DUMP_PPM", "");
#else
    unsetenv("JADEFX_DUMP_PPM");
#endif

    FILE* file = std::fopen(path, "rb");
    int width = 0;
    int height = 0;
    const bool header = file != nullptr && std::fscanf(file, "P6\n%d %d\n255\n", &width, &height) == 2 && width > 0 &&
                        height > 0;
    std::vector<unsigned char> pixels;
    if (header) {
        pixels.resize(static_cast<std::size_t>(width * height * 3));
        if (std::fread(pixels.data(), 1, pixels.size(), file) != pixels.size()) {
            pixels.clear();
        }
    }
    if (file != nullptr) {
        std::fclose(file);
    }
    std::remove(path);
    stage.shutdownGraphics();
    host.destroy();

    if (!first || !second || !stage.graphicsOk() || pixels.empty()) {
        return shot;
    }
    for (std::size_t i = 0; i + 2 < pixels.size(); i += 3) {
        const int red = pixels[i];
        const int green = pixels[i + 1];
        const int blue = pixels[i + 2];
        const int spread = std::max(red, std::max(green, blue)) - std::min(red, std::min(green, blue));
        if (spread >= 8) {
            ++shot.fringe;
        }
        if (red < 40 && green < 40 && blue < 40) {
            ++shot.dark;
        }
        if (red > 230 && green > 230 && blue > 230) {
            ++shot.light;
        }
    }
    shot.ok = true;
    return shot;
}

void TestSubpixelFrame() {
    const GlyphShot shot = CaptureGlyph("jadefx-subpixel.ppm", nullptr);
    Expect(shot.ok, "subpixel frame draws without a GL error");
    Expect(shot.fringe > 20, "glyph edges carry separate red, green, and blue coverage");
    Expect(shot.dark > 20, "glyph interior is covered");
    Expect(shot.light > 20, "background stays light");
}

void TestResizeRedraws() {
    jadefx::GlfwHost host;
    if (!host.create(240, 160, "resize")) {
        Expect(false, "resize window opens");
        return;
    }
    jadefx::Stage stage;
    if (!stage.initializeGraphics(&jadefx::GlfwHost::proc)) {
        Expect(false, "resize context");
        host.destroy();
        return;
    }
    host.bind(&stage);
    int calls = 0;
    int seenWidth = 0;
    int seenHeight = 0;
    host.setRedraw([&] {
        int pointWidth = 0;
        int pointHeight = 0;
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        host.windowSize(pointWidth, pointHeight);
        host.framebufferSize(framebufferWidth, framebufferHeight);
        seenWidth = pointWidth;
        seenHeight = pointHeight;
        ++calls;
        stage.frame(pointWidth, pointHeight, framebufferWidth, framebufferHeight);
        host.swap();
    });
    host.setSize(360, 240);
    host.poll();
    Expect(calls > 0, "resizing the window redraws before the main loop continues");
    Expect(seenWidth == 360 && seenHeight == 240, "the resize redraw sees the new window size");
    Expect(stage.getWidth() == 360 && stage.getHeight() == 240, "the scene adopts the new window size");
    Expect(stage.getScene().getWidth() == 360 && stage.getScene().getHeight() == 240, "layout uses the new window size");
    Expect(stage.graphicsOk(), "resize redraw does not raise a GL error");
    stage.shutdownGraphics();
    host.destroy();
}

jadefx::Node* FindClass(jadefx::Node& root, const std::string& name, std::size_t nth) {
    const std::vector<jadefx::Node*> found = root.getElementsByClassName(name);
    return nth < found.size() ? found[nth] : nullptr;
}

std::size_t CountVisible(jadefx::Node& root, const std::string& name) {
    std::size_t count = 0;
    for (jadefx::Node* node : root.getElementsByClassName(name)) {
        if (node != nullptr && node->isVisible()) {
            ++count;
        }
    }
    return count;
}

void ClickAt(jadefx::Scene& scene, jadefx::Node* node, double xBias) {
    const double x = node->getAbsoluteX() + xBias;
    const double y = node->getAbsoluteY() + node->getHeight() * 0.5;
    scene.noteButton(0, true, x, y);
    scene.noteButton(0, false, x, y);
}

void RightClick(jadefx::Scene& scene, jadefx::Node* node) {
    if (node == nullptr) {
        return;
    }
    const double x = node->getAbsoluteX() + node->getWidth() * 0.5;
    const double y = node->getAbsoluteY() + node->getHeight() * 0.5;
    scene.noteButton(1, true, x, y);
    scene.noteButton(1, false, x, y);
}

void ChooseMenu(jadefx::Scene& scene, const char* id) {
    scene.layout(scene.getWidth(), scene.getHeight(), 0);
    jadefx::Node* row = scene.getElementById(id);
    if (row != nullptr) {
        ClickAt(scene, row, row->getWidth() * 0.5);
    }
    scene.layout(scene.getWidth(), scene.getHeight(), 0);
}

void TestTabPane() {
    {
        auto firstPage = jadefx::make<jadefx::Label>("Alpha");
        auto secondPage = jadefx::make<jadefx::Label>("Beta");
        auto first = jadefx::make<jadefx::Tab>("Inbox", firstPage);
        auto second = jadefx::make<jadefx::Tab>("Sent", secondPage);
        first->setId("inbox");
        int firstChanges = 0;
        int secondChanges = 0;
        first->setOnSelectionChanged([&] { ++firstChanges; });
        second->setOnSelectionChanged([&] { ++secondChanges; });
        auto pane = jadefx::make<jadefx::TabPane>();
        pane->setPrefSize(320, 220);
        pane->getTabs().add(first);
        Expect(pane->getTabs().size() == 1, "a tab joins the pane");
        Expect(first->getTabPane() == pane.get(), "the tab remembers its pane");
        Expect(first->isSelected(), "the first tab is selected");
        Expect(firstChanges == 1, "selecting the first tab notifies it");
        pane->getTabs().add(second);
        Expect(secondChanges == 0, "a later tab stays unselected");
        Expect(pane->getSelectedIndex() == 0, "the selected index stays on the first tab");
        pane->getTabs().add(first);
        Expect(pane->getTabs().size() == 2, "the same tab is not inserted twice");

        auto scene = jadefx::make<jadefx::Scene>(pane, 320, 220);
        scene->layout(320, 220, 0);
        Expect(scene->getElementById("inbox") != nullptr, "the tab id is on its header");
        Expect(firstPage->getParent() == pane.get(), "the selected page is parented to the pane");
        Expect(secondPage->getParent() == nullptr, "an unselected page stays out of the scene");
        Expect(firstPage->getWidth() > 100, "the selected page fills the pane width");
        Expect(firstPage->getHeight() > 100, "the selected page fills the area under the headers");
        jadefx::Node* firstHeader = FindClass(*scene, "tab", 0);
        jadefx::Node* secondHeader = FindClass(*scene, "tab", 1);
        Expect(firstHeader != nullptr && secondHeader != nullptr, "each tab has a header");
        Expect(Near(secondHeader->getX(), firstHeader->getX() + firstHeader->getWidth() + 2),
               "headers sit beside each other");
        Expect(Near(firstPage->getY(), firstHeader->getY() + firstHeader->getHeight()),
               "the page starts under the selected header");
        Expect(CountVisible(*scene, "tab-close-button") == 1, "the default policy shows one close button");

        ClickAt(*scene, secondHeader, 8);
        scene->layout(320, 220, 0);
        Expect(second->isSelected() && !first->isSelected(), "clicking a header selects that tab");
        Expect(firstChanges == 2 && secondChanges == 1, "selection notifies the old tab and the new tab");
        Expect(secondPage->getParent() == pane.get(), "the newly selected page is shown");
        Expect(firstPage->getParent() == nullptr, "the previous page leaves the scene");
        Expect(Near(secondPage->getY(), secondHeader->getY() + secondHeader->getHeight()) ||
                   secondPage->getY() > secondHeader->getY(),
               "the new page is laid out in the content area");

        pane->setTabClosingPolicy(jadefx::TabPane::TabClosingPolicy::AllTabs);
        scene->layout(320, 220, 0);
        Expect(CountVisible(*scene, "tab-close-button") == 2, "all closable tabs show a close button");
        second->setClosable(false);
        scene->layout(320, 220, 0);
        Expect(CountVisible(*scene, "tab-close-button") == 1, "a tab that is not closable hides its close button");
        second->setClosable(true);
        pane->setTabClosingPolicy(jadefx::TabPane::TabClosingPolicy::Unavailable);
        scene->layout(320, 220, 0);
        Expect(CountVisible(*scene, "tab-close-button") == 0, "unavailable policy hides every close button");
        pane->setTabClosingPolicy(jadefx::TabPane::TabClosingPolicy::SelectedTab);
        scene->layout(320, 220, 0);

        int closed = 0;
        second->setOnClosed([&] { ++closed; });
        pane->getTabs().removeAt(0);
        Expect(closed == 0, "removing a tab from the list does not fire onClosed");
        Expect(pane->getSelectedTab() == second.get(), "removing the earlier tab keeps the selected page");
        Expect(pane->getTabs().size() == 1, "one tab remains");

        pane->setTabClosingPolicy(jadefx::TabPane::TabClosingPolicy::AllTabs);
        auto third = jadefx::make<jadefx::Tab>("Drafts", jadefx::make<jadefx::Label>("Gamma"));
        int blocked = 0;
        third->setOnCloseRequest([&](jadefx::TabCloseRequest& request) {
            ++blocked;
            request.consume();
        });
        third->setOnClosed([&] { ++closed; });
        pane->getTabs().add(third);
        scene->layout(320, 220, 0);
        jadefx::Node* thirdHeader = FindClass(*scene, "tab", 1);
        Expect(thirdHeader != nullptr, "the added tab grows a header");
        jadefx::Node* thirdClose = nullptr;
        for (jadefx::Node* node : scene->getElementsByClassName("tab-close-button")) {
            if (node != nullptr && node->isVisible() && node->getParent() == thirdHeader) {
                thirdClose = node;
            }
        }
        Expect(thirdClose != nullptr, "the new tab has a close button");
        ClickAt(*scene, thirdClose, thirdClose->getWidth() * 0.5);
        scene->layout(320, 220, 0);
        Expect(blocked == 1 && closed == 0 && pane->getTabs().size() == 2, "consume() keeps the tab");
        third->setOnCloseRequest(nullptr);
        ClickAt(*scene, thirdClose, thirdClose->getWidth() * 0.5);
        scene->layout(320, 220, 0);
        Expect(closed == 1 && pane->getTabs().size() == 1, "a close click removes the tab and fires onClosed");
        Expect(pane->getSelectedTab() == second.get(), "closing another tab leaves the selection in place");
    }
    {
        auto pane = jadefx::make<jadefx::TabPane>();
        pane->setPrefSize(280, 180);
        auto first = jadefx::make<jadefx::Tab>("One", jadefx::make<jadefx::Label>("1"));
        auto second = jadefx::make<jadefx::Tab>("Two", jadefx::make<jadefx::Label>("2"));
        auto third = jadefx::make<jadefx::Tab>("Three", jadefx::make<jadefx::Label>("3"));
        pane->getTabs().add(first);
        pane->getTabs().add(second);
        pane->getTabs().add(third);
        pane->select(2);
        Expect(pane->getSelectedIndex() == 2, "select(index) moves the selection");
        pane->getTabs().removeAt(2);
        Expect(pane->getSelectedTab() == second.get(), "closing the last tab selects the one before it");
        auto inserted = jadefx::make<jadefx::Tab>("Zero", jadefx::make<jadefx::Label>("0"));
        pane->getTabs().insert(0, inserted);
        Expect(pane->getTabs()[0] == inserted, "insert puts the tab at that index");
        Expect(pane->getSelectedTab() == second.get() && pane->getSelectedIndex() == 2,
               "inserting ahead of the selection keeps the same tab");
        auto other = jadefx::make<jadefx::TabPane>();
        other->getTabs().add(second);
        Expect(pane->getTabs().size() == 2, "moving a tab removes it from the previous pane");
        Expect(second->getTabPane() == other.get() && other->getSelectedTab() == second.get(),
               "the moved tab is selected when the destination is empty");
        Expect(pane->getSelectedTab() == first.get() || pane->getSelectedTab() == inserted.get(),
               "the old pane selects a tab that is still there");
    }
    {
        auto pane = jadefx::make<jadefx::TabPane>();
        pane->setPrefSize(300, 180);
        auto enabled = jadefx::make<jadefx::Tab>("Open", jadefx::make<jadefx::Label>("open"));
        auto disabled = jadefx::make<jadefx::Tab>("Locked", jadefx::make<jadefx::Label>("locked"));
        disabled->setDisable(true);
        pane->getTabs().add(enabled);
        pane->getTabs().add(disabled);
        auto scene = jadefx::make<jadefx::Scene>(pane, 300, 180);
        scene->layout(300, 180, 0);
        jadefx::Node* locked = FindClass(*scene, "tab", 1);
        Expect(locked != nullptr && Near(locked->computedStyle().opacity, 0.45f, 0.02f),
               "a disabled header fades");
        ClickAt(*scene, locked, 8);
        scene->layout(300, 180, 0);
        Expect(pane->getSelectedTab() == enabled.get(), "a disabled header ignores clicks");
        pane->select(disabled);
        Expect(disabled->isSelected(), "select() can show a disabled tab");
        pane->setDisable(true);
        scene->layout(300, 180, 0);
        Expect(disabled->isDisabled() && enabled->isDisabled(), "disabling the pane disables its tabs");
        ClickAt(*scene, FindClass(*scene, "tab", 0), 8);
        Expect(pane->getSelectedTab() == disabled.get(), "a disabled pane ignores header clicks");
    }
    {
        const std::string mark = "\u2026";
        auto pane = jadefx::make<jadefx::TabPane>();
        pane->setPrefSize(150, 160);
        pane->getTabs().add(jadefx::make<jadefx::Tab>("DocumentationSettingsPanel", jadefx::make<jadefx::Label>("Body")));
        auto scene = jadefx::make<jadefx::Scene>(pane, 150, 160);
        scene->layout(150, 160, 0);
        jadefx::Node* title = FindClass(*scene, "tab-label", 0);
        auto* label = dynamic_cast<jadefx::Label*>(title);
        Expect(label != nullptr, "the header title is a label");
        if (label != nullptr) {
            Expect(EndsWith(label->displayedText(), mark), "a narrow tab ends its title with an ellipsis");
            Expect(label->displayedText() != label->getText(), "the ellipsis replaces part of the title");
        }
    }
    {
        auto page = jadefx::make<jadefx::Label>("Moved");
        auto graphic = jadefx::make<jadefx::Pane>();
        graphic->setPrefSize(12, 12);
        auto tab = jadefx::make<jadefx::Tab>("With icon", page);
        tab->setGraphic(graphic);
        auto pane = jadefx::make<jadefx::TabPane>();
        pane->setPrefSize(280, 180);
        pane->getTabs().add(tab);
        auto scene = jadefx::make<jadefx::Scene>(pane, 280, 180);
        scene->layout(280, 180, 0);
        Expect(graphic->getParent() != nullptr && graphic->getParent()->getParent() == pane.get(),
               "the graphic is placed in the header");
        Expect(graphic->getWidth() > 8, "the graphic keeps its size");
        auto holder = jadefx::make<jadefx::Pane>();
        holder->getChildren().add(page);
        Expect(tab->getContent() == nullptr && page->getParent() == holder.get(),
               "moving the page out of the tab clears the content slot");
        holder->getChildren().add(graphic);
        Expect(tab->getGraphic() == nullptr && graphic->getParent() == holder.get(),
               "moving the graphic out of the header clears that slot");
    }
    {
        auto page = jadefx::make<jadefx::Label>("Side");
        auto tab = jadefx::make<jadefx::Tab>("Edge", page);
        auto pane = jadefx::make<jadefx::TabPane>();
        pane->setPrefSize(300, 200);
        pane->getTabs().add(tab);
        auto scene = jadefx::make<jadefx::Scene>(pane, 300, 200);
        pane->setSide(jadefx::Side::Bottom);
        scene->layout(300, 200, 0);
        jadefx::Node* header = FindClass(*scene, "tab", 0);
        Expect(header != nullptr && page->getY() + page->getHeight() <= header->getY() + 0.75,
               "a bottom strip sits under the page");
        pane->setSide(jadefx::Side::Left);
        scene->layout(300, 200, 0);
        header = FindClass(*scene, "tab", 0);
        Expect(header != nullptr && Near(page->getX(), header->getX() + header->getWidth()),
               "a left strip sits beside the page");
        pane->setSide(jadefx::Side::Right);
        scene->layout(300, 200, 0);
        header = FindClass(*scene, "tab", 0);
        Expect(header != nullptr && page->getX() + page->getWidth() <= header->getX() + 0.75,
               "a right strip sits on the far side of the page");
    }
    {
        auto pane = jadefx::make<jadefx::TabPane>();
        pane->setPrefSize(240, 160);
        auto first = jadefx::make<jadefx::Tab>("Plain", jadefx::make<jadefx::Label>("plain"));
        auto second = jadefx::make<jadefx::Tab>("Marked", jadefx::make<jadefx::Label>("marked"));
        pane->getTabs().add(first);
        pane->getTabs().add(second);
        auto scene = jadefx::make<jadefx::Scene>(pane, 240, 160);
        scene->setStylesheet("tab { background-color: #ddeeff; } tab:selected { background-color: #112233; }");
        scene->layout(240, 160, 0);
        jadefx::Node* selected = FindClass(*scene, "tab", 0);
        jadefx::Node* idle = FindClass(*scene, "tab", 1);
        const jadefx::Color selectedColor = jadefx::Color::parse("#112233");
        const jadefx::Color idleColor = jadefx::Color::parse("#ddeeff");
        Expect(selected != nullptr && idle != nullptr, "styled headers are in the scene");
        if (selected != nullptr && idle != nullptr) {
            const jadefx::Color& have = selected->computedStyle().background.color;
            const jadefx::Color& rest = idle->computedStyle().background.color;
            Expect(Near(have.r, selectedColor.r, 0.02) && Near(have.g, selectedColor.g, 0.02) &&
                       Near(have.b, selectedColor.b, 0.02),
                   "tab:selected paints the selected header");
            Expect(Near(rest.r, idleColor.r, 0.02) && Near(rest.g, idleColor.g, 0.02) &&
                       Near(rest.b, idleColor.b, 0.02),
                   "tab paints the other headers");
        }
    }
    {
        auto pane = jadefx::make<jadefx::TabPane>();
        Expect(pane->getSelectedTab() == nullptr && pane->getSelectedIndex() == jadefx::TabPane::npos,
               "an empty pane has no selection");
        auto only = jadefx::make<jadefx::Tab>("Only", jadefx::make<jadefx::Label>("only"));
        int closed = 0;
        only->setOnClosed([&] { ++closed; });
        pane->getTabs().add(only);
        pane->setPrefSize(200, 140);
        auto scene = jadefx::make<jadefx::Scene>(pane, 200, 140);
        pane->setTabClosingPolicy(jadefx::TabPane::TabClosingPolicy::AllTabs);
        scene->layout(200, 140, 0);
        jadefx::Node* header = FindClass(*scene, "tab", 0);
        jadefx::Node* close = nullptr;
        for (jadefx::Node* node : scene->getElementsByClassName("tab-close-button")) {
            if (node != nullptr && node->isVisible()) {
                close = node;
            }
        }
        Expect(close != nullptr && header != nullptr, "the only tab can be closed");
        if (close != nullptr) {
            ClickAt(*scene, close, close->getWidth() * 0.5);
        }
        scene->layout(200, 140, 0);
        Expect(closed == 1 && pane->getTabs().empty() && pane->getSelectedTab() == nullptr,
               "closing the last tab clears the selection");
        Expect(only->getContent() != nullptr && only->getContent()->getParent() == nullptr,
               "the closed page is kept by the tab and leaves the scene");
    }
    {
        auto pane = jadefx::make<jadefx::TabPane>();
        pane->setPrefSize(420, 200);
        pane->setTabClosingPolicy(jadefx::TabPane::TabClosingPolicy::AllTabs);
        auto alphaPage = jadefx::make<jadefx::Label>("A");
        auto alpha = jadefx::make<jadefx::Tab>("Alpha", alphaPage);
        auto beta = jadefx::make<jadefx::Tab>("Beta", jadefx::make<jadefx::Label>("B"));
        auto gamma = jadefx::make<jadefx::Tab>("Gamma", jadefx::make<jadefx::Label>("C"));
        beta->setClosable(false);
        int alphaClosed = 0;
        int betaClosed = 0;
        int gammaClosed = 0;
        alpha->setOnClosed([&] { ++alphaClosed; });
        beta->setOnClosed([&] { ++betaClosed; });
        gamma->setOnClosed([&] { ++gammaClosed; });
        pane->getTabs().add(alpha);
        pane->getTabs().add(beta);
        pane->getTabs().add(gamma);
        auto scene = jadefx::make<jadefx::Scene>(pane, 420, 200);
        scene->layout(420, 200, 0);

        RightClick(*scene, alphaPage.get());
        Expect(scene->getElementById("Close") == nullptr, "a right-click on the page is not a tab menu");

        RightClick(*scene, FindClass(*scene, "tab", 2));
        scene->layout(420, 200, 0);
        Expect(pane->getSelectedTab() == gamma.get(), "a right-click selects that tab");
        jadefx::Node* closeItem = scene->getElementById("Close");
        jadefx::Node* othersItem = scene->getElementById("Close Others");
        jadefx::Node* rightItem = scene->getElementById("Close to the Right");
        Expect(closeItem != nullptr && othersItem != nullptr && rightItem != nullptr,
               "the tab menu lists Close, Close Others, and Close to the Right");
        Expect(closeItem != nullptr && !closeItem->isDisabled(), "Close is enabled on a closable tab");
        Expect(othersItem != nullptr && !othersItem->isDisabled(),
               "Close Others is enabled when another tab can close");
        Expect(rightItem != nullptr && rightItem->isDisabled(), "Close to the Right is disabled on the last tab");
        ChooseMenu(*scene, "Close to the Right");
        Expect(pane->getTabs().size() == 3, "a disabled Close to the Right leaves the tabs");

        RightClick(*scene, FindClass(*scene, "tab", 2));
        ChooseMenu(*scene, "Close Others");
        Expect(alphaClosed == 1 && betaClosed == 0 && gammaClosed == 0,
               "Close Others closes the other closable tabs and fires onClosed");
        Expect(pane->getTabs().size() == 2 && pane->getTabs()[0] == beta && pane->getTabs()[1] == gamma,
               "Close Others keeps the clicked tab and a tab that is not closable");
        Expect(pane->getSelectedTab() == gamma.get(), "Close Others leaves the clicked tab selected");

        pane->getTabs().insert(0, alpha);
        scene->layout(420, 200, 0);
        RightClick(*scene, FindClass(*scene, "tab", 0));
        scene->layout(420, 200, 0);
        Expect(pane->getSelectedTab() == alpha.get(), "a right-click moves the selection to that tab");
        rightItem = scene->getElementById("Close to the Right");
        Expect(rightItem != nullptr && !rightItem->isDisabled(),
               "Close to the Right is enabled when a later tab can close");
        ChooseMenu(*scene, "Close to the Right");
        Expect(gammaClosed == 1 && betaClosed == 0 && pane->getTabs().size() == 2,
               "Close to the Right closes later closable tabs");
        Expect(pane->getTabs()[0] == alpha && pane->getTabs()[1] == beta,
               "Close to the Right keeps the clicked tab and later tabs that are not closable");

        RightClick(*scene, FindClass(*scene, "tab", 1));
        scene->layout(420, 200, 0);
        Expect(pane->getSelectedTab() == beta.get(), "a right-click selects a tab that is not closable");
        closeItem = scene->getElementById("Close");
        othersItem = scene->getElementById("Close Others");
        rightItem = scene->getElementById("Close to the Right");
        Expect(closeItem != nullptr && closeItem->isDisabled(), "Close is disabled when the tab is not closable");
        Expect(othersItem != nullptr && !othersItem->isDisabled(),
               "Close Others stays enabled from a tab that is not closable");
        Expect(rightItem != nullptr && rightItem->isDisabled(),
               "Close to the Right is disabled when nothing later can close");
        ChooseMenu(*scene, "Close");
        Expect(pane->getTabs().size() == 2 && betaClosed == 0, "Close does not remove a tab that is not closable");

        RightClick(*scene, FindClass(*scene, "tab", 1));
        ChooseMenu(*scene, "Close Others");
        Expect(alphaClosed == 2 && pane->getTabs().size() == 1 && pane->getSelectedTab() == beta.get(),
               "Close Others from a fixed tab closes the closable ones");

        auto delta = jadefx::make<jadefx::Tab>("Delta", jadefx::make<jadefx::Label>("D"));
        int blocked = 0;
        int deltaClosed = 0;
        delta->setOnCloseRequest([&](jadefx::TabCloseRequest& request) {
            ++blocked;
            request.consume();
        });
        delta->setOnClosed([&] { ++deltaClosed; });
        pane->getTabs().add(delta);
        scene->layout(420, 200, 0);
        RightClick(*scene, FindClass(*scene, "tab", 1));
        ChooseMenu(*scene, "Close");
        Expect(blocked == 1 && deltaClosed == 0 && pane->getTabs().size() == 2,
               "Close honors a consumed close request");

        delta->setOnCloseRequest(nullptr);
        RightClick(*scene, FindClass(*scene, "tab", 1));
        ChooseMenu(*scene, "Close");
        Expect(deltaClosed == 1 && pane->getTabs().size() == 1 && pane->getSelectedTab() == beta.get(),
               "Close removes the tab and fires onClosed");

        pane->getTabs().add(delta);
        scene->layout(420, 200, 0);
        jadefx::Node* deltaHeader = FindClass(*scene, "tab", 1);
        jadefx::Node* deltaClose = nullptr;
        for (jadefx::Node* node : scene->getElementsByClassName("tab-close-button")) {
            if (node != nullptr && node->isVisible() && node->getParent() == deltaHeader) {
                deltaClose = node;
            }
        }
        Expect(deltaClose != nullptr, "the reopened tab shows a close button");
        RightClick(*scene, deltaClose);
        scene->layout(420, 200, 0);
        Expect(pane->getTabs().size() == 2 && deltaClosed == 1,
               "a right-click on the close button does not close the tab");
        Expect(scene->getElementById("Close") != nullptr, "a right-click on the close button opens the tab menu");
    }
    {
        auto pane = jadefx::make<jadefx::TabPane>();
        pane->setPrefSize(360, 180);
        auto open = jadefx::make<jadefx::Tab>("Open", jadefx::make<jadefx::Label>("open"));
        auto locked = jadefx::make<jadefx::Tab>("Locked", jadefx::make<jadefx::Label>("locked"));
        locked->setDisable(true);
        pane->setTabClosingPolicy(jadefx::TabPane::TabClosingPolicy::AllTabs);
        pane->getTabs().add(open);
        pane->getTabs().add(locked);
        auto scene = jadefx::make<jadefx::Scene>(pane, 360, 180);
        scene->layout(360, 180, 0);
        RightClick(*scene, FindClass(*scene, "tab", 1));
        scene->layout(360, 180, 0);
        Expect(pane->getSelectedTab() == open.get(), "a right-click on a disabled tab does not select it");
        jadefx::Node* closeItem = scene->getElementById("Close");
        Expect(closeItem != nullptr && closeItem->isDisabled(), "Close is disabled on a disabled tab");
    }
    {
        auto pane = jadefx::make<jadefx::TabPane>();
        pane->setPrefSize(360, 180);
        auto first = jadefx::make<jadefx::Tab>("One", jadefx::make<jadefx::Label>("1"));
        auto second = jadefx::make<jadefx::Tab>("Two", jadefx::make<jadefx::Label>("2"));
        auto third = jadefx::make<jadefx::Tab>("Three", jadefx::make<jadefx::Label>("3"));
        int closed = 0;
        third->setOnClosed([&] { ++closed; });
        pane->getTabs().add(first);
        pane->getTabs().add(second);
        pane->getTabs().add(third);
        auto scene = jadefx::make<jadefx::Scene>(pane, 360, 180);
        scene->layout(360, 180, 0);
        Expect(CountVisible(*scene, "tab-close-button") == 1, "only the selected tab shows a close button");
        RightClick(*scene, FindClass(*scene, "tab", 1));
        ChooseMenu(*scene, "Close to the Right");
        Expect(closed == 1 && pane->getTabs().size() == 2 && pane->getTabs()[0] == first && pane->getTabs()[1] == second,
               "Close to the Right closes a later tab whose close button is hidden");
        Expect(pane->getSelectedTab() == second.get(), "Close to the Right leaves the clicked tab selected");
    }
    {
        auto pane = jadefx::make<jadefx::TabPane>();
        pane->setPrefSize(300, 160);
        pane->setTabClosingPolicy(jadefx::TabPane::TabClosingPolicy::Unavailable);
        auto only = jadefx::make<jadefx::Tab>("Only", jadefx::make<jadefx::Label>("only"));
        int closed = 0;
        only->setOnClosed([&] { ++closed; });
        pane->getTabs().add(only);
        auto scene = jadefx::make<jadefx::Scene>(pane, 300, 160);
        scene->layout(300, 160, 0);
        RightClick(*scene, FindClass(*scene, "tab", 0));
        scene->layout(300, 160, 0);
        jadefx::Node* closeItem = scene->getElementById("Close");
        jadefx::Node* othersItem = scene->getElementById("Close Others");
        jadefx::Node* rightItem = scene->getElementById("Close to the Right");
        Expect(closeItem != nullptr && closeItem->isDisabled(), "Unavailable disables Close");
        Expect(othersItem != nullptr && othersItem->isDisabled(), "Unavailable disables Close Others");
        Expect(rightItem != nullptr && rightItem->isDisabled(), "Unavailable disables Close to the Right");
        ChooseMenu(*scene, "Close");
        Expect(closed == 0 && pane->getTabs().size() == 1, "Unavailable keeps the tab");
    }
}

std::string CellText(jadefx::Node* cell) {
    if (cell == nullptr) {
        return {};
    }
    for (jadefx::Node* node : cell->getElementsByClassName("tree-cell-label")) {
        if (auto* label = dynamic_cast<jadefx::Label*>(node)) {
            return label->getText();
        }
    }
    return {};
}

jadefx::Node* CellNamed(jadefx::Node& root, const std::string& text) {
    for (jadefx::Node* cell : root.getElementsByClassName("tree-cell")) {
        if (CellText(cell) == text) {
            return cell;
        }
    }
    return nullptr;
}

std::vector<jadefx::Node*> ShownCells(jadefx::Node& root) {
    std::vector<jadefx::Node*> cells;
    for (jadefx::Node* cell : root.getElementsByClassName("tree-cell")) {
        if (cell != nullptr && cell->isVisible() && cell->getHeight() > 1) {
            cells.push_back(cell);
        }
    }
    std::sort(cells.begin(), cells.end(), [](jadefx::Node* a, jadefx::Node* b) { return a->getY() < b->getY(); });
    return cells;
}

jadefx::Node* ArrowOf(jadefx::Node* cell) {
    if (cell == nullptr) {
        return nullptr;
    }
    for (jadefx::Node* node : cell->getElementsByClassName("tree-disclosure-node")) {
        if (node != nullptr && node->isVisible() && node->getWidth() > 1) {
            return node;
        }
    }
    return nullptr;
}

void TestTreeView() {
    auto root = jadefx::make<jadefx::TreeItem>("Company");
    auto sales = jadefx::make<jadefx::TreeItem>("Sales");
    auto carol = jadefx::make<jadefx::TreeItem>("Carol");
    auto dana = jadefx::make<jadefx::TreeItem>("Dana");
    root->getChildren().add(sales);
    sales->getChildren().add(carol);
    sales->getChildren().add(dana);
    Expect(carol->getParent() == sales.get() && sales->getParent() == root.get(), "a child remembers its parent");
    Expect(carol->nextSibling() == dana.get() && dana->previousSibling() == carol.get(), "siblings keep list order");
    Expect(carol->previousSibling() == nullptr && dana->nextSibling() == nullptr, "end siblings have no neighbor");
    Expect(sales->isLeaf() == false && carol->isLeaf(), "a branch has children and a leaf does not");

    sales->getChildren().add(carol);
    Expect(sales->getChildren().size() == 2, "the same child is not inserted twice");
    carol->getChildren().add(root);
    Expect(carol->getChildren().empty() && root->getParent() == nullptr, "a child cannot contain its ancestor");
    carol->getChildren().add(carol);
    Expect(carol->getChildren().empty(), "an item cannot contain itself");

    int leafOpens = 0;
    carol->setOnExpanded([&](jadefx::TreeItem&) { ++leafOpens; });
    carol->setExpanded(true);
    Expect(leafOpens == 0 && carol->isExpanded(), "expanding a leaf keeps the flag and does not notify");
    int opens = 0;
    int closes = 0;
    sales->setOnExpanded([&](jadefx::TreeItem&) { ++opens; });
    sales->setOnCollapsed([&](jadefx::TreeItem&) { ++closes; });
    root->setExpanded(true);
    Expect(opens == 0, "a collapsed branch stays closed when its parent opens");
    sales->setExpanded(true);
    Expect(opens == 1 && closes == 0, "opening a branch notifies it once");
    sales->setExpanded(true);
    Expect(opens == 1, "setting the same expanded state does not notify again");

    auto tree = jadefx::make<jadefx::TreeView>(root);
    tree->setPrefSize(240, 200);
    Expect(tree->getElementType() != nullptr && std::string(tree->getElementType()) == "treeview",
           "the control type is treeview");
    Expect(tree->getExpandedItemCount() == 4, "an expanded branch counts itself and its children");
    Expect(tree->getTreeItem(0) == root.get() && tree->getTreeItem(2) == carol.get(), "rows follow expanded order");
    Expect(tree->getRow(dana.get()) == 3 && tree->getRow(nullptr) == -1, "getRow finds a visible item");
    Expect(tree->getFixedCellSize() == 32 && Near(tree->getIndent(), 10), "row height is 32 and indent is 10");

    sales->setExpanded(false);
    Expect(tree->getExpandedItemCount() == 2 && tree->getRow(carol.get()) == -1, "collapsing hides the children");
    sales->setExpanded(true);
    Expect(closes == 1 && opens == 2, "collapsing and opening again both notify");

    int changes = 0;
    jadefx::TreeItem* last = nullptr;
    tree->setOnSelectionChanged([&](jadefx::TreeItem* item) {
        ++changes;
        last = item;
    });
    tree->select(carol.get());
    Expect(changes == 1 && last == carol.get() && tree->getSelectedItem() == carol.get(), "select notifies once");
    tree->select(carol.get());
    Expect(changes == 1, "selecting the current row does not notify again");
    Expect(tree->getSelectedIndex() == 2, "the selected index matches the visible row");

    auto scene = jadefx::make<jadefx::Scene>(tree, 240, 200);
    scene->layout(240, 200, 0);
    jadefx::Node* rootCell = CellNamed(*scene, "Company");
    jadefx::Node* salesCell = CellNamed(*scene, "Sales");
    jadefx::Node* carolCell = CellNamed(*scene, "Carol");
    jadefx::Node* danaCell = CellNamed(*scene, "Dana");
    Expect(rootCell && salesCell && carolCell && danaCell, "each visible item has a row");
    if (rootCell && salesCell && carolCell) {
        Expect(Near(salesCell->getY(), rootCell->getY() + tree->getFixedCellSize()), "rows stack by the cell size");
        jadefx::Node* rootLabel = nullptr;
        jadefx::Node* salesLabel = nullptr;
        jadefx::Node* carolLabel = nullptr;
        for (jadefx::Node* node : rootCell->getElementsByClassName("tree-cell-label")) {
            rootLabel = node;
        }
        for (jadefx::Node* node : salesCell->getElementsByClassName("tree-cell-label")) {
            salesLabel = node;
        }
        for (jadefx::Node* node : carolCell->getElementsByClassName("tree-cell-label")) {
            carolLabel = node;
        }
        Expect(rootLabel && salesLabel && carolLabel, "each row has a label");
        if (rootLabel && salesLabel && carolLabel) {
            Expect(Near(salesLabel->getAbsoluteX(), rootLabel->getAbsoluteX() + tree->getIndent()),
                   "a child indents one level past its parent");
            Expect(Near(carolLabel->getAbsoluteX(), salesLabel->getAbsoluteX() + tree->getIndent()),
                   "a grandchild indents one more level");
        }
        Expect(ArrowOf(rootCell) != nullptr && ArrowOf(salesCell) != nullptr, "branches draw a disclosure arrow");
        Expect(ArrowOf(carolCell) == nullptr && ArrowOf(danaCell) == nullptr, "a leaf draws no disclosure arrow");
        bool salesExpanded = false;
        for (const std::string& name : salesCell->getClassList().items()) {
            if (name == "expanded") {
                salesExpanded = true;
            }
        }
        Expect(salesExpanded, "an open branch carries the expanded class");
    }
    Expect(CountVisible(*scene, "tree-disclosure-node") == 2, "only branches show an arrow");

    jadefx::Node* bar = nullptr;
    for (jadefx::Node* node : scene->getElementsByClassName("selection-bar")) {
        if (node != nullptr && node->isVisible() && node->getWidth() > 1) {
            bar = node;
        }
    }
    Expect(bar != nullptr && carolCell != nullptr && bar->getParent() == carolCell, "the selection bar sits on the selected row");
    if (bar != nullptr) {
        Expect(Near(bar->getX(), 0) && Near(bar->getWidth(), 3) && Near(bar->getHeight(), tree->getFixedCellSize()),
               "the selection bar is a 3px strip along the row");
        const jadefx::Color& ink = bar->computedStyle().background.color;
        const jadefx::Color accent = jadefx::Color::rgb8(26, 115, 232);
        Expect(Near(ink.r, accent.r, 0.02) && Near(ink.g, accent.g, 0.02) && Near(ink.b, accent.b, 0.02),
               "the selection bar starts in the accent blue");
    }

    if (salesCell != nullptr) {
        sales->setExpanded(false);
        scene->layout(240, 200, 0);
        salesCell = CellNamed(*scene, "Sales");
        jadefx::Node* arrow = ArrowOf(salesCell);
        Expect(arrow != nullptr, "a collapsed branch keeps its arrow");
        jadefx::TreeItem* before = tree->getSelectedItem();
        Expect(before != nullptr && before != sales.get(), "another row is selected before the arrow click");
        if (arrow != nullptr) {
            ClickAt(*scene, arrow, arrow->getWidth() * 0.5);
        }
        scene->layout(240, 200, 0);
        Expect(sales->isExpanded(), "clicking the arrow opens the branch");
        Expect(tree->getSelectedItem() == before, "clicking the arrow leaves the selection alone");
        Expect(tree->getExpandedItemCount() == 4, "the opened children come back");
    }

    sales->setExpanded(false);
    scene->layout(240, 200, 0);
    salesCell = CellNamed(*scene, "Sales");
    if (salesCell != nullptr) {
        ClickAt(*scene, salesCell, salesCell->getWidth() * 0.5);
        Expect(!sales->isExpanded() && tree->getSelectedItem() == sales.get(), "one click selects a branch and leaves it closed");
        ClickAt(*scene, salesCell, salesCell->getWidth() * 0.5);
        scene->layout(240, 200, 0);
        Expect(sales->isExpanded(), "a second click opens the branch");
    }

    scene->setStylesheet("tree-cell:selected { background-color: #112233; } .selection-bar { background-color: #00aa00; }");
    tree->select(carol.get());
    scene->layout(240, 200, 0);
    carolCell = CellNamed(*scene, "Carol");
    bar = nullptr;
    for (jadefx::Node* node : scene->getElementsByClassName("selection-bar")) {
        if (node != nullptr && node->isVisible() && node->getWidth() > 1) {
            bar = node;
        }
    }
    if (carolCell != nullptr && bar != nullptr) {
        const jadefx::Color want = jadefx::Color::parse("#112233");
        const jadefx::Color& have = carolCell->computedStyle().background.color;
        const jadefx::Color& stripe = bar->computedStyle().background.color;
        Expect(Near(have.r, want.r, 0.02) && Near(have.g, want.g, 0.02) && Near(have.b, want.b, 0.02),
               "tree-cell:selected paints the selected row");
        Expect(stripe.g > 0.5f && stripe.r < 0.1f, "selection-bar color comes from the stylesheet");
    }

    scene->noteKey(jadefx::Key::Down, true, false, 0);
    Expect(tree->getSelectedItem() == dana.get(), "Down moves the selection to the next row");
    scene->noteKey(jadefx::Key::Left, true, false, 0);
    Expect(tree->getSelectedItem() == sales.get(), "Left on a leaf selects the parent");
    scene->noteKey(jadefx::Key::Left, true, false, 0);
    Expect(!sales->isExpanded(), "Left on an open branch closes it");
    scene->noteKey(jadefx::Key::Right, true, false, 0);
    Expect(sales->isExpanded() && tree->getSelectedItem() == sales.get(), "Right on a closed branch opens it");
    scene->noteKey(jadefx::Key::Right, true, false, 0);
    Expect(tree->getSelectedItem() == carol.get(), "Right on an open branch selects its first child");

    tree->setShowRoot(false);
    Expect(tree->getExpandedItemCount() == 3 && tree->getTreeItem(0) == sales.get(),
           "hiding the root lists its children");
    scene->layout(240, 200, 0);
    salesCell = CellNamed(*scene, "Sales");
    carolCell = CellNamed(*scene, "Carol");
    if (salesCell && carolCell) {
        jadefx::Node* salesLabel = nullptr;
        jadefx::Node* carolLabel = nullptr;
        for (jadefx::Node* node : salesCell->getElementsByClassName("tree-cell-label")) {
            salesLabel = node;
        }
        for (jadefx::Node* node : carolCell->getElementsByClassName("tree-cell-label")) {
            carolLabel = node;
        }
        if (salesLabel && carolLabel) {
            Expect(Near(carolLabel->getAbsoluteX(), salesLabel->getAbsoluteX() + tree->getIndent()),
                   "children of a hidden root still indent one level");
        }
    }
    root->setExpanded(false);
    Expect(tree->getExpandedItemCount() == 0, "a hidden root that is collapsed shows nothing");
    root->setExpanded(true);
    sales->setExpanded(true);
    tree->setShowRoot(true);

    auto icon = jadefx::make<jadefx::Label>("*");
    icon->setElementId("star");
    carol->setGraphic(icon);
    scene->layout(240, 200, 0);
    Expect(scene->getElementById("star") == icon.get(), "a graphic is placed in the row");
    carolCell = CellNamed(*scene, "Carol");
    if (carolCell != nullptr && icon->getWidth() > 0) {
        jadefx::Node* carolLabel = nullptr;
        for (jadefx::Node* node : carolCell->getElementsByClassName("tree-cell-label")) {
            carolLabel = node;
        }
        if (carolLabel != nullptr) {
            Expect(icon->getAbsoluteX() + icon->getWidth() <= carolLabel->getAbsoluteX() + 0.5,
                   "the graphic sits to the left of the label");
        }
    }

    tree->select(dana.get());
    auto outside = jadefx::make<jadefx::TreeItem>("Outside");
    outside->getChildren().add(dana);
    Expect(dana->getParent() == outside.get(), "moving an item detaches it from the old parent");
    Expect(sales->getChildren().size() == 1, "the old parent loses the moved child");
    Expect(tree->getSelectedItem() == nullptr && last == nullptr, "removing the selected item clears the selection");

    sales->getChildren().add(dana);
    sales->setExpanded(true);
    root->setExpanded(true);
    tree->setPrefSize(240, 80);
    scene->layout(240, 80, 0);
    std::vector<jadefx::Node*> shown = ShownCells(*scene);
    Expect(shown.size() == 3 && CellText(shown[0]) == "Company" && CellText(shown[1]) == "Sales" &&
               CellText(shown[2]) == "Carol",
           "a short view shows every row that meets the window");
    jadefx::Node* company = shown.empty() ? nullptr : shown[0];
    if (company != nullptr) {
        scene->noteScroll(company->getAbsoluteX() + 8, company->getAbsoluteY() + 8, 0, -0.25);
    }
    scene->layout(240, 80, 0);
    company = CellNamed(*scene, "Company");
    Expect(company != nullptr && Near(company->getY(), -8), "a small trackpad scroll moves the rows");
    tree->scrollTo(0);
    scene->layout(240, 80, 0);
    shown = ShownCells(*scene);
    if (!shown.empty()) {
        scene->noteScroll(shown[0]->getAbsoluteX() + 8, shown[0]->getAbsoluteY() + 8, 0, -1);
    }
    scene->layout(240, 80, 0);
    shown = ShownCells(*scene);
    Expect(shown.size() == 3 && CellText(shown[0]) == "Sales" && CellText(shown.back()) == "Dana",
           "a wheel notch scrolls by one row");
    tree->scrollTo(carol.get());
    scene->layout(240, 80, 0);
    shown = ShownCells(*scene);
    bool sawCarol = false;
    for (jadefx::Node* cell : shown) {
        if (CellText(cell) == "Carol") {
            sawCarol = true;
        }
    }
    Expect(sawCarol && !shown.empty() && CellText(shown.back()) == "Dana",
           "scrollTo brings an item into view and stops at the last row");

    tree->scrollTo(0);
    scene->layout(240, 80, 0);
    jadefx::Node* scrollBar = nullptr;
    for (jadefx::Node* node : scene->getElementsByClassName("scroll-bar")) {
        if (node != nullptr && node->isVisible() && node->getHeight() > 1) {
            scrollBar = node;
        }
    }
    Expect(scrollBar != nullptr && Near(scrollBar->getWidth(), 10, 1.5), "the scrollbar uses the shared thumb width");
    if (scrollBar != nullptr) {
        const double x = scrollBar->getAbsoluteX() + scrollBar->getWidth() * 0.5;
        const double thumbY = scrollBar->getAbsoluteY() + 10;
        scene->noteButton(0, true, x, thumbY);
        scene->noteMove(x, scrollBar->getAbsoluteY() + 30);
        scene->noteButton(0, false, x, scrollBar->getAbsoluteY() + 30);
        scene->layout(240, 80, 0);
        jadefx::Node* sales = CellNamed(*scene, "Sales");
        Expect(sales != nullptr && Near(sales->getY(), 0), "dragging the thumb scrolls with the pointer");
        Expect(tree->getSelectedItem() == nullptr, "dragging the scrollbar does not select a row");

        tree->scrollTo(0);
        scene->layout(240, 80, 0);
        scrollBar = nullptr;
        for (jadefx::Node* node : scene->getElementsByClassName("scroll-bar")) {
            if (node != nullptr && node->isVisible()) {
                scrollBar = node;
            }
        }
        if (scrollBar != nullptr) {
            const double trackX = scrollBar->getAbsoluteX() + scrollBar->getWidth() * 0.5;
            scene->noteButton(0, true, trackX, scrollBar->getAbsoluteY() + scrollBar->getHeight() - 4);
            scene->noteButton(0, false, trackX, scrollBar->getAbsoluteY() + scrollBar->getHeight() - 4);
        }
        scene->layout(240, 80, 0);
        jadefx::Node* dana = CellNamed(*scene, "Dana");
        Expect(dana != nullptr && dana->isVisible() && Near(dana->getY(), 48), "clicking the track pages by one view");
    }

    tree->setPrefSize(240, 200);
    tree->scrollTo(0);
    sales->setExpanded(false);
    scene->layout(240, 200, 0);
    int activations = 0;
    tree->setOnItemActivated([&](jadefx::TreeItem& item) {
        ++activations;
        return &item == sales.get();
    });
    salesCell = CellNamed(*scene, "Sales");
    if (salesCell != nullptr) {
        ClickAt(*scene, salesCell, salesCell->getWidth() * 0.5);
        ClickAt(*scene, salesCell, salesCell->getWidth() * 0.5);
    }
    Expect(activations == 1 && !sales->isExpanded(), "a handled double-click does not open the branch");
    tree->setOnItemActivated(nullptr);
    if (salesCell != nullptr) {
        ClickAt(*scene, salesCell, salesCell->getWidth() * 0.5);
        ClickAt(*scene, salesCell, salesCell->getWidth() * 0.5);
        scene->layout(240, 200, 0);
    }
    Expect(sales->isExpanded(), "a double-click with no handler opens the branch");

    int contexts = 0;
    jadefx::TreeItem* contextItem = nullptr;
    tree->setOnContextMenuRequested([&](jadefx::TreeItem& item, const jadefx::MouseEvent&) {
        ++contexts;
        contextItem = &item;
    });
    carolCell = CellNamed(*scene, "Carol");
    if (carolCell != nullptr) {
        const double x = carolCell->getAbsoluteX() + carolCell->getWidth() * 0.5;
        const double y = carolCell->getAbsoluteY() + carolCell->getHeight() * 0.5;
        scene->noteButton(1, true, x, y);
        scene->noteButton(1, false, x, y);
    }
    Expect(contexts == 1 && contextItem == carol.get(), "a right-click asks for a context menu on that row");
    Expect(tree->getSelectedItem() == carol.get(), "a right-click selects the row");

    sales->setExpanded(true);
    root->setExpanded(true);
    tree->setShowRoot(true);
    tree->scrollTo(0);
    tree->setPrefSize(240, 200);
    auto plus = jadefx::make<jadefx::Label>("+");
    plus->setAlignment(jadefx::Pos::Center);
    plus->setPrefSize(16, 16);
    plus->getClassList().add("row-plus");
    int plusClicks = 0;
    plus->setOnMouseClicked([&](const jadefx::MouseEvent&) { ++plusClicks; });
    scene->notePointerExit();
    tree->setHoverAccessory(plus);
    scene->layout(240, 200, 0);
    Expect(!plus->isVisible(), "the plus is hidden until a row is hovered");
    salesCell = CellNamed(*scene, "Sales");
    if (salesCell != nullptr) {
        const bool wasOpen = sales->isExpanded();
        scene->noteMove(salesCell->getAbsoluteX() + 12, salesCell->getAbsoluteY() + salesCell->getHeight() * 0.5);
        scene->layout(240, 200, 0);
        salesCell = CellNamed(*scene, "Sales");
        Expect(plus->isVisible() && plus->getWidth() > 8, "hovering a row shows the plus");
        Expect(tree->getHoveredItem() == sales.get(), "the hovered row is Sales");
        if (salesCell != nullptr) {
            Expect(plus->getAbsoluteX() > salesCell->getAbsoluteX() + salesCell->getWidth() * 0.5,
                   "the plus sits on the right of the row");
            Expect(plus->getAbsoluteX() + plus->getWidth() <= salesCell->getAbsoluteX() + salesCell->getWidth() + 1,
                   "the plus stays inside the row");
            Expect(Near(plus->getAbsoluteY() + plus->getHeight() * 0.5,
                        salesCell->getAbsoluteY() + salesCell->getHeight() * 0.5, 2),
                   "the plus is centered on the row");
            jadefx::Node* salesLabel = nullptr;
            for (jadefx::Node* node : salesCell->getElementsByClassName("tree-cell-label")) {
                salesLabel = node;
            }
            if (salesLabel != nullptr) {
                Expect(salesLabel->getAbsoluteX() + salesLabel->getWidth() <= plus->getAbsoluteX() + 1,
                       "the label stops before the plus");
            }
        }
        scene->noteMove(plus->getAbsoluteX() + plus->getWidth() * 0.5,
                        plus->getAbsoluteY() + plus->getHeight() * 0.5);
        scene->layout(240, 200, 0);
        Expect(plus->isVisible() && tree->getHoveredItem() == sales.get(),
               "the plus stays while the pointer is on it");
        ClickAt(*scene, plus.get(), plus->getWidth() * 0.5);
        Expect(plusClicks == 1, "the plus receives the click");
        Expect(sales->isExpanded() == wasOpen, "clicking the plus does not toggle the branch");
        carolCell = CellNamed(*scene, "Carol");
        if (carolCell != nullptr) {
            scene->noteMove(carolCell->getAbsoluteX() + 12, carolCell->getAbsoluteY() + carolCell->getHeight() * 0.5);
            scene->layout(240, 200, 0);
            carolCell = CellNamed(*scene, "Carol");
            Expect(tree->getHoveredItem() == carol.get(), "the plus follows the row under the pointer");
            if (carolCell != nullptr) {
                Expect(Near(plus->getAbsoluteY() + plus->getHeight() * 0.5,
                            carolCell->getAbsoluteY() + carolCell->getHeight() * 0.5, 2),
                       "the plus moves onto Carol");
            }
        }
    }
    scene->notePointerExit();
    scene->layout(240, 200, 0);
    Expect(!plus->isVisible(), "leaving the tree hides the plus");
    tree->setHoverAccessory(nullptr);
}

void TestGrayscaleFrame() {
    const GlyphShot shot = CaptureGlyph("jadefx-gray.ppm", "font-smoothing: antialiased;");
    Expect(shot.ok, "grayscale frame draws without a GL error");
    Expect(shot.fringe == 0, "grayscale text keeps the color channels equal");
    Expect(shot.dark > 20, "grayscale glyph interior is covered");
    Expect(shot.light > 20, "grayscale background stays light");
}

void TestTabReorder() {
    auto first = jadefx::make<jadefx::Tab>("One", jadefx::make<jadefx::Label>("A"));
    auto second = jadefx::make<jadefx::Tab>("Two", jadefx::make<jadefx::Label>("B"));
    auto third = jadefx::make<jadefx::Tab>("Three", jadefx::make<jadefx::Label>("C"));
    auto pane = jadefx::make<jadefx::TabPane>();
    pane->setPrefSize(360, 200);
    pane->getTabs().add(first);
    pane->getTabs().add(second);
    pane->getTabs().add(third);
    auto scene = jadefx::make<jadefx::Scene>(pane, 360, 200);
    scene->layout(360, 200, 0);

    jadefx::Node* one = FindClass(*scene, "tab", 0);
    jadefx::Node* three = FindClass(*scene, "tab", 2);
    Expect(one != nullptr && three != nullptr, "three headers are laid out");
    if (one == nullptr || three == nullptr) {
        return;
    }
    const double y = one->getAbsoluteY() + one->getHeight() * 0.5;
    const double dropX = three->getAbsoluteX() + three->getWidth() - 1;
    scene->noteButton(0, true, one->getAbsoluteX() + 8, y);
    scene->noteMove(dropX, y);
    scene->layout(360, 200, 0);
    Expect(pane->getTabs()[0]->getText() == "Two", "dragging the first header past the others moves it");
    Expect(pane->getTabs()[2]->getText() == "One", "the dragged tab lands at the end");
    scene->noteButton(0, false, dropX, y);

    scene->layout(360, 200, 0);
    jadefx::Node* left = FindClass(*scene, "tab", 0);
    if (left != nullptr) {
        const double leftX = left->getAbsoluteX() + 8;
        const double leftY = left->getAbsoluteY() + left->getHeight() * 0.5;
        scene->noteButton(0, true, leftX, leftY);
        scene->noteMove(leftX + 2, leftY);
        scene->noteButton(0, false, leftX + 2, leftY);
    }
    Expect(pane->getTabs()[0]->getText() == "Two", "a short drag leaves the order alone");

    int outsides = 0;
    pane->setOnTabDrag([&](const jadefx::TabDrag& drag) {
        if (drag.released && drag.outside && drag.tab) {
            ++outsides;
        }
    });
    scene->layout(360, 200, 0);
    jadefx::Node* header = FindClass(*scene, "tab", 0);
    if (header != nullptr) {
        const double hx = header->getAbsoluteX() + 10;
        const double hy = header->getAbsoluteY() + header->getHeight() * 0.5;
        scene->noteButton(0, true, hx, hy);
        scene->noteMove(hx, -40);
        scene->noteButton(0, false, hx, -40);
    }
    Expect(outsides == 1, "releasing outside the pane reports the drag");
    Expect(pane->getTabs().size() == 3, "the pane keeps the tab until the handler moves it");

    int moves = 0;
    pane->setOnTabDrag([&](const jadefx::TabDrag& drag) {
        if (!drag.released && drag.outside) {
            ++moves;
        }
    });
    scene->layout(360, 200, 0);
    header = FindClass(*scene, "tab", 0);
    const std::string before = pane->getTabs()[0]->getText();
    if (header != nullptr) {
        const double hx = header->getAbsoluteX() + 10;
        const double hy = header->getAbsoluteY() + header->getHeight() * 0.5;
        const jadefx::TabHeaderGap gap = pane->headerGap(hx, hy);
        Expect(gap.valid && gap.index == 0, "a point on the first header marks that gap");
        scene->noteButton(0, true, hx, hy);
        scene->noteMove(hx, hy + header->getHeight() + 30);
        scene->noteButton(0, false, hx, hy + header->getHeight() + 30);
    }
    Expect(moves >= 1, "dragging off the header reports the gesture");
    Expect(pane->getTabs()[0]->getText() == before, "dragging into the content does not reorder");

    auto stuck = jadefx::make<jadefx::Tab>("Stay", jadefx::make<jadefx::Label>("S"));
    stuck->setClosable(false);
    pane->getTabs().add(stuck);
    Expect(!pane->close(stuck), "a tab that is not closable stays");
    Expect(pane->close(first), "close removes a closable tab");
    int blocked = 0;
    second->setOnCloseRequest([&](jadefx::TabCloseRequest& request) {
        ++blocked;
        request.consume();
    });
    Expect(!pane->close(second) && blocked == 1, "a consumed close request keeps the tab");
}

void TestUtilityWindow() {
    jadefx::GlfwHost host;
    if (!host.create(640, 480, "primary")) {
        Expect(false, "the primary window opens");
        return;
    }
    jadefx::Stage stage;
    host.bind(&stage);
    jadefx::bindDesktopPrimary(host, stage);
    if (!stage.initializeGraphics(&jadefx::GlfwHost::proc)) {
        Expect(false, "primary graphics initialize");
        host.destroy();
        return;
    }
    stage.show();
    host.poll();
    std::shared_ptr<jadefx::UtilityWindow> utility = jadefx::UtilityWindow::open("Tools", 320, 240, 40, 40);
    Expect(utility != nullptr && utility->isOpen(), "a utility window opens beside the primary");
    if (utility) {
        double screenX = 0;
        double screenY = 0;
        Expect(jadefx::stageToScreen(utility->stage(), 0, 0, screenX, screenY), "a utility point maps to the screen");
        jadefx::Stage* hit = nullptr;
        double localX = 0;
        double localY = 0;
        host.poll();
        Expect(jadefx::windowUnderScreen(screenX + 10, screenY + 10, hit, localX, localY) && hit == &utility->stage(),
               "the utility window is the window under its own screen point");
        utility->setCanClose([]() { return false; });
        Expect(!utility->tryClose() && utility->isOpen(), "a refused close leaves the utility window open");
        utility->setCanClose([]() { return true; });
        Expect(utility->tryClose() && !utility->isOpen(), "an allowed close destroys the utility window");
    }
    jadefx::shutdownDesktopWindows();
    stage.shutdownGraphics();
    host.destroy();
}

}  // namespace

int RunRichTextTests();
int RunButtonTests();
int RunCursorTests();
int RunToggleTests();
int RunRadioButtonTests();
int RunCheckBoxTests();
int RunProgressBarTests();
int RunTooltipTests();
int RunAlertTests();
int RunTextFieldTests();
int RunMenuTests();
int RunComboBoxTests();
int RunSliderTests();
int RunSpinnerTests();
int RunSplitPaneTests();
int RunImageTests();
int RunRunLaterTests();
int RunTreeViewTests();

int main() {
    TestColors();
    TestFont();
    TestSubpixelCoverage();
    TestFontSmoothing();
    TestSubpixelFrame();
    TestResizeRedraws();
    TestGrayscaleFrame();
    TestStylesheetHover();
    TestCssFixes();
    TestVBox();
    TestLayoutImprovements();
    TestCalcAndBorder();
    TestLabelEllipsis();
    TestTabPane();
    TestTabReorder();
    TestUtilityWindow();
    TestTreeView();
    gFailures += RunRichTextTests();
    gFailures += RunRunLaterTests();
    gFailures += RunButtonTests();
    gFailures += RunCursorTests();
    gFailures += RunToggleTests();
    gFailures += RunRadioButtonTests();
    gFailures += RunCheckBoxTests();
    gFailures += RunProgressBarTests();
    gFailures += RunTooltipTests();
    gFailures += RunAlertTests();
    gFailures += RunTextFieldTests();
    gFailures += RunMenuTests();
    gFailures += RunComboBoxTests();
    gFailures += RunSliderTests();
    gFailures += RunSpinnerTests();
    gFailures += RunSplitPaneTests();
    gFailures += RunImageTests();
    gFailures += RunTreeViewTests();
    if (gFailures == 0) {
        std::printf("layout tests passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d layout tests failed\n", gFailures);
    return 1;
}

#include "jadefx/jadefx.hpp"

#include <cmath>
#include <cstdio>
#include <memory>

// Color's HSB and hex, ColorChooser, and ColorPicker.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

bool Near(double a, double b, double tolerance = 0.01) { return std::fabs(a - b) <= tolerance; }

template <typename T>
T* Find(jadefx::Node& root, const char* styleClass) {
    for (jadefx::Node* node : root.getElementsByClassName(styleClass)) {
        if (T* typed = dynamic_cast<T*>(node)) {
            return typed;
        }
    }
    return nullptr;
}

int SpinnerValue(jadefx::Node& root, const char* styleClass) {
    jadefx::Spinner* spinner = Find<jadefx::Spinner>(root, styleClass);
    auto* factory = spinner != nullptr ? dynamic_cast<jadefx::IntegerSpinnerValueFactory*>(spinner->getValueFactory())
                                       : nullptr;
    return factory != nullptr ? factory->getValue() : -1;
}

void TestColorModel() {
    const jadefx::Color red = jadefx::Color::hsb(0, 1, 1);
    Expect(Near(red.r, 1) && Near(red.g, 0) && Near(red.b, 0) && Near(red.a, 1), "hsb 0 is red");
    const jadefx::Color wrapped = jadefx::Color::hsb(480, 1, 0.5);
    Expect(Near(wrapped.g, 0.5) && Near(wrapped.r, 0) && Near(wrapped.b, 0), "hue wraps: 480 is a half-bright green");
    const jadefx::Color blue = jadefx::Color::parse("#336699");
    Expect(Near(blue.getHue(), 210, 0.5) && Near(blue.getSaturation(), 0.667) && Near(blue.getBrightness(), 0.6),
           "hue, saturation, and brightness of #336699");
    Expect(jadefx::Color::rgb8(128, 128, 128).getHue() == 0 && jadefx::Color::rgb8(128, 128, 128).getSaturation() == 0,
           "a gray has no hue or saturation");
    Expect(blue.toHex() == "#336699" && jadefx::Color::rgb8(255, 0, 16, 128).toHex(true) == "#ff001080",
           "hex codes are lowercase, with alpha on request");
}

void TestChooser() {
    auto chooser = jadefx::make<jadefx::ColorChooser>(jadefx::Color::parse("#336699"));
    auto scene = jadefx::make<jadefx::Scene>(chooser, 700, 500);
    scene->layout(700, 500, 0);
    int changes = 0;
    chooser->setOnValueChanged([&] { ++changes; });
    Expect(SpinnerValue(*chooser, "red") == 0x33 && SpinnerValue(*chooser, "blue") == 0x99 &&
               SpinnerValue(*chooser, "hue") == 210 && SpinnerValue(*chooser, "alpha") == 255,
           "every number box shows the value");
    auto* hex = Find<jadefx::TextField>(*chooser, "hex");
    Expect(hex != nullptr && hex->getText() == "#336699", "the hex field shows the value");

    auto* saturation = Find<jadefx::Slider>(*chooser, "saturation");
    saturation->setValue(0);
    Expect(changes == 1 && Near(chooser->getValue().getSaturation(), 0) && SpinnerValue(*chooser, "hue") == 210,
           "a gray keeps its hue");
    saturation->setValue(50);
    Expect(Near(chooser->getValue().getHue(), 210, 0.5) && Near(chooser->getValue().getSaturation(), 0.5),
           "so saturation comes back to the same hue");

    auto* redSpinner = Find<jadefx::Spinner>(*chooser, "red");
    dynamic_cast<jadefx::IntegerSpinnerValueFactory*>(redSpinner->getValueFactory())->setValue(255);
    Expect(Near(chooser->getValue().r, 1) && Find<jadefx::Slider>(*chooser, "red")->getValue() == 255,
           "a number box moves its slider and the color");

    hex->setText("ff000080");
    hex->fire();
    const jadefx::Color typed = chooser->getValue();
    Expect(Near(typed.r, 1) && Near(typed.g, 0) && Near(typed.a, 128 / 255.0) && SpinnerValue(*chooser, "alpha") == 128,
           "a hex code without # sets the color and its alpha");
    hex->setText("#00f");
    hex->fire();
    Expect(Near(chooser->getValue().b, 1) && Near(chooser->getValue().a, 128 / 255.0),
           "a short hex code keeps the alpha");
    hex->setText("nonsense");
    hex->fire();
    Expect(hex->getText() == "#0000ff80" && Near(chooser->getValue().b, 1), "a bad hex code is put back");

    const int before = changes;
    chooser->setValue(jadefx::Color::white());
    Expect(changes == before, "setValue does not run the value handler");

    chooser->addRecentColor(jadefx::Color::black());
    chooser->addRecentColor(jadefx::Color::white());
    chooser->addRecentColor(jadefx::Color::black());
    Expect(chooser->getRecentColors().size() == 2 && jadefx::near(chooser->getRecentColors()[0], jadefx::Color::black()),
           "a recent color moves to the front instead of repeating");
    for (int i = 0; i < 20; ++i) {
        chooser->addRecentColor(jadefx::Color::rgb8(i, 0, 0));
    }
    Expect(chooser->getRecentColors().size() == jadefx::ColorChooser::kRecentLimit, "recent colors are capped");
    Expect(chooser->getPresets().size() == 72, "the default palette is twelve grays and sixty hues");

    // Without alpha the chooser is RGB only.
    chooser->setValue(jadefx::Color::rgba(1, 0, 0, 0.5f));
    chooser->setShowAlpha(false);
    scene->layout(700, 500, 1);
    Expect(!chooser->isShowAlpha() && jadefx::near(chooser->getValue(), jadefx::Color::rgba(1, 0, 0, 1)),
           "hiding alpha makes the value opaque");
    Expect(!Find<jadefx::Slider>(*chooser, "alpha")->isVisible() && hex->getText() == "#ff0000",
           "and hides the alpha row, and alpha in the hex code");
    hex->setText("#00ff0080");
    hex->fire();
    Expect(jadefx::near(chooser->getValue(), jadefx::Color::rgba(0, 1, 0, 1)), "typed alpha is dropped");
    chooser->setShowAlpha(true);
    Expect(Find<jadefx::Slider>(*chooser, "alpha")->isVisible(), "alpha comes back");

    const double tall = chooser->measuredHeight(700, -1);
    chooser->setShowRecentColors(false);
    scene->layout(700, 500, 2);
    Expect(!chooser->isShowRecentColors() && chooser->getElementsByClassName("caption").size() == 4,
           "hiding recent colors takes their caption and swatches out");
    Expect(chooser->measuredHeight(700, -1) < tall, "and the chooser is shorter for it");
    chooser->setShowRecentColors(true);
    Expect(chooser->getElementsByClassName("caption").size() == 5, "they come back");
}

// Tab goes down the fields, R to A with the hex code after B, and Shift+Tab back up.
void TestTabOrder() {
    auto chooser = jadefx::make<jadefx::ColorChooser>(jadefx::Color::parse("#336699"));
    auto scene = jadefx::make<jadefx::Scene>(chooser, 700, 500);
    scene->layout(700, 500, 0);
    jadefx::TextField* red = Find<jadefx::Spinner>(*chooser, "red")->getEditor();
    jadefx::TextField* green = Find<jadefx::Spinner>(*chooser, "green")->getEditor();
    jadefx::TextField* blue = Find<jadefx::Spinner>(*chooser, "blue")->getEditor();
    jadefx::TextField* value = Find<jadefx::Spinner>(*chooser, "brightness")->getEditor();
    jadefx::TextField* alpha = Find<jadefx::Spinner>(*chooser, "alpha")->getEditor();
    auto* hex = Find<jadefx::TextField>(*chooser, "hex");
    auto tab = [&](bool shift) { scene->noteKey(jadefx::Key::Tab, true, false, shift ? jadefx::Key::ModShift : 0); };

    red->requestFocus();
    tab(false);
    Expect(green->isFocused() && green->getSelectedText() == "102", "Tab moves to the next field and selects its text");
    tab(true);
    Expect(red->isFocused(), "Shift+Tab moves back up");
    blue->requestFocus();
    tab(false);
    Expect(hex->isFocused() && hex->getSelectedText() == "#336699", "the hex code comes after blue");
    alpha->requestFocus();
    tab(false);
    Expect(red->isFocused(), "Tab from the last field wraps to the first");
    tab(true);
    Expect(alpha->isFocused(), "and Shift+Tab from the first wraps to the last");

    chooser->setShowAlpha(false);
    scene->layout(700, 500, 1);
    value->requestFocus();
    tab(false);
    Expect(red->isFocused(), "a hidden alpha field is skipped");

    // A typed value is applied on the way out.
    red->selectAll();
    scene->noteText("200");
    tab(false);
    Expect(SpinnerValue(*chooser, "red") == 200 && green->isFocused() && green->getSelectedText() == "102",
           "Tab commits the field it leaves and selects the next");
    scene->layout(700, 500, 2);
    Expect(green->getSelectedText() == "102", "and the selection stays after the next layout");
}

void TestPicker() {
    auto picker = jadefx::make<jadefx::ColorPicker>(jadefx::Color::parse("#1a73e8"));
    auto root = jadefx::make<jadefx::Pane>();
    root->getChildren().add(picker);
    auto scene = jadefx::make<jadefx::Scene>(root, 900, 700);
    scene->layout(900, 700, 0);
    int actions = 0;
    int changes = 0;
    picker->setOnAction([&](jadefx::ActionEvent&) { ++actions; });
    picker->setOnValueChanged([&] { ++changes; });
    jadefx::ColorChooser& chooser = picker->getColorChooser();
    auto* hue = Find<jadefx::Slider>(chooser, "hue");

    // Escape puts the old color back.
    picker->requestFocus();
    scene->noteKey(jadefx::Key::Space, true, false, 0);
    scene->layout(900, 700, 0.1);
    Expect(picker->isShowing() && scene->isPopupShowing(&chooser), "Space opens the chooser");
    Expect(chooser.pseudoState("popover-open"), "the open chooser matches :popover-open");
    hue->setValue(0);
    Expect(changes == 1 && Near(picker->getValue().r, 232 / 255.0, 0.01), "the value follows the chooser live");
    scene->noteKey(jadefx::Key::Escape, true, false, 0);
    scene->layout(900, 700, 0.2);
    Expect(!picker->isShowing() && picker->getValue().toHex() == "#1a73e8" && actions == 0,
           "Escape closes and restores the color without an action");
    Expect(chooser.getRecentColors().empty(), "a cancelled color is not recent");

    // A field focused in the popup lets go of the focus when the popup closes.
    picker->show();
    scene->layout(900, 700, 0.25);
    jadefx::TextField* red = Find<jadefx::Spinner>(chooser, "red")->getEditor();
    red->requestFocus();
    red->selectAll();
    picker->hide();
    Expect(!red->isFocused() && !red->isFocusWithin() && red->getSelectedText().empty(),
           "a field in a closed popup is neither focused nor selected");

    // Enter keeps the new color.
    picker->show();
    hue->setValue(120);
    scene->noteKey(jadefx::Key::Enter, true, false, 0);
    scene->layout(900, 700, 0.3);
    const std::string green = picker->getValue().toHex();
    Expect(!picker->isShowing() && actions == 1 && green != "#1a73e8", "Enter keeps the color and fires the action");
    Expect(chooser.getRecentColors().size() == 1 && chooser.getRecentColors()[0].toHex() == green,
           "the kept color is recent");

    // A press outside keeps it too, and closing on the same color fires nothing.
    picker->show();
    Expect(chooser.getOriginalValue().toHex() == green, "the chooser compares against the color it opened with");
    hue->setValue(240);
    scene->noteButton(0, true, 890, 690, 0);
    scene->noteButton(0, false, 890, 690, 0);
    scene->layout(900, 700, 0.4);
    Expect(!picker->isShowing() && actions == 2, "a press outside keeps the color");
    picker->show();
    picker->hide();
    Expect(actions == 2, "closing unchanged fires nothing");

    picker->setValue(jadefx::Color::rgba(1, 0, 0, 0.5f));
    Expect(actions == 2 && picker->getValue().toHex(true) == "#ff000080", "setValue does not fire the action");
}

}  // namespace

int RunColorPickerTests() {
    gFailures = 0;
    TestColorModel();
    TestChooser();
    TestTabOrder();
    TestPicker();
    if (gFailures == 0) {
        std::printf("color picker tests passed\n");
    }
    return gFailures;
}

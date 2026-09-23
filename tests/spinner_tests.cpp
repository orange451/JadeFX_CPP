#include "jadefx/jadefx.hpp"
#include "jadefx/scene/controls/Spinner.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
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

void ExpectEq(const std::string& actual, const std::string& wanted, const char* message) {
    if (actual != wanted) {
        std::fprintf(stderr, "FAIL %s (got \"%s\", wanted \"%s\")\n", message, actual.c_str(), wanted.c_str());
        ++gFailures;
    }
}

void ExpectNear(double actual, double wanted, const char* message) {
    if (std::fabs(actual - wanted) > 0.001) {
        std::fprintf(stderr, "FAIL %s (got %.4f, wanted %.4f)\n", message, actual, wanted);
        ++gFailures;
    }
}

void Click(jadefx::Scene& scene, double x, double y) {
    scene.noteButton(0, true, x, y);
    scene.noteButton(0, false, x, y);
}

void ClickNode(jadefx::Scene& scene, jadefx::Node& node) {
    Click(scene, node.getAbsoluteX() + node.getWidth() * 0.5, node.getAbsoluteY() + node.getHeight() * 0.5);
}

struct Box {
    std::shared_ptr<jadefx::Pane> root;
    std::shared_ptr<jadefx::Spinner> spinner;
    std::shared_ptr<jadefx::Scene> scene;
};

Box Place(const std::shared_ptr<jadefx::Spinner>& spinner) {
    Box box;
    box.spinner = spinner;
    box.root = jadefx::make<jadefx::Pane>();
    box.root->setPrefSize(420, 220);
    box.root->getChildren().add(spinner);
    box.scene = jadefx::make<jadefx::Scene>(box.root, 420, 220);
    box.scene->layout(420, 220, 0);
    return box;
}

void TestIntegerFactory() {
    auto values = std::make_shared<jadefx::IntegerSpinnerValueFactory>(0, 10, 3, 2);
    Expect(values->getValue() == 3, "the integer factory starts at the initial value");
    Expect(values->getAmountToStepBy() == 2, "the step is stored");
    values->increment(1);
    Expect(values->getValue() == 5, "increment adds the step");
    values->setValue(100);
    Expect(values->getValue() == 10, "setValue clamps to max");
    values->setValue(-4);
    Expect(values->getValue() == 0, "setValue clamps to min");
    values->setMin(4);
    Expect(values->getMin() == 4, "min is stored");
    Expect(values->getValue() == 4, "raising min pulls the value up");
    values->setMax(6);
    values->setValue(6);
    values->setMax(5);
    Expect(values->getMax() == 5, "max is stored");
    Expect(values->getValue() == 5, "lowering max pulls the value down");

    auto ends = std::make_shared<jadefx::IntegerSpinnerValueFactory>(0, 10, 10);
    ends->setWrapAround(true);
    ends->increment(1);
    Expect(ends->getValue() == 0, "wrap from max lands on min");
    ends->decrement(1);
    Expect(ends->getValue() == 10, "wrap from min lands on max");
    ends->setValue(1);
    ends->setAmountToStepBy(3);
    ends->decrement(1);
    Expect(ends->getValue() == 9, "a step past min wraps by the whole span");

    auto shifted = std::make_shared<jadefx::IntegerSpinnerValueFactory>(5, 8, 5);
    shifted->setWrapAround(true);
    shifted->decrement(1);
    Expect(shifted->getValue() == 8, "wrap works when min is above zero");

    Expect(!values->commitText("nope"), "an integer factory rejects text that is not a number");
    Expect(values->getValue() == 5, "a rejected commit keeps the value");
    Expect(values->commitText(" 8 "), "surrounding spaces are allowed");
    Expect(values->getValue() == 5, "8 is clamped back to the current max");
    values->setMax(20);
    Expect(values->commitText("010"), "leading zeros parse as an integer");
    Expect(values->getValue() == 10, "the parsed integer is stored");
    ExpectEq(values->valueText(), "10", "the editor text is the decimal form");
}

void TestDoubleAndList() {
    auto values = std::make_shared<jadefx::DoubleSpinnerValueFactory>(0, 10, 1.5, 0.5);
    ExpectNear(values->getValue(), 1.5, "the double factory starts at the initial value");
    ExpectEq(values->valueText(), "1.5", "one fractional digit is kept");
    values->setValue(2);
    ExpectEq(values->valueText(), "2", "a whole double drops the fraction");
    values->setValue(1.256);
    ExpectEq(values->valueText(), "1.26", "the converter keeps two fractional digits");
    values->setValue(1.26);
    values->increment(1);
    ExpectNear(values->getValue(), 1.76, "increment adds the double step");
    Expect(values->commitText("4.5"), "a decimal commits");
    ExpectNear(values->getValue(), 4.5, "the committed decimal is stored");
    Expect(!values->commitText("4.5x"), "trailing junk is rejected");
    ExpectNear(values->getValue(), 4.5, "rejected decimal text keeps the value");

    values->setValue(10);
    values->setWrapAround(true);
    values->increment(1);
    ExpectNear(values->getValue(), 0.5, "a double step past max wraps by max - min");
    values->setValue(0);
    values->setAmountToStepBy(1);
    values->decrement(1);
    ExpectNear(values->getValue(), 9, "a double step below min wraps with the same modulus");

    auto list = std::make_shared<jadefx::ListSpinnerValueFactory>(std::vector<std::string>{"Red", "Green", "Blue"});
    ExpectEq(list->getValue(), "Red", "the list starts on the first item");
    Expect(list->getIndex() == 0, "the first index is zero");
    list->increment(1);
    ExpectEq(list->getValue(), "Green", "increment walks the list");
    list->increment(1);
    list->increment(1);
    ExpectEq(list->getValue(), "Blue", "the list stops on the last item");
    list->setWrapAround(true);
    list->increment(1);
    ExpectEq(list->getValue(), "Red", "wrap returns to the first item");
    list->decrement(1);
    ExpectEq(list->getValue(), "Blue", "wrap from the first item returns to the last");
    list->setValue("Gold");
    ExpectEq(list->getValue(), "Gold", "an unknown string becomes the value");
    Expect(list->getItems().size() == 4, "that string is appended to the list");
    Expect(list->commitText("Green"), "committing a list item selects it");
    ExpectEq(list->getValue(), "Green", "the committed item is selected");
    Expect(list->getIndex() == 1, "the committed item keeps its index");
}

void TestEditorAndArrows() {
    auto values = std::make_shared<jadefx::IntegerSpinnerValueFactory>(1, 12, 5);
    auto spinner = jadefx::make<jadefx::Spinner>(values);
    Expect(std::strcmp(spinner->getElementType(), "spinner") == 0, "element type is spinner");
    Expect(spinner->arrowsAreVertical(), "the default arrows point up and down");
    Expect(spinner->getEditor() != nullptr, "the editor exists before the spinner is editable");
    ExpectEq(spinner->getEditor()->getText(), "5", "the editor shows the value");
    Box box = Place(spinner);
    jadefx::TextField* editor = spinner->getEditor();
    Expect(editor->getY() == 0, "the editor starts at the top of the spinner");
    Expect(std::fabs(editor->getHeight() - spinner->getHeight()) < 0.6, "the editor fills the spinner height");
    Expect(editor->computedStyle().background.color.a == 0.f, "the editor background stays clear");

    jadefx::Node* up = spinner->getElementById("increment");
    jadefx::Node* down = spinner->getElementById("decrement");
    Expect(up != nullptr && down != nullptr, "both arrow buttons are children");
    if (up == nullptr || down == nullptr) {
        return;
    }
    Expect(up->getX() + 0.5 >= editor->getX() + editor->getWidth(), "the arrows sit to the right of the editor");
    Expect(up->getY() < down->getY(), "increment is above decrement");
    Expect(std::fabs(up->getHeight() + down->getHeight() - spinner->getHeight()) < 0.6, "the arrows split the height");

    int changes = 0;
    spinner->setOnValueChanged([&] { ++changes; });
    ClickNode(*box.scene, *up);
    Expect(values->getValue() == 6, "the increment button steps once");
    ExpectEq(editor->getText(), "6", "the editor follows the button");
    Expect(changes == 1, "the step notifies the spinner");
    ClickNode(*box.scene, *down);
    Expect(values->getValue() == 5, "the decrement button steps back");

    box.scene->requestFocus(spinner.get());
    Expect(box.scene->noteKey(jadefx::Key::Up, true, false, 0), "Up is consumed");
    Expect(values->getValue() == 6, "Up increments");
    Expect(box.scene->noteKey(jadefx::Key::Down, true, false, 0), "Down is consumed");
    Expect(values->getValue() == 5, "Down decrements");
    Expect(!box.scene->noteKey(jadefx::Key::Left, true, false, 0), "Left is left for a vertical spinner");
    Expect(values->getValue() == 5, "Left does not change a vertical spinner");

    spinner->setEditable(true);
    box.scene->requestFocus(editor);
    editor->setText("9");
    Expect(values->getValue() == 5, "typing does not change the value");
    Expect(box.scene->noteKey(jadefx::Key::Enter, true, false, 0), "Enter is consumed");
    Expect(values->getValue() == 9, "Enter commits the editor");
    ExpectEq(editor->getText(), "9", "a clean number stays in the editor");
    editor->setText("nope");
    box.scene->noteKey(jadefx::Key::Enter, true, false, 0);
    Expect(values->getValue() == 9, "bad text does not change the value");
    ExpectEq(editor->getText(), "9", "bad text is replaced with the value");

    editor->setText("11");
    box.scene->noteKey(jadefx::Key::Up, true, false, 0);
    Expect(values->getValue() == 12, "Up commits the editor and then steps");
    ExpectEq(editor->getText(), "12", "the step is written back to the editor");

    box.scene->requestFocus(editor);
    box.scene->layout(420, 220, 0);
    editor->setText("4");
    Click(*box.scene, 400, 200);
    box.scene->layout(420, 220, 0);
    Expect(values->getValue() == 4, "leaving the editor commits");
    Expect(!editor->isFocused(), "the outside click moves focus");
}

void TestRepeatAndLayouts() {
    auto values = std::make_shared<jadefx::IntegerSpinnerValueFactory>(0, 40, 1);
    auto spinner = jadefx::make<jadefx::Spinner>(values);
    Box box = Place(spinner);
    jadefx::Node* up = spinner->getElementById("increment");
    Expect(up != nullptr, "the repeat test can see the increment button");
    if (up == nullptr) {
        return;
    }
    const double x = up->getAbsoluteX() + up->getWidth() * 0.5;
    const double y = up->getAbsoluteY() + up->getHeight() * 0.5;
    box.scene->noteButton(0, true, x, y);
    Expect(values->getValue() == 2, "pressing an arrow steps immediately");
    box.scene->layout(420, 220, 0.2);
    Expect(values->getValue() == 2, "the first repeat waits for the initial delay");
    box.scene->layout(420, 220, 0.31);
    Expect(values->getValue() == 3, "the value steps again after the initial delay");
    box.scene->layout(420, 220, 0.37);
    Expect(values->getValue() == 4, "later repeats use the repeat delay");
    box.scene->noteButton(0, false, x, y);
    box.scene->layout(420, 220, 2);
    Expect(values->getValue() == 4, "releasing the arrow stops the repeat");

    spinner->getClassList().add(jadefx::Spinner::kSplitArrowsHorizontal);
    box.scene->layout(420, 220, 2);
    Expect(!spinner->arrowsAreVertical(), "split horizontal arrows point sideways");
    jadefx::Node* down = spinner->getElementById("decrement");
    jadefx::TextField* editor = spinner->getEditor();
    Expect(down != nullptr && editor != nullptr, "the split layout keeps both arrows and the editor");
    if (down != nullptr && editor != nullptr) {
        Expect(down->getX() + down->getWidth() <= editor->getX() + 0.6, "decrement sits to the left of the editor");
        Expect(up->getX() + 0.5 >= editor->getX() + editor->getWidth(), "increment sits to the right of the editor");
    }
    box.scene->requestFocus(spinner.get());
    Expect(box.scene->noteKey(jadefx::Key::Right, true, false, 0), "Right increments sideways arrows");
    Expect(values->getValue() == 5, "Right steps the value");
    Expect(!box.scene->noteKey(jadefx::Key::Up, true, false, 0), "Up is not consumed for sideways arrows");
    Expect(values->getValue() == 5, "Up does not step sideways arrows");

    spinner->getClassList().add(jadefx::Spinner::kArrowsOnLeftVertical);
    box.scene->layout(420, 220, 2);
    Expect(spinner->arrowsAreVertical(), "left vertical wins over the horizontal class");
    if (down != nullptr && editor != nullptr) {
        Expect(up->getX() + up->getWidth() <= editor->getX() + 0.6, "left vertical arrows sit before the editor");
    }

    spinner->setDisable(true);
    const int frozen = values->getValue();
    if (up != nullptr) {
        ClickNode(*box.scene, *up);
    }
    box.scene->noteKey(jadefx::Key::Down, true, false, 0);
    Expect(values->getValue() == frozen, "a disabled spinner ignores the arrows and the keys");

    auto bare = jadefx::make<jadefx::Spinner>();
    bare->increment();
    bare->decrement();
    Expect(bare->getValueFactory() == nullptr, "a spinner can exist without a value factory");
    ExpectEq(bare->getEditor()->getText(), "", "an empty spinner shows no text");
}

}  // namespace

int RunSpinnerTests() {
    TestIntegerFactory();
    TestDoubleAndList();
    TestEditorAndArrows();
    TestRepeatAndLayouts();
    return gFailures;
}

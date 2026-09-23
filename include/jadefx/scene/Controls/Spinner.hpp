#pragma once

#include "jadefx/collections/ObservableList.hpp"
#include "jadefx/scene/Controls/Controls.hpp"
#include "jadefx/scene/Controls/TextField.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace jadefx {

class Spinner;
class SpinnerArrow;
class SpinnerEditor;

// The model behind a Spinner: the current value, the step, and the text
// conversion. OpenJFX splits this the same way. A negative step count runs
// in the other direction.
class SpinnerValueFactory {
    friend class Spinner;

public:
    SpinnerValueFactory(const SpinnerValueFactory&) = delete;
    SpinnerValueFactory& operator=(const SpinnerValueFactory&) = delete;

    virtual ~SpinnerValueFactory() = default;

    virtual void increment(int steps) = 0;
    virtual void decrement(int steps) = 0;

    // The text shown in the editor. Empty when the factory has no value.
    virtual std::string valueText() const = 0;
    // Parses editor text into the value. False leaves the value unchanged.
    virtual bool commitText(const std::string& text) = 0;

    void setWrapAround(bool value) { wrap_ = value; }
    bool isWrapAround() const { return wrap_; }

    // Runs after the value changes. The spinner updates its editor first.
    void setOnValueChanged(std::function<void()> handler) { onChanged_ = std::move(handler); }

protected:
    SpinnerValueFactory() = default;
    void changed();

    bool wrap_ = false;

private:
    void attach(Spinner* spinner) { owner_ = spinner; }

    Spinner* owner_ = nullptr;
    std::function<void()> onChanged_;
};

// Whole numbers from min to max. The step defaults to 1. An initial value
// outside the range becomes min. With wrap on, the next step past either end
// comes back in at the other end.
class IntegerSpinnerValueFactory : public SpinnerValueFactory {
public:
    IntegerSpinnerValueFactory(int min, int max);
    IntegerSpinnerValueFactory(int min, int max, int initialValue);
    IntegerSpinnerValueFactory(int min, int max, int initialValue, int amountToStepBy);

    void setMin(int value);
    int getMin() const { return min_; }
    void setMax(int value);
    int getMax() const { return max_; }
    void setValue(int value);
    int getValue() const { return value_; }
    void setAmountToStepBy(int value) { step_ = value; }
    int getAmountToStepBy() const { return step_; }

    void increment(int steps) override;
    void decrement(int steps) override;
    std::string valueText() const override;
    bool commitText(const std::string& text) override;

private:
    int min_ = 0;
    int max_ = 0;
    int value_ = 0;
    int step_ = 1;
    bool has_ = false;
};

// Decimal numbers. The editor shows at most two fractional digits, matching
// the OpenJFX DecimalFormat("#.##") converter. Wrap uses the distance from
// min to max as the modulus.
class DoubleSpinnerValueFactory : public SpinnerValueFactory {
public:
    DoubleSpinnerValueFactory(double min, double max);
    DoubleSpinnerValueFactory(double min, double max, double initialValue);
    DoubleSpinnerValueFactory(double min, double max, double initialValue, double amountToStepBy);

    void setMin(double value);
    double getMin() const { return min_; }
    void setMax(double value);
    double getMax() const { return max_; }
    void setValue(double value);
    double getValue() const { return value_; }
    void setAmountToStepBy(double value);
    double getAmountToStepBy() const { return step_; }

    void increment(int steps) override;
    void decrement(int steps) override;
    std::string valueText() const override;
    bool commitText(const std::string& text) override;

private:
    double min_ = 0;
    double max_ = 0;
    double value_ = 0;
    double step_ = 1;
    bool has_ = false;
};

// Walks a list of strings. The first item is selected when the list is not
// empty. setValue of a string that is not in the list appends it. With wrap
// on, the ends lead into each other.
class ListSpinnerValueFactory : public SpinnerValueFactory {
public:
    explicit ListSpinnerValueFactory(std::vector<std::string> items);
    ~ListSpinnerValueFactory() override;

    ObservableList<std::string>& getItems() { return items_; }
    const ObservableList<std::string>& getItems() const { return items_; }

    void setValue(std::string value);
    const std::string& getValue() const { return value_; }
    int getIndex() const { return has_ ? current_ : -1; }

    void increment(int steps) override;
    void decrement(int steps) override;
    std::string valueText() const override;
    bool commitText(const std::string& text) override;

private:
    void select(int index);
    void itemsChanged();
    int indexOf(const std::string& value) const;

    ObservableList<std::string> items_;
    std::string value_;
    int current_ = 0;
    bool has_ = false;
    bool mutating_ = false;
};

// A one-line value with a pair of step buttons, in the shape of OpenJFX Spinner.
// The buttons and the arrow keys step the value factory. Holding a button
// repeats: the first repeat waits initialDelay (0.3s), then every repeatDelay (0.06s).
// setEditable(true) keeps typed text local until Enter or until focus leaves the editor.
// The default stacks both arrows on the right. These style classes select the
// other OpenJFX layouts, in the same priority the skin uses:
// arrows-on-left-vertical, arrows-on-left-horizontal, arrows-on-right-horizontal,
// split-arrows-vertical, split-arrows-horizontal.
class Spinner : public Controls {
    friend class SpinnerValueFactory;
    friend class SpinnerArrow;
    friend class SpinnerEditor;

public:
    static constexpr const char* kArrowsOnRightHorizontal = "arrows-on-right-horizontal";
    static constexpr const char* kArrowsOnLeftVertical = "arrows-on-left-vertical";
    static constexpr const char* kArrowsOnLeftHorizontal = "arrows-on-left-horizontal";
    static constexpr const char* kSplitArrowsVertical = "split-arrows-vertical";
    static constexpr const char* kSplitArrowsHorizontal = "split-arrows-horizontal";

    Spinner();
    explicit Spinner(std::shared_ptr<SpinnerValueFactory> factory);
    Spinner(int min, int max, int initialValue);
    Spinner(int min, int max, int initialValue, int amountToStepBy);
    Spinner(double min, double max, double initialValue);
    Spinner(double min, double max, double initialValue, double amountToStepBy);
    explicit Spinner(std::vector<std::string> items);
    ~Spinner() override;

    const char* getElementType() const override { return "spinner"; }

    void setValueFactory(std::shared_ptr<SpinnerValueFactory> factory);
    SpinnerValueFactory* getValueFactory() const { return factory_.get(); }

    void setEditable(bool editable);
    bool isEditable() const { return editable_; }

    // The field is always present. It fills the spinner's height in the side-arrow layouts.
    TextField* getEditor() const;

    void setPromptText(std::string text);
    const std::string& getPromptText() const;

    // Seconds. A negative delay is treated as zero.
    void setInitialDelay(double seconds);
    double getInitialDelay() const { return initialDelay_; }
    void setRepeatDelay(double seconds);
    double getRepeatDelay() const { return repeatDelay_; }

    // Also disables the editor. Node::setDisable is not virtual; call this on the spinner.
    void setDisable(bool value);

    // Commits the editor first when the spinner is editable. A null factory does nothing.
    // A failed commit leaves the value alone and restores the editor.
    void increment(int steps = 1);
    void decrement(int steps = 1);
    // False when editable text cannot be parsed. Not editable, or an empty factory, returns true.
    bool commitValue();
    void cancelEdit();

    // Up and Down when the arrows point vertically. Left and Right when they point sideways.
    bool arrowsAreVertical() const;

    void setOnValueChanged(std::function<void()> handler) { onChanged_ = std::move(handler); }

protected:
    void layoutChildren() override;
    void render(UiRenderer& renderer, float opacity) override;
    void renderContent(UiRenderer& renderer, float opacity) override;
    double preferredContentWidth(double innerAvailable) const override;
    double preferredContentHeight(double innerWidth) const override;
    void handleKey(KeyEvent& event) override;
    void sceneChanged(Scene* previous) override;

private:
    enum class ArrowLayout { RightVertical, LeftVertical, RightHorizontal, LeftHorizontal, SplitVertical, SplitHorizontal };

    ArrowLayout arrowLayout() const;
    bool claimsArrowKey(int key) const;
    void syncFromFactory();
    void onFactoryChanged();
    void pressArrow(bool increment);
    void releaseArrow();
    void advanceSpin();
    double arrowBreadth() const;

    std::shared_ptr<SpinnerEditor> editor_;
    std::shared_ptr<SpinnerArrow> incrementArrow_;
    std::shared_ptr<SpinnerArrow> decrementArrow_;
    std::shared_ptr<SpinnerValueFactory> factory_;
    std::function<void()> onChanged_;
    double initialDelay_ = 0.3;
    double repeatDelay_ = 0.06;
    double nextSpin_ = 0;
    bool editable_ = false;
    bool editorHadFocus_ = false;
    bool committing_ = false;
    bool spinning_ = false;
    bool spinUp_ = false;
};

}  // namespace jadefx

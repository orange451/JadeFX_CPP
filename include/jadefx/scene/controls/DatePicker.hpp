#pragma once

#include "jadefx/scene/controls/ComboBoxBase.hpp"
#include "jadefx/scene/controls/Labeled.hpp"
#include "jadefx/time/LocalDate.hpp"
#include "jadefx/util/StringConverter.hpp"

#include <functional>
#include <memory>
#include <optional>

namespace jadefx {

class DatePicker;
class DatePickerContent;

// One day in a DatePicker's calendar, in the shape of OpenJFX DateCell.
// The calendar binds a date to each cell and calls updateItem. To disable a day
// or style it, as JavaFX's dayCellFactory does, subclass and override updateItem,
// calling the base first; the calendar re-enables every cell before binding it.
// Every cell has the class .day-cell, as in JavaFX. The calendar sets the
// classes .today, .previous-month, and .next-month, the
// :selected pseudo on the picker's date, and :focus-visible on the day the
// arrow keys have moved to. A disabled day cannot be chosen.
class DateCell : public Labeled {
    friend class DatePickerContent;

public:
    DateCell();

    const char* getElementType() const override { return "date-cell"; }

    // The bound date, or nothing before the calendar binds one.
    const std::optional<LocalDate>& getItem() const { return item_; }

    // Shows the day of the month. empty is true only for a cell with no date.
    virtual void updateItem(const LocalDate& date, bool empty);

protected:
    void handleMousePressed(const MouseEvent& event) override;

private:
    void bind(const LocalDate& date);

    std::optional<LocalDate> item_;
    std::function<void(const LocalDate&)> choose_;
};

// A date field with a calendar popup, in the shape of OpenJFX DatePicker and the
// HTML date input. The value is optional: an empty field is no date. The field
// is editable by default and commits typed text, read by the converter, on Enter
// or when focus leaves it; unreadable text is put back. The popup shows a month:
// arrows beside the month and year page through them, and a click on a day
// chooses it. While it is open, the arrow keys move a day or a week, Page Up and
// Page Down a month (a year with Shift), and Enter chooses. Choosing a date, or
// committing a different one, fires the action.
class DatePicker : public ComboBoxBase {
    friend class DatePickerContent;

public:
    DatePicker();
    explicit DatePicker(std::optional<LocalDate> value);
    ~DatePicker() override;

    const char* getElementType() const override { return "date-picker"; }

    // Does not fire the action.
    void setValue(std::optional<LocalDate> value);
    const std::optional<LocalDate>& getValue() const { return value_; }
    // Runs each time the value changes.
    void setOnValueChanged(std::function<void()> handler) { onChanged_ = std::move(handler); }

    // Text for the field and back. The default is ISO 8601, yyyy-mm-dd.
    void setConverter(StringConverter<LocalDate> converter);
    const StringConverter<LocalDate>& getConverter() const { return converter_; }

    // A column of ISO week numbers left of the days.
    void setShowWeekNumbers(bool show);
    bool isShowWeekNumbers() const { return showWeekNumbers_; }
    // The first column of the calendar. The default is Sunday.
    void setFirstDayOfWeek(DayOfWeek day);
    DayOfWeek getFirstDayOfWeek() const { return firstDay_; }
    // Makes the calendar's cells, so a subclass of DateCell can disable or style days.
    void setDayCellFactory(std::function<std::shared_ptr<DateCell>()> factory);

protected:
    std::shared_ptr<Node> createPopupContent() override;
    void popupShowing() override;
    bool handlePopupKey(KeyEvent& event) override;
    std::string valueText() const override;
    void editorAction() override;
    void editorFocusLost() override;
    double preferredContentWidth(double innerAvailable) const override;

private:
    void choose(const LocalDate& date);
    // Reads the field. False when its text is not a date; the field then shows the value again.
    bool commitEditorText();
    void changed(bool fire);

    std::optional<LocalDate> value_;
    StringConverter<LocalDate> converter_;
    std::function<std::shared_ptr<DateCell>()> cellFactory_;
    std::function<void()> onChanged_;
    std::shared_ptr<DatePickerContent> content_;
    DayOfWeek firstDay_ = DayOfWeek::Sunday;
    bool showWeekNumbers_ = false;
};

}  // namespace jadefx

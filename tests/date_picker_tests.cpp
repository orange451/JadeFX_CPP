#include "jadefx/jadefx.hpp"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// LocalDate and DatePicker.
namespace {

int gFailures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        ++gFailures;
    }
}

using jadefx::DayOfWeek;
using jadefx::LocalDate;

void Key(jadefx::Scene& scene, int key, int mods = 0) {
    scene.noteKey(key, true, false, mods);
    scene.noteKey(key, false, false, mods);
}

void TestLocalDate() {
    Expect(LocalDate().toString() == "1970-01-01" && LocalDate::ofEpochDay(0) == LocalDate(), "day 0 is 1970-01-01");
    Expect(LocalDate::of(2024, 2, 30).toString() == "2024-02-29" && LocalDate::of(2023, 13, 1).getMonthValue() == 12,
           "of clamps the month and the day");
    Expect(LocalDate::isLeapYear(2000) && !LocalDate::isLeapYear(1900) && LocalDate::isLeapYear(2024),
           "leap years follow the Gregorian rule");
    bool roundTrip = true;
    for (long long day = -800000; day <= 800000; day += 367) {
        roundTrip = roundTrip && LocalDate::ofEpochDay(day).toEpochDay() == day;
    }
    Expect(roundTrip, "epoch days round-trip across two thousand years either side");
    Expect(LocalDate::of(2026, 9, 27).getDayOfWeek() == DayOfWeek::Sunday &&
               LocalDate::of(1969, 12, 31).getDayOfWeek() == DayOfWeek::Wednesday,
           "day of the week, before and after 1970");
    Expect(LocalDate::of(2024, 1, 31).plusMonths(1).toString() == "2024-02-29" &&
               LocalDate::of(2024, 1, 15).plusMonths(-13).toString() == "2022-12-15" &&
               LocalDate::of(2024, 12, 31).plusDays(1).toString() == "2025-01-01",
           "month arithmetic clamps the day and crosses years");
    Expect(LocalDate::of(2021, 1, 3).getIsoWeek() == 53 && LocalDate::of(2026, 1, 1).getIsoWeek() == 1 &&
               LocalDate::of(2026, 9, 27).getIsoWeek() == 39 && LocalDate::of(2026, 12, 31).getIsoWeek() == 53,
           "ISO week numbers, including weeks that belong to the next or previous year");
    Expect(LocalDate::parse(" 2026-09-27 ") == LocalDate::of(2026, 9, 27) && !LocalDate::parse("2026-02-30") &&
               !LocalDate::parse("2026-9-27") && !LocalDate::parse("tomorrow"),
           "parse reads ISO dates and refuses anything else");
    const jadefx::StringConverter<LocalDate> converter;
    Expect(converter.format(LocalDate::of(2026, 1, 2)) == "2026-01-02" &&
               converter.parse("2026-01-02") == LocalDate::of(2026, 1, 2) && !converter.parse("x"),
           "the default converter uses the ISO text");
}

// Refuses weekends, as a dayCellFactory does in JavaFX.
class WeekdayCell : public jadefx::DateCell {
public:
    void updateItem(const LocalDate& date, bool empty) override {
        DateCell::updateItem(date, empty);
        const DayOfWeek day = date.getDayOfWeek();
        if (day == DayOfWeek::Saturday || day == DayOfWeek::Sunday) {
            setDisable(true);
        }
    }
};

std::vector<jadefx::Label*> Labels(jadefx::Node& root, const char* styleClass) {
    std::vector<jadefx::Label*> labels;
    for (jadefx::Node* node : root.getElementsByClassName(styleClass)) {
        if (auto* label = dynamic_cast<jadefx::Label*>(node)) {
            labels.push_back(label);
        }
    }
    return labels;
}

void TestPicker() {
    auto picker = jadefx::make<jadefx::DatePicker>(LocalDate::of(2026, 9, 15));
    auto root = jadefx::make<jadefx::Pane>();
    root->getChildren().add(picker);
    auto scene = jadefx::make<jadefx::Scene>(root, 800, 600);
    scene->layout(800, 600, 0);
    int actions = 0;
    int changes = 0;
    picker->setOnAction([&](jadefx::ActionEvent&) { ++actions; });
    picker->setOnValueChanged([&] { ++changes; });
    jadefx::TextField* editor = picker->getEditor();
    Expect(picker->isEditable() && editor != nullptr && editor->getText() == "2026-09-15",
           "the field is editable and shows the date");

    editor->setText("2026-10-01");
    editor->fire();
    Expect(picker->getValue() == LocalDate::of(2026, 10, 1) && actions == 1 && changes == 1,
           "Enter in the field commits typed text and fires");
    editor->setText("not a date");
    editor->fire();
    Expect(editor->getText() == "2026-10-01" && actions == 1, "unreadable text is put back");
    editor->setText("");
    editor->fire();
    Expect(!picker->getValue() && actions == 2, "an empty field is no date");
    picker->setValue(LocalDate::of(2026, 9, 15));
    Expect(actions == 2 && editor->getText() == "2026-09-15", "setValue does not fire");

    // The calendar: arrows move the day, Enter chooses it.
    picker->show();
    scene->layout(800, 600, 0.1);
    Expect(picker->isShowing(), "show opens the calendar");
    Key(*scene, jadefx::Key::Right);
    Key(*scene, jadefx::Key::Down);
    Key(*scene, jadefx::Key::Enter);
    scene->layout(800, 600, 0.2);
    Expect(!picker->isShowing() && picker->getValue() == LocalDate::of(2026, 9, 23) && actions == 3,
           "Right and Down move a day and a week, and Enter chooses");
    picker->show();
    Key(*scene, jadefx::Key::PageDown, jadefx::Key::ModShift);
    Key(*scene, jadefx::Key::Escape);
    scene->layout(800, 600, 0.3);
    Expect(!picker->isShowing() && picker->getValue() == LocalDate::of(2026, 9, 23) && actions == 3,
           "Escape closes without choosing");

    // A click on a day chooses it.
    picker->show();
    scene->layout(800, 600, 0.4);
    jadefx::DateCell* target = nullptr;
    for (jadefx::Node* node : scene->getElementsByClassName("next-month")) {
        if (auto* cell = dynamic_cast<jadefx::DateCell*>(node); cell != nullptr && target == nullptr) {
            target = cell;
        }
    }
    Expect(target != nullptr && target->getItem() && target->getItem()->getMonthValue() == 10,
           "days of the next month carry .next-month");
    int selected = 0;
    bool valueSelected = false;
    for (jadefx::Node* node : scene->getElementsByClassName("day-cell")) {
        if (auto* cell = dynamic_cast<jadefx::DateCell*>(node); cell != nullptr && cell->isSelected()) {
            valueSelected = cell->getItem() == picker->getValue();
            ++selected;
        }
    }
    Expect(selected == 1 && valueSelected, "only the value's day matches :selected");
    if (target != nullptr) {
        // The calendar turns to the chosen month, so the cell is read before the click.
        const LocalDate day = *target->getItem();
        const double x = target->getAbsoluteX() + target->getWidth() * 0.5;
        const double y = target->getAbsoluteY() + target->getHeight() * 0.5;
        scene->noteButton(0, true, x, y, 0);
        scene->noteButton(0, false, x, y, 0);
        scene->layout(800, 600, 0.5);
        Expect(!picker->isShowing() && picker->getValue() == day && actions == 4,
               "a click on a day chooses it and closes the calendar");
    }

    // Layout options and a cell factory.
    picker->setFirstDayOfWeek(DayOfWeek::Monday);
    picker->setShowWeekNumbers(true);
    picker->setDayCellFactory([] { return std::make_shared<WeekdayCell>(); });
    picker->setValue(LocalDate::of(2026, 9, 25));
    picker->show();
    scene->layout(800, 600, 0.6);
    const std::vector<jadefx::Label*> names = Labels(*scene, "day-name");
    const std::vector<jadefx::Label*> weeks = Labels(*scene, "week-number");
    Expect(names.size() == 7 && names.front()->getText() == "Mo" && names.back()->getText() == "Su",
           "setFirstDayOfWeek reorders the day names");
    Expect(weeks.size() == 6 && weeks.front()->getText() == "36", "week numbers run down the side");
    Key(*scene, jadefx::Key::Right);
    Key(*scene, jadefx::Key::Enter);
    Expect(picker->isShowing() && picker->getValue() == LocalDate::of(2026, 9, 25),
           "Enter on a day the cell factory disabled does nothing");
    Key(*scene, jadefx::Key::Right);
    Key(*scene, jadefx::Key::Right);
    Key(*scene, jadefx::Key::Enter);
    scene->layout(800, 600, 0.7);
    Expect(!picker->isShowing() && picker->getValue() == LocalDate::of(2026, 9, 28), "an enabled day still chooses");

    jadefx::StringConverter<LocalDate> american;
    american.toString = [](const LocalDate& date) {
        return std::to_string(date.getMonthValue()) + "/" + std::to_string(date.getDayOfMonth()) + "/" +
               std::to_string(date.getYear());
    };
    picker->setConverter(american);
    Expect(editor->getText() == "9/28/2026", "the converter formats the field");
}

}  // namespace

int RunDatePickerTests() {
    gFailures = 0;
    TestLocalDate();
    TestPicker();
    if (gFailures == 0) {
        std::printf("date picker tests passed\n");
    }
    return gFailures;
}

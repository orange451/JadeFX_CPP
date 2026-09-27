#include "jadefx/scene/controls/DatePicker.hpp"

#include "ControlChrome.hpp"
#include "gl/UiRenderer.hpp"
#include "jadefx/scene/controls/Label.hpp"
#include "jadefx/scene/layout/GridPane.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace jadefx {
namespace {

constexpr int kWeeks = 6;
constexpr int kDays = 7;
constexpr double kCellWidth = 34;
constexpr double kCellHeight = 28;
constexpr double kHeaderGap = 6;

const char* MonthName(int month) {
    static constexpr const char* kNames[] = {"January", "February", "March",     "April",   "May",      "June",
                                             "July",    "August",   "September", "October", "November", "December"};
    return kNames[std::clamp(month, 1, 12) - 1];
}

const char* DayName(DayOfWeek day) {
    static constexpr const char* kNames[] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
    return kNames[static_cast<int>(day) - 1];
}

// The column of day in a week that starts on first, 0 to 6.
int ColumnOf(DayOfWeek day, DayOfWeek first) { return (static_cast<int>(day) - static_cast<int>(first) + 7) % 7; }

// A small arrow that pages the calendar.
class CalendarArrow : public Controls {
public:
    CalendarArrow(Side points, std::function<void()> action) : points_(points), action_(std::move(action)) {
        setDefaultCursor(Cursor::Pointer);
        setFocusTraversable(false);
        setPrefSize(24, 24);
    }

    const char* getElementType() const override { return "calendar-arrow"; }

protected:
    void renderContent(UiRenderer& renderer, float opacity) override {
        if (isHovered()) {
            chrome::DrawWash(renderer, *this, opacity, isPressed(), 4.f);
        }
        chrome::DrawArrowHead(renderer, static_cast<float>(getAbsoluteX() + getWidth() * 0.5),
                              static_cast<float>(getAbsoluteY() + getHeight() * 0.5), points_,
                              chrome::Themed(*this, ThemeColor::Muted, opacity));
    }

    void handleMousePressed(const MouseEvent& event) override {
        if (event.button == 0 && action_) {
            action_();
        }
    }

private:
    Side points_;
    std::function<void()> action_;
};

std::shared_ptr<Label> CenteredLabel(const char* styleClass) {
    auto label = std::make_shared<Label>();
    label->getClassList().add(styleClass);
    label->setAlignment(Pos::Center);
    return label;
}

}  // namespace

DateCell::DateCell() : Labeled(std::string()) {
    getClassList().add("day-cell");
    setAlignment(Pos::Center);
    setPrefSize(kCellWidth, kCellHeight);
    setFocusTraversable(false);
    setDefaultCursor(Cursor::Pointer);
}

void DateCell::updateItem(const LocalDate& date, bool empty) {
    setText(empty ? std::string() : std::to_string(date.getDayOfMonth()));
}

void DateCell::bind(const LocalDate& date) {
    item_ = date;
    Node::setDisable(false);
    updateItem(date, false);
}

void DateCell::handleMousePressed(const MouseEvent& event) {
    if (event.button == 0 && item_ && !isDisabled() && choose_) {
        choose_(*item_);
    }
}

// The month under a DatePicker: the month and year with their arrows, the day
// names, and six weeks of DateCells, with week numbers when the picker asks.
class DatePickerContent : public Controls {
public:
    explicit DatePickerContent(DatePicker& picker) : picker_(picker) {
        header_ = std::make_shared<GridPane>();
        GridPane& header = *header_;
        ColumnConstraints grow;
        grow.hgrow = Priority::Always;
        grow.halignment = HPos::Center;
        header.getColumnConstraints() = {ColumnConstraints(), grow, ColumnConstraints(), ColumnConstraints(),
                                          ColumnConstraints(), ColumnConstraints()};
        month_ = CenteredLabel("month");
        year_ = CenteredLabel("year");
        header.add(std::make_shared<CalendarArrow>(Side::Left, [this] { page(-1); }), 0, 0);
        header.add(month_, 1, 0);
        header.add(std::make_shared<CalendarArrow>(Side::Right, [this] { page(1); }), 2, 0);
        header.add(std::make_shared<CalendarArrow>(Side::Left, [this] { page(-12); }), 3, 0);
        header.add(year_, 4, 0);
        header.add(std::make_shared<CalendarArrow>(Side::Right, [this] { page(12); }), 5, 0);
        grid_ = std::make_shared<GridPane>();
        grid_->setHgap(2);
        grid_->setVgap(2);
        children().add(header_);
        children().add(grid_);
        rebuildCells();
    }

    const char* getElementType() const override { return "date-picker-popup"; }

    // Shows the month of date, with date as the keyboard's day.
    void showDate(const LocalDate& date, bool keyboard) {
        focused_ = date;
        keyboard_ = keyboard;
        refresh();
    }

    const LocalDate& focusedDate() const { return focused_; }

    // The cells, made again by the factory, and the day names, in the picker's order.
    void rebuildCells() {
        grid_->getChildren().clear();
        cells_.clear();
        weekLabels_.clear();
        const int offset = picker_.showWeekNumbers_ ? 1 : 0;
        for (int column = 0; column < kDays; ++column) {
            const DayOfWeek day =
                static_cast<DayOfWeek>((static_cast<int>(picker_.firstDay_) - 1 + column) % kDays + 1);
            auto name = CenteredLabel("day-name");
            name->setText(DayName(day));
            name->setPrefWidth(kCellWidth);
            grid_->add(name, column + offset, 0);
        }
        for (int week = 0; week < kWeeks; ++week) {
            if (offset > 0) {
                auto number = CenteredLabel("week-number");
                number->setPrefSize(kCellWidth * 0.75, kCellHeight);
                grid_->add(number, 0, week + 1);
                weekLabels_.push_back(number);
            }
            for (int column = 0; column < kDays; ++column) {
                std::shared_ptr<DateCell> cell = picker_.cellFactory_ ? picker_.cellFactory_() : nullptr;
                if (!cell) {
                    cell = std::make_shared<DateCell>();
                }
                cell->choose_ = [this](const LocalDate& date) { picker_.choose(date); };
                grid_->add(cell, column + offset, week + 1);
                cells_.push_back(std::move(cell));
            }
        }
        refresh();
    }

    void refresh() {
        const LocalDate first = focused_.withDayOfMonth(1);
        month_->setText(MonthName(first.getMonthValue()));
        year_->setText(std::to_string(first.getYear()));
        const LocalDate start = first.minusDays(ColumnOf(first.getDayOfWeek(), picker_.firstDay_));
        const LocalDate today = LocalDate::now();
        for (std::size_t i = 0; i < cells_.size(); ++i) {
            const LocalDate date = start.plusDays(static_cast<long long>(i));
            DateCell& cell = *cells_[i];
            ObservableList<std::string>& classes = cell.getClassList();
            classes.removeIf([](const std::string& name) {
                return name == "today" || name == "previous-month" || name == "next-month";
            });
            if (date == today) {
                classes.add("today");
            }
            if (date.getMonthValue() != first.getMonthValue()) {
                classes.add(date < first ? "previous-month" : "next-month");
            }
            cell.setSelected(picker_.value_ && *picker_.value_ == date);
            cell.setPseudoState("focus-visible", keyboard_ && date == focused_);
            cell.bind(date);
        }
        for (std::size_t week = 0; week < weekLabels_.size(); ++week) {
            weekLabels_[week]->setText(std::to_string(start.plusWeeks(static_cast<long long>(week)).plusDays(3).getIsoWeek()));
        }
    }

    // Moves the keyboard's day, paging the calendar when it leaves the month.
    void moveFocus(long long days, long long months) {
        focused_ = focused_.plusMonths(months).plusDays(days);
        keyboard_ = true;
        refresh();
    }

    // The day the keyboard is on, unless its cell is disabled.
    std::optional<LocalDate> focusedChoice() const {
        for (const std::shared_ptr<DateCell>& cell : cells_) {
            if (cell->getItem() && *cell->getItem() == focused_) {
                return cell->isDisabled() ? std::nullopt : std::optional<LocalDate>(focused_);
            }
        }
        return std::nullopt;
    }

protected:
    // The header spans the days, which set the width.
    void layoutChildren() override {
        const double headerHeight = header_->measuredHeight(contentWidth(), -1);
        header_->performLayout(contentLeft(), contentTop(), contentWidth(), headerHeight);
        const double gridTop = contentTop() + headerHeight + kHeaderGap;
        grid_->performLayout(contentLeft(), gridTop, contentWidth(), std::max(0.0, contentHeight() - headerHeight - kHeaderGap));
    }
    double preferredContentWidth(double innerAvailable) const override {
        return std::max(grid_->measuredWidth(innerAvailable), header_->measuredWidth(innerAvailable));
    }
    double preferredContentHeight(double innerWidth) const override {
        return header_->measuredHeight(innerWidth, -1) + kHeaderGap + grid_->measuredHeight(innerWidth, -1);
    }

private:
    void page(long long months) {
        focused_ = focused_.plusMonths(months);
        refresh();
    }

    DatePicker& picker_;
    std::shared_ptr<GridPane> header_;
    std::shared_ptr<GridPane> grid_;
    std::shared_ptr<Label> month_;
    std::shared_ptr<Label> year_;
    std::vector<std::shared_ptr<DateCell>> cells_;
    std::vector<std::shared_ptr<Label>> weekLabels_;
    LocalDate focused_ = LocalDate::now();
    bool keyboard_ = false;
};

DatePicker::DatePicker() : DatePicker(std::nullopt) {}

DatePicker::DatePicker(std::optional<LocalDate> value) : value_(value) {
    setEditable(true);
}

DatePicker::~DatePicker() = default;

void DatePicker::setValue(std::optional<LocalDate> value) {
    if (value == value_) {
        return;
    }
    value_ = value;
    changed(false);
}

void DatePicker::changed(bool fire) {
    syncEditor();
    if (content_ != nullptr && value_) {
        content_->showDate(*value_, false);
    } else if (content_ != nullptr) {
        content_->refresh();
    }
    if (onChanged_) {
        onChanged_();
    }
    if (fire) {
        fireAction();
    }
}

void DatePicker::setConverter(StringConverter<LocalDate> converter) {
    converter_ = std::move(converter);
    syncEditor();
}

void DatePicker::setShowWeekNumbers(bool show) {
    showWeekNumbers_ = show;
    if (content_ != nullptr) {
        content_->rebuildCells();
    }
}

void DatePicker::setFirstDayOfWeek(DayOfWeek day) {
    firstDay_ = day;
    if (content_ != nullptr) {
        content_->rebuildCells();
    }
}

void DatePicker::setDayCellFactory(std::function<std::shared_ptr<DateCell>()> factory) {
    cellFactory_ = std::move(factory);
    if (content_ != nullptr) {
        content_->rebuildCells();
    }
}

std::shared_ptr<Node> DatePicker::createPopupContent() {
    content_ = std::make_shared<DatePickerContent>(*this);
    return content_;
}

void DatePicker::popupShowing() {
    if (!isShowing()) {
        // Text typed before opening decides which month opens.
        commitEditorText();
        content_->showDate(value_.value_or(LocalDate::now()), false);
    }
}

bool DatePicker::handlePopupKey(KeyEvent& event) {
    if (!event.pressed && !event.repeat) {
        return false;
    }
    switch (event.key) {
        case Key::Left: content_->moveFocus(-1, 0); return true;
        case Key::Right: content_->moveFocus(1, 0); return true;
        case Key::Up: content_->moveFocus(-7, 0); return true;
        case Key::Down: content_->moveFocus(7, 0); return true;
        case Key::PageUp: content_->moveFocus(0, event.shift ? -12 : -1); return true;
        case Key::PageDown: content_->moveFocus(0, event.shift ? 12 : 1); return true;
        case Key::Enter:
        case Key::KpEnter: {
            // Text typed in the field wins over the calendar's day.
            TextField* editor = getEditor();
            if (editor != nullptr && editor->getText() != valueText()) {
                return false;
            }
            if (std::optional<LocalDate> day = content_->focusedChoice()) {
                choose(*day);
            }
            return true;
        }
        default: return false;
    }
}

void DatePicker::choose(const LocalDate& date) {
    const bool different = !value_ || *value_ != date;
    value_ = date;
    hide();
    // Back to the field, as the calendar's cells do not take the focus.
    if (TextField* editor = getEditor()) {
        editor->requestFocus();
    } else {
        requestFocus();
    }
    if (different) {
        changed(true);
    } else {
        syncEditor();
    }
}

std::string DatePicker::valueText() const { return value_ ? converter_.format(*value_) : std::string(); }

void DatePicker::editorFocusLost() { commitEditorText(); }

void DatePicker::editorAction() {
    commitEditorText();
    hide();
}

bool DatePicker::commitEditorText() {
    TextField* editor = getEditor();
    if (editor == nullptr || editor->getText() == valueText()) {
        return true;
    }
    std::optional<LocalDate> typed;
    if (!editor->getText().empty()) {
        typed = converter_.parse(editor->getText());
        if (!typed) {
            syncEditor();
            return false;
        }
    }
    if (typed == value_) {
        syncEditor();
        return true;
    }
    value_ = typed;
    changed(true);
    return true;
}

double DatePicker::preferredContentWidth(double) const {
    const Font font = chrome::FontOf(*this);
    // A wide date in the current format, so the box does not change size with the value.
    const double text = std::max(font.measureWidth(converter_.format(LocalDate::of(2000, 12, 28))),
                                 font.measureWidth(getPromptText()));
    return std::max(120.0, text + 12.0) + kArrowWidth;
}

}  // namespace jadefx

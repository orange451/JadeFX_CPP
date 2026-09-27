#pragma once

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

namespace jadefx {

// ISO days of the week, Monday first, as java.time.DayOfWeek.
enum class DayOfWeek { Monday = 1, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday };

// A date without a time or zone, in the proleptic Gregorian calendar, in the
// shape of java.time.LocalDate. Months and days count from 1. Arithmetic that
// lands past the end of a month, such as January 31 plus one month, clamps to
// the month's last day. Text is ISO 8601, yyyy-mm-dd, the value of an HTML
// date input; operator<< and operator>> use it, so StringConverter does too.
class LocalDate {
public:
    // 1970-01-01.
    LocalDate() = default;

    // month is clamped to 1-12 and day to the month's length.
    static LocalDate of(int year, int month, int day);
    static LocalDate ofEpochDay(long long epochDay);
    // Today in the local time zone.
    static LocalDate now();
    // yyyy-mm-dd, or nothing for text that is not a real date.
    static std::optional<LocalDate> parse(std::string_view text);

    static bool isLeapYear(int year);
    static int lengthOfMonth(int year, int month);

    int getYear() const { return year_; }
    int getMonthValue() const { return month_; }
    int getDayOfMonth() const { return day_; }
    DayOfWeek getDayOfWeek() const;
    int getDayOfYear() const;
    bool isLeapYear() const { return isLeapYear(year_); }
    int lengthOfMonth() const { return lengthOfMonth(year_, month_); }
    long long toEpochDay() const;
    // The ISO week of the year, 1 to 53: weeks start on Monday, and week 1 holds the year's first Thursday.
    int getIsoWeek() const;

    LocalDate plusDays(long long days) const;
    LocalDate plusWeeks(long long weeks) const { return plusDays(weeks * 7); }
    LocalDate plusMonths(long long months) const;
    LocalDate plusYears(long long years) const { return plusMonths(years * 12); }
    LocalDate minusDays(long long days) const { return plusDays(-days); }
    LocalDate minusMonths(long long months) const { return plusMonths(-months); }
    LocalDate withDayOfMonth(int day) const { return of(year_, month_, day); }

    bool isBefore(const LocalDate& other) const { return toEpochDay() < other.toEpochDay(); }
    bool isAfter(const LocalDate& other) const { return toEpochDay() > other.toEpochDay(); }

    // yyyy-mm-dd.
    std::string toString() const;

    bool operator==(const LocalDate& other) const {
        return year_ == other.year_ && month_ == other.month_ && day_ == other.day_;
    }
    bool operator!=(const LocalDate& other) const { return !(*this == other); }
    bool operator<(const LocalDate& other) const { return isBefore(other); }

private:
    LocalDate(int year, int month, int day) : year_(year), month_(month), day_(day) {}

    int year_ = 1970;
    int month_ = 1;
    int day_ = 1;
};

std::ostream& operator<<(std::ostream& out, const LocalDate& date);
// Reads one yyyy-mm-dd word, and fails the stream when it is not a date.
std::istream& operator>>(std::istream& in, LocalDate& date);

}  // namespace jadefx

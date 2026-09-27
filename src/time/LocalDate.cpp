#include "jadefx/time/LocalDate.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <istream>
#include <ostream>

namespace jadefx {
namespace {

// Floor division, so days before 1970 land in the right era.
long long FloorDiv(long long a, long long b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0)) ? 1 : 0); }

bool Digits(std::string_view text, std::size_t from, std::size_t count, int& value) {
    if (from + count > text.size()) {
        return false;
    }
    value = 0;
    for (std::size_t i = from; i < from + count; ++i) {
        if (std::isdigit(static_cast<unsigned char>(text[i])) == 0) {
            return false;
        }
        value = value * 10 + (text[i] - '0');
    }
    return true;
}

}  // namespace

bool LocalDate::isLeapYear(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

int LocalDate::lengthOfMonth(int year, int month) {
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    month = std::clamp(month, 1, 12);
    return month == 2 && isLeapYear(year) ? 29 : kDays[month - 1];
}

LocalDate LocalDate::of(int year, int month, int day) {
    month = std::clamp(month, 1, 12);
    return LocalDate(year, month, std::clamp(day, 1, lengthOfMonth(year, month)));
}

// Howard Hinnant's civil-from-days and days-from-civil.
LocalDate LocalDate::ofEpochDay(long long epochDay) {
    const long long z = epochDay + 719468;
    const long long era = FloorDiv(z, 146097);
    const long long doe = z - era * 146097;
    const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long long mp = (5 * doy + 2) / 153;
    const int day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    const int month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    const int year = static_cast<int>(yoe + era * 400 + (month <= 2 ? 1 : 0));
    return LocalDate(year, month, day);
}

long long LocalDate::toEpochDay() const {
    const long long year = year_ - (month_ <= 2 ? 1 : 0);
    const long long era = FloorDiv(year, 400);
    const long long yoe = year - era * 400;
    const long long mp = month_ > 2 ? month_ - 3 : month_ + 9;
    const long long doy = (153 * mp + 2) / 5 + day_ - 1;
    const long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

LocalDate LocalDate::now() {
    const std::time_t seconds = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    return of(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

std::optional<LocalDate> LocalDate::parse(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    int year = 0;
    int month = 0;
    int day = 0;
    if (text.size() != 10 || text[4] != '-' || text[7] != '-' || !Digits(text, 0, 4, year) ||
        !Digits(text, 5, 2, month) || !Digits(text, 8, 2, day)) {
        return std::nullopt;
    }
    if (month < 1 || month > 12 || day < 1 || day > lengthOfMonth(year, month)) {
        return std::nullopt;
    }
    return LocalDate(year, month, day);
}

DayOfWeek LocalDate::getDayOfWeek() const {
    // 1970-01-01 was a Thursday.
    const long long index = ((toEpochDay() + 3) % 7 + 7) % 7;
    return static_cast<DayOfWeek>(index + 1);
}

int LocalDate::getDayOfYear() const { return static_cast<int>(toEpochDay() - of(year_, 1, 1).toEpochDay()) + 1; }

int LocalDate::getIsoWeek() const {
    // The week's Thursday decides the week's year.
    const LocalDate thursday = plusDays(4 - static_cast<int>(getDayOfWeek()));
    return (thursday.getDayOfYear() - 1) / 7 + 1;
}

LocalDate LocalDate::plusDays(long long days) const { return days == 0 ? *this : ofEpochDay(toEpochDay() + days); }

LocalDate LocalDate::plusMonths(long long months) const {
    const long long total = static_cast<long long>(year_) * 12 + (month_ - 1) + months;
    const int year = static_cast<int>(FloorDiv(total, 12));
    const int month = static_cast<int>(total - static_cast<long long>(year) * 12) + 1;
    return of(year, month, day_);
}

std::string LocalDate::toString() const {
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year_, month_, day_);
    return buffer;
}

std::ostream& operator<<(std::ostream& out, const LocalDate& date) { return out << date.toString(); }

std::istream& operator>>(std::istream& in, LocalDate& date) {
    std::string word;
    if (in >> word) {
        if (std::optional<LocalDate> parsed = LocalDate::parse(word)) {
            date = *parsed;
        } else {
            in.setstate(std::ios::failbit);
        }
    }
    return in;
}

}  // namespace jadefx

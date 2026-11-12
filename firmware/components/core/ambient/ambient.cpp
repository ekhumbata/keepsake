#include "ambient/ambient.h"

#include <cstdio>

namespace ambient {

bool IsNight(const NightHours& hours, int hour, int minute)
{
    (void)minute;
    if (hours.start_hour == hours.end_hour) {
        return false;
    }
    if (hours.start_hour < hours.end_hour) {
        return hour >= hours.start_hour && hour < hours.end_hour;
    }
    return hour >= hours.start_hour || hour < hours.end_hour;
}

std::string ClockText(int hour, int minute, bool twelve_hour)
{
    hour = ((hour % 24) + 24) % 24;
    minute = ((minute % 60) + 60) % 60;
    if (twelve_hour) {
        hour = hour % 12;
        if (hour == 0) {
            hour = 12;
        }
    }
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%d:%02d", hour, minute);
    return buffer;
}

int DaysInMonth(int year, int month)
{
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 0;
    }
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return kDays[month - 1] + (month == 2 && leap ? 1 : 0);
}

int Weekday(int year, int month, int day)
{
    // Sakamoto's method: 0 = Sunday; shifted to Monday-first.
    static constexpr int kOffsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) {
        year -= 1;
    }
    const int sunday_first =
        (year + year / 4 - year / 100 + year / 400 + kOffsets[(month - 1) % 12] + day) % 7;
    return (sunday_first + 6) % 7;
}

MonthGrid LayOutMonth(int year, int month)
{
    MonthGrid grid;
    const int days = DaysInMonth(year, month);
    if (days == 0) {
        return grid;
    }
    int column = Weekday(year, month, 1);
    int row = 0;
    for (int day = 1; day <= days; ++day) {
        grid.cells[static_cast<size_t>(row)][static_cast<size_t>(column)] = day;
        if (++column == 7 && day < days) {
            column = 0;
            ++row;
        }
    }
    grid.rows = row + 1;
    return grid;
}

const char* MonthName(int month)
{
    static constexpr const char* kNames[] = {"January", "February", "March",     "April",
                                             "May",     "June",     "July",      "August",
                                             "September", "October", "November", "December"};
    return month >= 1 && month <= 12 ? kNames[month - 1] : "";
}

}  // namespace ambient

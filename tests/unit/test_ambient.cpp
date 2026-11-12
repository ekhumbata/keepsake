// The ambient lock screen (#38): night hours, the clock, and the month grid.
#include <doctest/doctest.h>

#include <string>

#include "ambient/ambient.h"

using namespace ambient;

TEST_CASE("night hours, across midnight or not")
{
    const NightHours night = {};  // 23:00-07:00
    CHECK(IsNight(night, 23, 0));
    CHECK(IsNight(night, 2, 30));
    CHECK(IsNight(night, 6, 59));
    CHECK_FALSE(IsNight(night, 7, 0));
    CHECK_FALSE(IsNight(night, 22, 59));
    const NightHours afternoon = {13, 15};
    CHECK(IsNight(afternoon, 14, 0));
    CHECK_FALSE(IsNight(afternoon, 15, 0));
    CHECK_FALSE(IsNight({7, 7}, 3, 0));
}

TEST_CASE("the clock")
{
    CHECK(ClockText(9, 41, false) == "9:41");
    CHECK(ClockText(21, 5, false) == "21:05");
    CHECK(ClockText(21, 5, true) == "9:05");
    CHECK(ClockText(0, 0, true) == "12:00");
    CHECK(ClockText(12, 30, true) == "12:30");
    CHECK(ClockText(0, 7, false) == "0:07");
}

TEST_CASE("weekdays and month lengths")
{
    CHECK(Weekday(2026, 11, 11) == 2);  // a Wednesday
    CHECK(Weekday(2000, 1, 1) == 5);    // a Saturday
    CHECK(Weekday(2024, 2, 29) == 3);   // a Thursday
    CHECK(DaysInMonth(2024, 2) == 29);
    CHECK(DaysInMonth(2100, 2) == 28);
    CHECK(DaysInMonth(2000, 2) == 29);
    CHECK(DaysInMonth(2026, 13) == 0);
    CHECK(std::string(MonthName(11)) == "November");
}

TEST_CASE("a month lays out Monday-first")
{
    const MonthGrid november = LayOutMonth(2026, 11);  // starts on a Sunday
    CHECK(november.rows == 6);
    CHECK(november.cells[0][6] == 1);
    CHECK(november.cells[0][5] == 0);
    CHECK(november.cells[1][0] == 2);
    CHECK(november.cells[5][0] == 30);
    const MonthGrid february = LayOutMonth(2027, 2);  // starts on a Monday, 28 days
    CHECK(february.rows == 4);
    CHECK(february.cells[0][0] == 1);
    CHECK(february.cells[3][6] == 28);
    CHECK(LayOutMonth(2026, 0).rows == 0);
}

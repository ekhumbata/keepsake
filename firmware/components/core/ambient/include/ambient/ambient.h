#ifndef AMBIENT_AMBIENT_H_
#define AMBIENT_AMBIENT_H_

#include <array>
#include <string>

// The ambient lock screen (#38): what a resting Keepsake shows. These are the rules that don't
// need a screen: when it's night, how the clock reads, and how a month lays out.
namespace ambient {

struct NightHours {
    int start_hour = 23;  // 0..23
    int end_hour = 7;     // 0..23; equal to start means "never night"
};

// True between start_hour:00 (inclusive) and end_hour:00 (exclusive), across midnight.
bool IsNight(const NightHours& hours, int hour, int minute);

// "9:41" / "21:41" (24 h), or "9:41" with twelve_hour (no am/pm: the date and light say it).
std::string ClockText(int hour, int minute, bool twelve_hour);

// A month laid out Monday-first: up to 6 rows of 7 cells; 0 = empty cell, else the day.
struct MonthGrid {
    std::array<std::array<int, 7>, 6> cells = {};
    int rows = 0;
};
// month 1..12. Weekday of the 1st is computed (proleptic Gregorian).
MonthGrid LayOutMonth(int year, int month);
int DaysInMonth(int year, int month);
// 0 = Monday .. 6 = Sunday.
int Weekday(int year, int month, int day);
const char* MonthName(int month);  // "November"

}  // namespace ambient

#endif  // AMBIENT_AMBIENT_H_

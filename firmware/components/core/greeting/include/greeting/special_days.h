#ifndef GREETING_SPECIAL_DAYS_H_
#define GREETING_SPECIAL_DAYS_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Special days (#85): birthdays, holidays, the anniversary, the first date, and any days Eric
// adds. Each has a short Home title, a line for the card, and a mascot picture. The dates come
// from the provisioning file (#42) on the device, never from the repo.
namespace greeting {

enum class Mascot : uint8_t {
    kCelebrate,   // Mochi with sparkles (custom days)
    kBirthday,    // Mochi in a party hat
    kChristmas,   // Mochi in a Santa hat, snow
    kNewYear,     // Mochi under fireworks
    kValentines,  // Mochi with hearts
    kAnniversary, // Mochi and Bookworm, a heart between them
    kFirstDate,   // Bookworm with a flower
    kHalloween,   // Mochi with a pumpkin and bats
};

// Which holidays to celebrate (a bit set). All on unless the provisioning file says otherwise.
enum Holiday : uint8_t {
    kHolidayChristmas = 1 << 0,
    kHolidayNewYear = 1 << 1,
    kHolidayValentines = 1 << 2,
    kHolidayHalloween = 1 << 3,
};
inline constexpr uint8_t kAllHolidays = kHolidayChristmas | kHolidayNewYear | kHolidayValentines |
                                        kHolidayHalloween;

// A day Eric adds: "MM-DD" (every year) or "YYYY-MM-DD" (counts years since), a title and a line.
struct CustomDay {
    std::string date;
    std::string title;
    std::string message;
    bool operator==(const CustomDay& other) const = default;
};

struct People {
    std::string name;         // the friend's name, may be empty
    std::string birthday;     // "MM-DD"
    std::string anniversary;  // "MM-DD" or "YYYY-MM-DD"
    std::string first_date;   // "MM-DD" or "YYYY-MM-DD"
    uint8_t holidays = kAllHolidays;
    std::vector<CustomDay> custom_days;
};

struct Date {
    int year = 0;
    int month = 0;  // 1..12
    int day = 0;    // 1..31
};

struct Occasion {
    std::string key;      // stable id, e.g. "birthday", "christmas", "custom:1": for "shown once"
    std::string title;    // Home's title for the day, e.g. "Happy birthday, Sam!"
    std::string message;  // the card's line
    Mascot mascot = Mascot::kCelebrate;
};

// Everything to celebrate today, most personal first (birthday, anniversary, first date,
// Eric's days, then holidays). Empty on an ordinary day.
std::vector<Occasion> OccasionsOn(const People& people, const Date& today);

// "1st", "2nd", "3rd", "4th", "11th", "21st" ...
std::string Ordinal(int number);

// "MM-DD" or "YYYY-MM-DD" -> month, day and (if given) year; false if it isn't a real date.
bool ParseDay(std::string_view text, int* year, int* month, int* day);

// Custom days as one line each (date, title, message; tab-separated, escaped) for NVS.
std::string SerializeCustomDays(const std::vector<CustomDay>& days);
std::vector<CustomDay> ParseCustomDays(std::string_view text);

}  // namespace greeting

#endif  // GREETING_SPECIAL_DAYS_H_

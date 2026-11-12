#include "greeting/special_days.h"

#include "greeting/greeting.h"

namespace greeting {
namespace {

bool IsLeap(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int DaysIn(int month, int year)
{
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 && (year == 0 || IsLeap(year)) ? 29 : kDays[month - 1];
}

int Number(std::string_view digits)
{
    int value = 0;
    for (const char c : digits) {
        if (c < '0' || c > '9') {
            return -1;
        }
        value = value * 10 + (c - '0');
    }
    return digits.empty() ? -1 : value;
}

// The day falls today: same month and day (Feb 29 -> Feb 28 in other years).
bool FallsOn(int month, int day, const Date& today)
{
    if (month == today.month && day == today.day) {
        return true;
    }
    return month == 2 && day == 29 && !IsLeap(today.year) && today.month == 2 && today.day == 28;
}

std::string WithName(std::string_view greeting, std::string_view name, std::string_view end)
{
    return std::string(greeting) + (name.empty() ? "" : ", " + std::string(name)) +
           std::string(end);
}

std::string Escape(std::string_view text)
{
    std::string out;
    for (const char c : text) {
        if (c == '\\') out += "\\\\";
        else if (c == '\t') out += "\\t";
        else if (c == '\n') out += "\\n";
        else out.push_back(c);
    }
    return out;
}

std::string Unescape(std::string_view text)
{
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            const char next = text[++i];
            out.push_back(next == 't' ? '\t' : next == 'n' ? '\n' : next);
        } else {
            out.push_back(text[i]);
        }
    }
    return out;
}

}  // namespace

bool ParseDay(std::string_view text, int* year, int* month, int* day)
{
    int y = 0;
    std::string_view month_day = text;
    if (text.size() == 10 && text[4] == '-') {
        y = Number(text.substr(0, 4));
        if (y < 1900 || y > 2200) {
            return false;
        }
        month_day = text.substr(5);
    }
    if (month_day.size() != 5 || month_day[2] != '-') {
        return false;
    }
    const int m = Number(month_day.substr(0, 2));
    const int d = Number(month_day.substr(3, 2));
    if (m < 1 || m > 12 || d < 1 || d > DaysIn(m, y)) {
        return false;
    }
    *year = y;
    *month = m;
    *day = d;
    return true;
}

std::string Ordinal(int number)
{
    const int last_two = number % 100;
    const char* suffix = "th";
    if (last_two < 11 || last_two > 13) {
        switch (number % 10) {
            case 1: suffix = "st"; break;
            case 2: suffix = "nd"; break;
            case 3: suffix = "rd"; break;
            default: break;
        }
    }
    return std::to_string(number) + suffix;
}

std::vector<Occasion> OccasionsOn(const People& people, const Date& today)
{
    std::vector<Occasion> out;
    if (today.month < 1 || today.month > 12 || today.day < 1) {
        return out;
    }
    int year = 0;
    int month = 0;
    int day = 0;
    if (ParseDay(people.birthday, &year, &month, &day) && FallsOn(month, day, today)) {
        out.push_back({"birthday", WithName("Happy birthday", people.name, "!"),
                       "Wishing you a cozy day, cake, and a good book.", Mascot::kBirthday});
    }
    if (ParseDay(people.anniversary, &year, &month, &day) && FallsOn(month, day, today)) {
        const int years = year > 0 ? today.year - year : 0;
        out.push_back({"anniversary",
                       years > 0 ? "Happy " + Ordinal(years) + " anniversary!" : "Happy anniversary!",
                       "Here's to the two of you, and many more.", Mascot::kAnniversary});
    }
    if (ParseDay(people.first_date, &year, &month, &day) && FallsOn(month, day, today)) {
        const int years = year > 0 ? today.year - year : 0;
        out.push_back({"first_date",
                       years == 1   ? std::string("One year since your first date")
                       : years > 1  ? std::to_string(years) + " years since your first date"
                                    : std::string("Your first-date day"),
                       "Still the best idea anyone ever had.", Mascot::kFirstDate});
    }
    for (size_t i = 0; i < people.custom_days.size(); ++i) {
        const CustomDay& custom = people.custom_days[i];
        if (ParseDay(custom.date, &year, &month, &day) && FallsOn(month, day, today) &&
            !custom.title.empty()) {
            out.push_back({"custom:" + std::to_string(i), custom.title, custom.message,
                           Mascot::kCelebrate});
        }
    }
    const auto holiday = [&](uint8_t bit, int m, int d, const char* key, std::string title,
                             const char* message, Mascot mascot) {
        if ((people.holidays & bit) && today.month == m && today.day == d) {
            out.push_back({key, std::move(title), message, mascot});
        }
    };
    holiday(kHolidayChristmas, 12, 25, "christmas", WithName("Merry Christmas", people.name, "!"),
            "Warm socks, hot cocoa and a good book.", Mascot::kChristmas);
    holiday(kHolidayNewYear, 1, 1, "new_year", WithName("Happy New Year", people.name, "!"),
            "Here's to a year full of good stories.", Mascot::kNewYear);
    holiday(kHolidayValentines, 2, 14, "valentines", "Happy Valentine's Day!",
            "You are loved, today and every day.", Mascot::kValentines);
    holiday(kHolidayHalloween, 10, 31, "halloween", "Happy Halloween!",
            "Something a little spooky to read tonight?", Mascot::kHalloween);
    return out;
}

std::string SerializeCustomDays(const std::vector<CustomDay>& days)
{
    std::string out;
    for (const CustomDay& day : days) {
        out += Escape(day.date) + "\t" + Escape(day.title) + "\t" + Escape(day.message) + "\n";
    }
    return out;
}

std::vector<CustomDay> ParseCustomDays(std::string_view text)
{
    std::vector<CustomDay> days;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        const std::string_view line = text.substr(start, end - start);
        start = end + 1;
        const size_t tab1 = line.find('\t');
        const size_t tab2 = tab1 == std::string_view::npos ? tab1 : line.find('\t', tab1 + 1);
        if (tab2 == std::string_view::npos) {
            continue;
        }
        CustomDay day{Unescape(line.substr(0, tab1)), Unescape(line.substr(tab1 + 1, tab2 - tab1 - 1)),
                      Unescape(line.substr(tab2 + 1))};
        int y = 0, m = 0, d = 0;
        if (ParseDay(day.date, &y, &m, &d) && !day.title.empty()) {
            days.push_back(std::move(day));
        }
    }
    return days;
}

}  // namespace greeting

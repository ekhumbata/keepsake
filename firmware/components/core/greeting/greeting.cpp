#include "greeting/greeting.h"

namespace greeting {

bool IsBirthday(std::string_view birthday, const Today& today, bool leap_year)
{
    if (!today.known || birthday.size() != 5 || birthday[2] != '-') {
        return false;
    }
    const auto digits = [](std::string_view two) {
        return (two[0] >= '0' && two[0] <= '9' && two[1] >= '0' && two[1] <= '9')
                   ? (two[0] - '0') * 10 + (two[1] - '0')
                   : -1;
    };
    const int month = digits(birthday.substr(0, 2));
    const int day = digits(birthday.substr(3, 2));
    if (month == today.month && day == today.day) {
        return true;
    }
    // Born on Feb 29: celebrate on Feb 28 when there's no 29th.
    return month == 2 && day == 29 && !leap_year && today.month == 2 && today.day == 28;
}

std::string_view TimeOfDay(int hour)
{
    if (hour >= 5 && hour < 12) {
        return "Good morning";
    }
    if (hour >= 12 && hour < 17) {
        return "Good afternoon";
    }
    if (hour >= 17 && hour < 22) {
        return "Good evening";
    }
    return "Hello";
}

std::string HomeTitle(std::string_view name, std::string_view birthday, const Today& today,
                      bool leap_year, uint32_t rotation, std::string_view prompt)
{
    if (IsBirthday(birthday, today, leap_year)) {
        return name.empty() ? "Happy birthday!" : "Happy birthday, " + std::string(name) + "!";
    }
    if (name.empty() || rotation % 2 == 1) {
        return std::string(prompt);
    }
    const std::string_view hello = today.known ? TimeOfDay(today.hour) : "Hello";
    return std::string(hello) + ", " + std::string(name);
}

}  // namespace greeting

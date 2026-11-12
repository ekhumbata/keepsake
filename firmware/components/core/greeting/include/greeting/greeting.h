#ifndef GREETING_GREETING_H_
#define GREETING_GREETING_H_

#include <cstdint>
#include <string>
#include <string_view>

// Personal touches (#85): what Home says at the top. The name and birthday come from the
// provisioning file (#42) on the device; with neither set, Home keeps its usual prompts.
namespace greeting {

struct Today {
    bool known = false;  // false until the clock is set
    int month = 0;       // 1..12
    int day = 0;         // 1..31
    int hour = 0;        // 0..23
};

// "MM-DD" matches today's month and day. Feb 29 birthdays are celebrated on Feb 28 in
// other years.
bool IsBirthday(std::string_view birthday, const Today& today, bool leap_year);

// "Good morning" (5-11), "Good afternoon" (12-16), "Good evening" (17-21), "Hello" otherwise.
std::string_view TimeOfDay(int hour);

// The title at the top of Home. On the birthday: "Happy birthday, Sam!". With a name, every
// other rotation step greets them ("Good evening, Sam") and the rest use `prompt` (the usual
// "What's on your mind?" lines); without a name, always `prompt`.
std::string HomeTitle(std::string_view name, std::string_view birthday, const Today& today,
                      bool leap_year, uint32_t rotation, std::string_view prompt);

}  // namespace greeting

#endif  // GREETING_GREETING_H_

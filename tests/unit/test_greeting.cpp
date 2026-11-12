// Personal touches (#85): the Home greeting and the birthday.
#include <doctest/doctest.h>

#include "greeting/greeting.h"

using greeting::Today;

TEST_CASE("birthdays match the day, with Feb 29 on Feb 28 in other years")
{
    const Today july14{true, 7, 14, 10};
    CHECK(greeting::IsBirthday("07-14", july14, false));
    CHECK_FALSE(greeting::IsBirthday("07-15", july14, false));
    CHECK_FALSE(greeting::IsBirthday("", july14, false));
    CHECK_FALSE(greeting::IsBirthday("7-14", july14, false));
    CHECK_FALSE(greeting::IsBirthday("07-14", Today{}, false));  // clock not set
    const Today feb28{true, 2, 28, 9};
    CHECK(greeting::IsBirthday("02-29", feb28, false));
    CHECK_FALSE(greeting::IsBirthday("02-29", feb28, true));
    CHECK(greeting::IsBirthday("02-29", Today{true, 2, 29, 9}, true));
}

TEST_CASE("the Home title greets by name every other step, and celebrates birthdays")
{
    const Today evening{true, 3, 1, 19};
    CHECK(greeting::HomeTitle("Sam", "07-14", evening, false, 0, "What's on your mind?") ==
          "Good evening, Sam");
    CHECK(greeting::HomeTitle("Sam", "07-14", evening, false, 1, "What's on your mind?") ==
          "What's on your mind?");
    CHECK(greeting::HomeTitle("", "", evening, false, 0, "Got a cool idea?") == "Got a cool idea?");
    CHECK(greeting::HomeTitle("Sam", "03-01", evening, false, 1, "x") == "Happy birthday, Sam!");
    CHECK(greeting::HomeTitle("", "03-01", evening, false, 0, "x") == "Happy birthday!");
    CHECK(greeting::HomeTitle("Sam", "", Today{}, false, 2, "x") == "Hello, Sam");
    CHECK(greeting::TimeOfDay(5) == "Good morning");
    CHECK(greeting::TimeOfDay(12) == "Good afternoon");
    CHECK(greeting::TimeOfDay(23) == "Hello");
}

// Special days (#85): which days to celebrate, what they say, and the stored custom days.
#include <doctest/doctest.h>

#include "greeting/special_days.h"

using greeting::Date;
using greeting::Mascot;
using greeting::People;

namespace {

People Sam()
{
    People p;
    p.name = "Sam";
    p.birthday = "07-14";
    p.anniversary = "2021-09-03";
    p.first_date = "2019-05-20";
    p.custom_days = {{"03-17", "Happy Gotcha Day, Biscuit!", "Treats for the dog today."},
                     {"2023-06-01", "Moving-in day", "Home sweet home."}};
    return p;
}

}  // namespace

TEST_CASE("ordinary days have nothing to celebrate")
{
    CHECK(greeting::OccasionsOn(Sam(), Date{2026, 4, 2}).empty());
    CHECK(greeting::OccasionsOn(Sam(), Date{}).empty());
}

TEST_CASE("personal days count the years when the year is known")
{
    auto birthday = greeting::OccasionsOn(Sam(), Date{2026, 7, 14});
    REQUIRE(birthday.size() == 1);
    CHECK(birthday[0].title == "Happy birthday, Sam!");
    CHECK(birthday[0].mascot == Mascot::kBirthday);
    CHECK(birthday[0].key == "birthday");

    auto anniversary = greeting::OccasionsOn(Sam(), Date{2026, 9, 3});
    REQUIRE(anniversary.size() == 1);
    CHECK(anniversary[0].title == "Happy 5th anniversary!");
    CHECK(anniversary[0].mascot == Mascot::kAnniversary);

    auto first = greeting::OccasionsOn(Sam(), Date{2026, 5, 20});
    REQUIRE(first.size() == 1);
    CHECK(first[0].title == "7 years since your first date");
    CHECK(first[0].mascot == Mascot::kFirstDate);
    CHECK(greeting::OccasionsOn(Sam(), Date{2020, 5, 20})[0].title == "One year since your first date");

    People no_years = Sam();
    no_years.anniversary = "09-03";
    CHECK(greeting::OccasionsOn(no_years, Date{2026, 9, 3})[0].title == "Happy anniversary!");
}

TEST_CASE("Eric's own days and holidays")
{
    auto gotcha = greeting::OccasionsOn(Sam(), Date{2026, 3, 17});
    REQUIRE(gotcha.size() == 1);
    CHECK(gotcha[0].title == "Happy Gotcha Day, Biscuit!");
    CHECK(gotcha[0].message == "Treats for the dog today.");
    CHECK(gotcha[0].key == "custom:0");
    CHECK(gotcha[0].mascot == Mascot::kCelebrate);

    auto xmas = greeting::OccasionsOn(Sam(), Date{2026, 12, 25});
    REQUIRE(xmas.size() == 1);
    CHECK(xmas[0].title == "Merry Christmas, Sam!");
    CHECK(xmas[0].mascot == Mascot::kChristmas);
    CHECK(greeting::OccasionsOn(Sam(), Date{2027, 1, 1})[0].mascot == Mascot::kNewYear);
    CHECK(greeting::OccasionsOn(Sam(), Date{2027, 2, 14})[0].title == "Happy Valentine's Day!");
    CHECK(greeting::OccasionsOn(Sam(), Date{2026, 10, 31})[0].mascot == Mascot::kHalloween);

    People no_halloween = Sam();
    no_halloween.holidays = greeting::kAllHolidays & ~greeting::kHolidayHalloween;
    CHECK(greeting::OccasionsOn(no_halloween, Date{2026, 10, 31}).empty());
}

TEST_CASE("several on one day: the personal one first")
{
    People p = Sam();
    p.birthday = "12-25";
    const auto both = greeting::OccasionsOn(p, Date{2026, 12, 25});
    REQUIRE(both.size() == 2);
    CHECK(both[0].key == "birthday");
    CHECK(both[1].key == "christmas");
}

TEST_CASE("leap-day dates show on Feb 28 in other years")
{
    People p;
    p.birthday = "02-29";
    CHECK(greeting::OccasionsOn(p, Date{2027, 2, 28}).size() == 1);
    CHECK(greeting::OccasionsOn(p, Date{2028, 2, 28}).empty());
    CHECK(greeting::OccasionsOn(p, Date{2028, 2, 29}).size() == 1);
}

TEST_CASE("dates, ordinals and the stored custom days")
{
    int y = 0, m = 0, d = 0;
    CHECK(greeting::ParseDay("2021-09-03", &y, &m, &d));
    CHECK((y == 2021 && m == 9 && d == 3));
    CHECK(greeting::ParseDay("02-29", &y, &m, &d));
    CHECK_FALSE(greeting::ParseDay("2023-02-29", &y, &m, &d));
    CHECK_FALSE(greeting::ParseDay("13-01", &y, &m, &d));
    CHECK_FALSE(greeting::ParseDay("7-14", &y, &m, &d));
    CHECK(greeting::Ordinal(1) == "1st");
    CHECK(greeting::Ordinal(2) == "2nd");
    CHECK(greeting::Ordinal(3) == "3rd");
    CHECK(greeting::Ordinal(11) == "11th");
    CHECK(greeting::Ordinal(12) == "12th");
    CHECK(greeting::Ordinal(21) == "21st");
    CHECK(greeting::Ordinal(102) == "102nd");

    const auto days = Sam().custom_days;
    CHECK(greeting::ParseCustomDays(greeting::SerializeCustomDays(days)) == days);
    const std::vector<greeting::CustomDay> tricky = {{"01-02", "Tabs\there", "line\nbreak \\ ok"}};
    CHECK(greeting::ParseCustomDays(greeting::SerializeCustomDays(tricky)) == tricky);
    CHECK(greeting::ParseCustomDays("bad line\n13-40\tx\ty\n").empty());
}

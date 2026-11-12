// Battery UX (#84): the smoothed percentage and the low-battery nudges.
#include <doctest/doctest.h>

#include "battery_ux/battery_ux.h"

using battery_ux::NudgeTracker;
using battery_ux::PercentSmoother;

TEST_CASE("the shown percentage smooths jitter and only moves one way")
{
    PercentSmoother smoother;
    CHECK(smoother.percent() == -1);
    CHECK(smoother.Update(60, false) == 60);
    // Jitter up while on battery never raises the number.
    CHECK(smoother.Update(63, false) == 60);
    CHECK(smoother.Update(61, false) == 60);
    // A real fall comes through, smoothed.
    int shown = 60;
    for (int i = 0; i < 20; ++i) {
        shown = smoother.Update(50, false);
    }
    CHECK(shown == 50);
    // One low glitch moves it a little, not all the way.
    CHECK(smoother.Update(30, false) > 40);

    // Charging: only goes up.
    PercentSmoother charging;
    charging.Update(40, true);
    CHECK(charging.Update(38, true) == 40);
    for (int i = 0; i < 20; ++i) {
        shown = charging.Update(60, true);
    }
    CHECK(shown == 60);
    // Unplugged: starts from where it was, no jump back.
    CHECK(charging.Update(58, false) <= 60);
    CHECK(charging.Update(150, false) <= 60);  // clamped, and still not up
}

TEST_CASE("nudges fire once each at 20, 10 and 5")
{
    NudgeTracker tracker;
    CHECK(tracker.Update(50, false) == 0);
    CHECK(tracker.Update(21, false) == 0);
    CHECK(tracker.Update(20, false) == 20);
    CHECK(tracker.Update(19, false) == 0);
    CHECK(tracker.Update(20, false) == 0);  // jitter back over the line doesn't repeat it
    CHECK(tracker.Update(10, false) == 10);
    CHECK(tracker.Update(6, false) == 0);
    CHECK(tracker.Update(5, false) == 5);
    CHECK(tracker.Update(3, false) == 0);
}

TEST_CASE("a fast drop nudges only about the lowest threshold")
{
    NudgeTracker tracker;
    CHECK(tracker.Update(8, false) == 10);
    CHECK(tracker.Update(7, false) == 0);
    NudgeTracker deep;
    CHECK(deep.Update(4, false) == 5);
    CHECK(deep.Update(4, false) == 0);
}

TEST_CASE("plugging in or charging re-arms the nudges")
{
    NudgeTracker tracker;
    CHECK(tracker.Update(20, false) == 20);
    CHECK(tracker.Update(19, true) == 0);  // on USB: no nudges
    CHECK(tracker.Update(19, false) == 20);
    // Charged a little, but not past 20 + margin: no repeat.
    NudgeTracker margin;
    CHECK(margin.Update(20, false) == 20);
    CHECK(margin.Update(24, false) == 0);
    CHECK(margin.Update(20, false) == 0);
    CHECK(margin.Update(25, false) == 0);
    CHECK(margin.Update(20, false) == 20);
}

TEST_CASE("the words")
{
    CHECK_FALSE(battery_ux::NudgeMessage(20).card);
    CHECK(battery_ux::NudgeMessage(10).card);
    CHECK(battery_ux::NudgeMessage(5).title == "Battery almost empty");
    CHECK(battery_ux::ChargingMessage(42).body.find("42%") != std::string::npos);
    CHECK(battery_ux::ChargingMessage(-1).body.find('%') == std::string::npos);
    CHECK(battery_ux::ChargingMessage(100).title == "All charged up");
}

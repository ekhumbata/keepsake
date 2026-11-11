#include <doctest/doctest.h>

#include "page_navigation/roving_focus.h"

using page_navigation::RovingFocus;

TEST_CASE("WrapIndex wraps both directions and rejects empty scopes")
{
    CHECK(RovingFocus::WrapIndex(0, 3) == 0);
    CHECK(RovingFocus::WrapIndex(3, 3) == 0);
    CHECK(RovingFocus::WrapIndex(-1, 3) == 2);
    CHECK(RovingFocus::WrapIndex(-4, 3) == 2);
    CHECK(RovingFocus::WrapIndex(7, 3) == 1);
    CHECK(RovingFocus::WrapIndex(0, 0) == -1);
    CHECK(RovingFocus::WrapIndex(5, -2) == -1);
}

TEST_CASE("an empty scope has no focus and never moves")
{
    RovingFocus focus;
    CHECK(focus.item_count() == 0);
    CHECK(focus.index() == -1);
    CHECK_FALSE(focus.Move(1));
    CHECK_FALSE(focus.SetIndex(0));
    CHECK(focus.index() == -1);
}

TEST_CASE("Move wraps around and reports whether the index changed")
{
    RovingFocus focus(3);
    CHECK(focus.index() == 0);

    CHECK(focus.Move(1));
    CHECK(focus.index() == 1);
    CHECK(focus.Move(2));
    CHECK(focus.index() == 0);  // wrapped past the end
    CHECK(focus.Move(-1));
    CHECK(focus.index() == 2);  // wrapped past the start

    CHECK_FALSE(focus.Move(0));
    CHECK_FALSE(focus.Move(3));  // a full lap lands on the same item
    CHECK(focus.index() == 2);
}

TEST_CASE("a single-item scope never reports a change")
{
    RovingFocus focus(1);
    CHECK(focus.index() == 0);
    CHECK_FALSE(focus.Move(1));
    CHECK_FALSE(focus.Move(-1));
    CHECK_FALSE(focus.SetIndex(0));
}

TEST_CASE("Configure clamps the initial index into range")
{
    RovingFocus focus;
    focus.Configure(4, 6);
    CHECK(focus.index() == 2);
    focus.Configure(4, -1);
    CHECK(focus.index() == 3);
    focus.Configure(-5, 1);
    CHECK(focus.item_count() == 0);
    CHECK(focus.index() == -1);
}

TEST_CASE("SetIndex wraps and only reports real changes")
{
    RovingFocus focus(5, 2);
    CHECK_FALSE(focus.SetIndex(2));
    CHECK(focus.SetIndex(-1));
    CHECK(focus.index() == 4);
    CHECK_FALSE(focus.SetIndex(9));  // 9 wraps to 4, already focused
}

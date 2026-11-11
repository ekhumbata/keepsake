// net_jobs::Scheduler: the Wi-Fi-on-demand policy (#17).
#include <doctest/doctest.h>

#include "net_jobs/scheduler.h"

using net_jobs::RadioState;
using net_jobs::Scheduler;
using net_jobs::Urgency;

namespace {

constexpr net_jobs::Millis kSecond = 1000;
constexpr net_jobs::Millis kMinute = 60 * kSecond;

}  // namespace

TEST_CASE("the radio is off until there's work, and powers down after a linger")
{
    Scheduler s;
    CHECK(s.state() == RadioState::kOff);
    CHECK(s.next_deadline() == 0);

    auto a = s.Submit(1, Urgency::kBackground, 0);
    CHECK(a.radio_on);
    CHECK(s.state() == RadioState::kConnecting);

    a = s.OnConnected(2 * kSecond);
    CHECK(a.run == std::vector<net_jobs::JobId>{1});
    CHECK(s.state() == RadioState::kOnline);

    a = s.OnJobFinished(1, false, 3 * kSecond);
    CHECK(s.state() == RadioState::kLingering);
    CHECK(s.next_deadline() == 33 * kSecond);

    CHECK_FALSE(s.Tick(20 * kSecond).radio_off);
    a = s.Tick(33 * kSecond);
    CHECK(a.radio_off);
    CHECK(s.state() == RadioState::kOff);
}

TEST_CASE("jobs queued while connecting run together, people first")
{
    Scheduler s;
    s.Submit(1, Urgency::kBackground, 0);
    CHECK_FALSE(s.Submit(2, Urgency::kBackground, 10).radio_on);  // already connecting
    s.Submit(3, Urgency::kUser, 20);
    const auto a = s.OnConnected(1000);
    CHECK(a.run == std::vector<net_jobs::JobId>{3, 1, 2});
    CHECK(s.running() == 3);
}

TEST_CASE("a job arriving during the linger runs at once without reconnecting")
{
    Scheduler s;
    s.Submit(1, Urgency::kBackground, 0);
    s.OnConnected(1000);
    s.OnJobFinished(1, false, 2000);
    REQUIRE(s.state() == RadioState::kLingering);
    const auto a = s.Submit(2, Urgency::kUser, 10 * kSecond);
    CHECK_FALSE(a.radio_on);
    CHECK(a.run == std::vector<net_jobs::JobId>{2});
    CHECK(s.state() == RadioState::kOnline);
}

TEST_CASE("no network: user jobs hear about it, background work backs off 30 then 60 min")
{
    Scheduler s;
    s.Submit(1, Urgency::kBackground, 0);
    s.Submit(2, Urgency::kUser, 0);
    auto a = s.OnConnectFailed(5 * kSecond);
    CHECK(a.radio_off);
    CHECK(a.unreachable == std::vector<net_jobs::JobId>{2});
    CHECK(s.state() == RadioState::kBackoff);
    CHECK(s.queued() == 1);
    CHECK(s.next_deadline() == 5 * kSecond + 30 * kMinute);

    // Nothing happens before the backoff ends: no scanning all day.
    CHECK_FALSE(s.Tick(10 * kMinute).radio_on);
    CHECK_FALSE(s.Submit(3, Urgency::kBackground, 11 * kMinute).radio_on);

    a = s.Tick(5 * kSecond + 30 * kMinute);
    CHECK(a.radio_on);
    a = s.OnConnectFailed(31 * kMinute);
    CHECK(s.next_deadline() == 31 * kMinute + 60 * kMinute);  // doubled
    s.Tick(91 * kMinute);
    s.OnConnectFailed(92 * kMinute);
    CHECK(s.next_deadline() == 92 * kMinute + 60 * kMinute);  // capped at an hour

    // A success resets the backoff.
    s.Tick(152 * kMinute);
    a = s.OnConnected(153 * kMinute);
    CHECK(a.run.size() == 2);
}

TEST_CASE("a job someone is waiting on skips the backoff")
{
    Scheduler s;
    s.Submit(1, Urgency::kBackground, 0);
    s.OnConnectFailed(1000);
    REQUIRE(s.state() == RadioState::kBackoff);
    const auto a = s.Submit(2, Urgency::kUser, 2 * kMinute);
    CHECK(a.radio_on);
    CHECK(s.state() == RadioState::kConnecting);
}

TEST_CASE("a connect that never completes times out like a failure")
{
    Scheduler s;
    s.Submit(1, Urgency::kUser, 0);
    CHECK(s.next_deadline() == 20 * kSecond);
    const auto a = s.Tick(20 * kSecond);
    CHECK(a.radio_off);
    CHECK(a.unreachable == std::vector<net_jobs::JobId>{1});
    CHECK(s.state() == RadioState::kOff);  // nothing left to retry
}

TEST_CASE("a retried job waits for a later connection instead of hammering this one")
{
    Scheduler s;
    s.Submit(1, Urgency::kBackground, 0);
    s.OnConnected(1000);
    const auto a = s.OnJobFinished(1, true, 2000);
    CHECK(a.radio_off);
    CHECK(s.state() == RadioState::kBackoff);
    CHECK(s.queued() == 1);
    s.Tick(2000 + 30 * kMinute);
    CHECK(s.OnConnected(2000 + 30 * kMinute + kSecond).run == std::vector<net_jobs::JobId>{1});
}

TEST_CASE("late events and unknown jobs are ignored; a full queue rejects")
{
    Scheduler s;
    CHECK_FALSE(s.OnConnected(0).radio_on);
    CHECK(s.OnConnected(0).run.empty());
    CHECK_FALSE(s.OnConnectFailed(0).radio_off);
    CHECK_FALSE(s.OnJobFinished(42, false, 0).radio_off);

    net_jobs::Config config;
    config.max_queued_jobs = 2;
    Scheduler small(config);
    small.Submit(1, Urgency::kBackground, 0);
    small.Submit(2, Urgency::kBackground, 0);
    CHECK(small.Submit(3, Urgency::kBackground, 0).rejected == std::vector<net_jobs::JobId>{3});
}

TEST_CASE("losing the link while lingering just powers down")
{
    Scheduler s;
    s.Submit(1, Urgency::kBackground, 0);
    s.OnConnected(1000);
    s.OnJobFinished(1, false, 2000);
    const auto a = s.OnDisconnected(3000);
    CHECK(a.radio_off);
    CHECK(s.state() == RadioState::kOff);
}

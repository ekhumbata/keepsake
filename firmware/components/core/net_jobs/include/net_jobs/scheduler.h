#ifndef NET_JOBS_SCHEDULER_H_
#define NET_JOBS_SCHEDULER_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

// Wi-Fi on demand (#17, plan "Power foundations" item 2). The radio is off by default;
// network work is queued as jobs, the radio comes up, the queue drains, the radio lingers a
// little (so a follow-up job doesn't pay a reconnect) and then powers down. With no known
// network in range it backs off (30 min, then 60 min) instead of scanning all day, but a job
// someone is waiting on always tries straight away.
//
// Pure policy, driven by events and a millisecond clock, and answering with Actions for the
// caller to carry out. No threads, no ESP-IDF, so it's unit-tested on the host.
namespace net_jobs {

using JobId = uint32_t;
using Millis = uint64_t;

enum class Urgency : uint8_t {
    kUser,        // someone is waiting (a search, "Check for updates"): connect now
    kBackground,  // weather, time sync, transcription retry, OTA check: batch it
};

enum class RadioState : uint8_t {
    kOff,         // radio off, nothing waiting
    kConnecting,  // radio on, waiting for an IP
    kOnline,      // connected, jobs running
    kLingering,   // connected, queue empty, powering down at linger_until
    kBackoff,     // last attempt found no network; background jobs wait until retry_at
};

struct Config {
    Millis connect_timeout_ms = 20 * 1000;
    Millis linger_ms = 30 * 1000;
    Millis first_backoff_ms = 30 * 60 * 1000;
    Millis max_backoff_ms = 60 * 60 * 1000;
    size_t max_queued_jobs = 32;
};

// What the caller must do after an event. Several can be set at once.
struct Actions {
    bool radio_on = false;
    bool radio_off = false;
    std::vector<JobId> run;          // start these jobs now (radio is connected)
    std::vector<JobId> unreachable;  // user jobs that can't run: no network (tell the person)
    std::vector<JobId> rejected;     // the queue was full
};

class Scheduler {
public:
    explicit Scheduler(Config config = {}) : config_(config) {}

    Actions Submit(JobId id, Urgency urgency, Millis now);
    Actions OnConnected(Millis now);
    // Connecting failed or timed out: no known network reachable.
    Actions OnConnectFailed(Millis now);
    // The link dropped while online; running jobs report their own outcome.
    Actions OnDisconnected(Millis now);
    // A job finished. `retry` puts it back in the queue (a transient failure).
    Actions OnJobFinished(JobId id, bool retry, Millis now);
    // Call periodically, or at next_deadline(): handles linger expiry, connect timeouts and
    // the end of a backoff.
    Actions Tick(Millis now);

    RadioState state() const { return state_; }
    size_t queued() const { return queue_.size(); }
    size_t running() const { return running_.size(); }
    // When Tick next has something to do; 0 when nothing is pending.
    Millis next_deadline() const;

private:
    struct Job {
        JobId id;
        Urgency urgency;
    };

    Actions StartConnecting(Millis now);
    void RunQueued(Actions* actions);
    void EnterLingerIfIdle(Millis now);
    bool HasUserJob() const;

    Config config_;
    RadioState state_ = RadioState::kOff;
    std::deque<Job> queue_;
    std::vector<JobId> running_;
    Millis deadline_ = 0;  // connect timeout, linger end or backoff end, by state
    Millis backoff_ms_ = 0;
};

const char* RadioStateName(RadioState state);

}  // namespace net_jobs

#endif  // NET_JOBS_SCHEDULER_H_

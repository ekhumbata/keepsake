// libFuzzer target: random event sequences against net_jobs::Scheduler. Every submitted job
// must end up exactly once as queued, running, done, unreachable or rejected; radio_on is
// never asked for while already connected; deadlines exist exactly in the timed states.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <map>

#include "net_jobs/scheduler.h"

namespace {

enum class Where { kWaiting, kRunning, kDone };

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    net_jobs::Config config;
    config.max_queued_jobs = 6;
    net_jobs::Scheduler s(config);
    std::map<net_jobs::JobId, Where> jobs;
    net_jobs::Millis now = 0;
    net_jobs::JobId next_id = 1;

    auto apply = [&](const net_jobs::Actions& a, net_jobs::RadioState before) {
        if (a.radio_on && (before == net_jobs::RadioState::kOnline ||
                           before == net_jobs::RadioState::kLingering ||
                           before == net_jobs::RadioState::kConnecting)) {
            std::abort();
        }
        for (auto id : a.run) {
            if (jobs.count(id) == 0 || jobs[id] != Where::kWaiting) std::abort();
            jobs[id] = Where::kRunning;
        }
        for (auto id : a.unreachable) {
            if (jobs.count(id) == 0 || jobs[id] != Where::kWaiting) std::abort();
            jobs[id] = Where::kDone;
        }
        for (auto id : a.rejected) {
            if (jobs.count(id) == 0 || jobs[id] != Where::kWaiting) std::abort();
            jobs[id] = Where::kDone;
        }
    };

    for (size_t i = 0; i < size; ++i) {
        const uint8_t op = data[i];
        now += (op >> 4) * 7 * 60 * 1000ull;  // jump up to ~105 minutes
        const auto before = s.state();
        switch (op & 0x7) {
            case 0:
            case 1: {
                const net_jobs::JobId id = next_id++;
                jobs[id] = Where::kWaiting;
                apply(s.Submit(id, (op & 1) ? net_jobs::Urgency::kUser : net_jobs::Urgency::kBackground, now), before);
                break;
            }
            case 2: apply(s.OnConnected(now), before); break;
            case 3: apply(s.OnConnectFailed(now), before); break;
            case 4: apply(s.OnDisconnected(now), before); break;
            case 5:
            case 6:
                for (auto& [id, where] : jobs) {
                    if (where == Where::kRunning) {
                        const bool retry = (op & 0x7) == 6;
                        where = retry ? Where::kWaiting : Where::kDone;
                        apply(s.OnJobFinished(id, retry, now), before);
                        break;
                    }
                }
                break;
            default: apply(s.Tick(now), before); break;
        }
        size_t waiting = 0;
        size_t running = 0;
        for (const auto& [id, where] : jobs) {
            waiting += where == Where::kWaiting;
            running += where == Where::kRunning;
        }
        if (waiting != s.queued() || running != s.running()) std::abort();
        const bool timed = s.state() == net_jobs::RadioState::kConnecting ||
                           s.state() == net_jobs::RadioState::kLingering ||
                           s.state() == net_jobs::RadioState::kBackoff;
        if (timed != (s.next_deadline() != 0) && !(timed && s.next_deadline() == 0 && now == 0)) {
            std::abort();
        }
    }
    return 0;
}

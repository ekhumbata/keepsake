#include "net_jobs/scheduler.h"

#include <algorithm>

namespace net_jobs {

bool Scheduler::HasUserJob() const
{
    return std::any_of(queue_.begin(), queue_.end(),
                       [](const Job& job) { return job.urgency == Urgency::kUser; });
}

Actions Scheduler::StartConnecting(Millis now)
{
    Actions actions;
    actions.radio_on = true;
    state_ = RadioState::kConnecting;
    deadline_ = now + config_.connect_timeout_ms;
    return actions;
}

void Scheduler::RunQueued(Actions* actions)
{
    // People first, then background work, each in submission order.
    for (const Urgency pass : {Urgency::kUser, Urgency::kBackground}) {
        for (const Job& job : queue_) {
            if (job.urgency == pass) {
                actions->run.push_back(job.id);
                running_.push_back(job.id);
            }
        }
    }
    queue_.clear();
}

void Scheduler::EnterLingerIfIdle(Millis now)
{
    if (state_ == RadioState::kOnline && running_.empty() && queue_.empty()) {
        state_ = RadioState::kLingering;
        deadline_ = now + config_.linger_ms;
    }
}

Actions Scheduler::Submit(JobId id, Urgency urgency, Millis now)
{
    Actions actions;
    if (queue_.size() >= config_.max_queued_jobs) {
        actions.rejected.push_back(id);
        return actions;
    }
    switch (state_) {
        case RadioState::kOnline:
        case RadioState::kLingering:
            state_ = RadioState::kOnline;
            actions.run.push_back(id);
            running_.push_back(id);
            return actions;
        case RadioState::kConnecting:
            queue_.push_back({id, urgency});
            return actions;
        case RadioState::kBackoff:
            queue_.push_back({id, urgency});
            if (urgency == Urgency::kUser) {
                return StartConnecting(now);  // someone is waiting: try now
            }
            return actions;
        case RadioState::kOff:
            queue_.push_back({id, urgency});
            return StartConnecting(now);
    }
    return actions;
}

Actions Scheduler::OnConnected(Millis now)
{
    Actions actions;
    if (state_ != RadioState::kConnecting) {
        return actions;  // late or duplicate event
    }
    backoff_ms_ = 0;
    state_ = RadioState::kOnline;
    RunQueued(&actions);
    EnterLingerIfIdle(now);
    return actions;
}

Actions Scheduler::OnConnectFailed(Millis now)
{
    Actions actions;
    if (state_ != RadioState::kConnecting) {
        return actions;
    }
    actions.radio_off = true;
    // Whoever was waiting hears about it now; background work keeps its place in the queue.
    for (auto it = queue_.begin(); it != queue_.end();) {
        if (it->urgency == Urgency::kUser) {
            actions.unreachable.push_back(it->id);
            it = queue_.erase(it);
        } else {
            ++it;
        }
    }
    backoff_ms_ = backoff_ms_ == 0 ? config_.first_backoff_ms
                                   : std::min(backoff_ms_ * 2, config_.max_backoff_ms);
    if (queue_.empty()) {
        state_ = RadioState::kOff;
        deadline_ = 0;
    } else {
        state_ = RadioState::kBackoff;
        deadline_ = now + backoff_ms_;
    }
    return actions;
}

Actions Scheduler::OnDisconnected(Millis now)
{
    Actions actions;
    if (state_ == RadioState::kLingering) {
        state_ = RadioState::kOff;
        deadline_ = 0;
        actions.radio_off = true;
        return actions;
    }
    if (state_ == RadioState::kOnline && running_.empty() && !queue_.empty()) {
        return StartConnecting(now);
    }
    // Online with jobs running: they fail on their own and report back with retry; the
    // reconnect happens when the first one is re-queued.
    if (state_ == RadioState::kOnline) {
        state_ = RadioState::kConnecting;
        deadline_ = now + config_.connect_timeout_ms;
    }
    return actions;
}

Actions Scheduler::OnJobFinished(JobId id, bool retry, Millis now)
{
    Actions actions;
    const auto it = std::find(running_.begin(), running_.end(), id);
    if (it == running_.end()) {
        return actions;
    }
    running_.erase(it);
    if (retry) {
        // Retrying on the same connection would most likely fail the same way, so a retried
        // job waits for a later connection instead.
        queue_.push_back({id, Urgency::kBackground});
    }
    if (state_ == RadioState::kOnline && running_.empty()) {
        if (queue_.empty()) {
            EnterLingerIfIdle(now);
        } else {
            // Only retried work is left: power down and try again after a backoff.
            actions.radio_off = true;
            state_ = RadioState::kBackoff;
            backoff_ms_ = backoff_ms_ == 0 ? config_.first_backoff_ms : backoff_ms_;
            deadline_ = now + backoff_ms_;
        }
    }
    return actions;
}

Actions Scheduler::Tick(Millis now)
{
    switch (state_) {
        case RadioState::kLingering:
            if (now >= deadline_) {
                Actions actions;
                actions.radio_off = true;
                state_ = RadioState::kOff;
                deadline_ = 0;
                return actions;
            }
            break;
        case RadioState::kConnecting:
            if (now >= deadline_) {
                return OnConnectFailed(now);
            }
            break;
        case RadioState::kBackoff:
            if (now >= deadline_) {
                return StartConnecting(now);
            }
            break;
        default:
            break;
    }
    return {};
}

Millis Scheduler::next_deadline() const
{
    switch (state_) {
        case RadioState::kLingering:
        case RadioState::kConnecting:
        case RadioState::kBackoff:
            return deadline_;
        default:
            return 0;
    }
}

const char* RadioStateName(RadioState state)
{
    switch (state) {
        case RadioState::kOff:
            return "off";
        case RadioState::kConnecting:
            return "connecting";
        case RadioState::kOnline:
            return "online";
        case RadioState::kLingering:
            return "lingering";
        case RadioState::kBackoff:
            return "backoff";
    }
    return "unknown";
}

}  // namespace net_jobs

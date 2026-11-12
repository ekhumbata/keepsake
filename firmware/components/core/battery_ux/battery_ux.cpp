#include "battery_ux/battery_ux.h"

#include <algorithm>
#include <cmath>

namespace battery_ux {
namespace {

constexpr float kAlpha = 0.25f;  // weight of a new reading in the moving average

}  // namespace

int PercentSmoother::Update(int raw_percent, bool charging)
{
    const int raw = std::clamp(raw_percent, 0, 100);
    if (shown_ < 0) {
        average_ = static_cast<float>(raw);
        shown_ = raw;
        charging_ = charging;
        return shown_;
    }
    if (charging != charging_) {
        // A new direction: start from the shown value, so the number doesn't jump back.
        average_ = static_cast<float>(shown_);
        charging_ = charging;
    }
    average_ += kAlpha * (static_cast<float>(raw) - average_);
    const int candidate = static_cast<int>(std::lround(average_));
    shown_ = charging ? std::max(shown_, candidate) : std::min(shown_, candidate);
    return shown_;
}

int NudgeTracker::Update(int percent, bool usb_power)
{
    constexpr int kCount = static_cast<int>(sizeof(kNudgeThresholds) / sizeof(kNudgeThresholds[0]));
    for (int i = 0; i < kCount; ++i) {
        if (usb_power || percent >= kNudgeThresholds[i] + kRearmMargin) {
            fired_[i] = false;
        }
    }
    if (usb_power || percent < 0) {
        return 0;
    }
    // The lowest threshold reached that hasn't fired; higher ones are then marked as done.
    for (int i = kCount - 1; i >= 0; --i) {
        if (percent <= kNudgeThresholds[i] && !fired_[i]) {
            for (int j = 0; j <= i; ++j) {
                fired_[j] = true;
            }
            return kNudgeThresholds[i];
        }
    }
    return 0;
}

Message NudgeMessage(int threshold)
{
    if (threshold <= 5) {
        return {"Battery almost empty",
                "Mochi is nodding off. Please plug Keepsake in now; your books and notes are "
                "safe either way.",
                true};
    }
    if (threshold <= 10) {
        return {"Battery at 10%",
                "Mochi's getting sleepy. Plug Keepsake in when you can.",
                true};
    }
    return {"Battery at " + std::to_string(threshold) + "%", "Time to charge soon.", false};
}

Message ChargingMessage(int percent)
{
    if (percent >= 100) {
        return {"All charged up", "Thanks for the snack! Keepsake is full.", true};
    }
    std::string body = "Mochi is having a snack.";
    if (percent >= 0) {
        body += " " + std::to_string(percent) + "% so far.";
    }
    body += " Keepsake works as usual while it charges.";
    return {"Charging", body, true};
}

}  // namespace battery_ux

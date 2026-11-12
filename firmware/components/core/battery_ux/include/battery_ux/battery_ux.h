#ifndef BATTERY_UX_BATTERY_UX_H_
#define BATTERY_UX_BATTERY_UX_H_

#include <cstdint>
#include <string>

// The friend should know when to charge, gently (#84). The PMIC's fuel gauge jitters by a few
// percent; the screen shows a smoothed value that only goes down on battery and only up while
// charging, and nudges once each at 20, 10 and 5 %.
namespace battery_ux {

class PercentSmoother {
public:
    // One reading (0..100, out-of-range values are clamped). Returns the percentage to show.
    int Update(int raw_percent, bool charging);
    // -1 before the first reading.
    int percent() const { return shown_; }

private:
    float average_ = 0.0f;
    int shown_ = -1;
    bool charging_ = false;
};

inline constexpr int kNudgeThresholds[] = {20, 10, 5};
// A threshold fires again only after the battery has been charged this far above it.
inline constexpr int kRearmMargin = 5;

class NudgeTracker {
public:
    // Returns the threshold to nudge about now (20, 10 or 5), or 0. Each fires once per
    // discharge; plugging in, or charging back above threshold + kRearmMargin, re-arms it.
    // Skipping thresholds (a fast drop) nudges only about the lowest one reached.
    int Update(int percent, bool usb_power);

private:
    bool fired_[3] = {false, false, false};
};

struct Message {
    std::string title;
    std::string body;
    bool card = false;  // a card (needs Close) rather than a passing toast
};

// What a nudge says, in plain, kind words.
Message NudgeMessage(int threshold);
// The card shown when the charger is plugged in.
Message ChargingMessage(int percent);

}  // namespace battery_ux

#endif  // BATTERY_UX_BATTERY_UX_H_

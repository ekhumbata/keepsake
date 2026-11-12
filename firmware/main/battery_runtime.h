#ifndef BATTERY_RUNTIME_H_
#define BATTERY_RUNTIME_H_

// Battery UX on the device (#84): the smoothed percentage for the status bar, a charging card
// with Mochi when the charger is plugged in, and gentle nudges at 20, 10 and 5 %. The rules
// live in core/battery_ux; this watches power_service's cached readings.
namespace battery_runtime {

// Starts watching. Call once the first screen is up (cards and toasts need it).
void Start();
// The percentage to show, smoothed; -1 until the battery has been read.
int DisplayPercent();

}  // namespace battery_runtime

#endif  // BATTERY_RUNTIME_H_

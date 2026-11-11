#ifndef CPU_BOOST_H_
#define CPU_BOOST_H_

#include "esp_err.h"

// Power management policy for the CPU clock: idle at a low frequency, run at full speed only
// while someone holds a boost. Keepsake's principle is "save power in the background, never
// on the interaction": anything a person is waiting on (drawing a page, handling a button)
// holds a ScopedBoost; background waits (a panel refresh in progress, an idle Wi-Fi link) don't.
//
// With CONFIG_PM_ENABLE or CONFIG_KEEPSAKE_PM_DFS off, everything here is a no-op and the CPU
// stays at its fixed default frequency.
namespace cpu_boost {

// Configures frequency scaling (max = the default CPU frequency, min =
// CONFIG_KEEPSAKE_PM_MIN_CPU_FREQ_MHZ, no automatic light sleep). Call once, early in startup,
// before tasks that take boosts are created. Safe to call again.
esp_err_t Init();

// Holds the CPU at full speed for its lifetime. Cheap and reentrant (the underlying lock is
// reference-counted), so nesting is fine. Before Init(), it does nothing.
class ScopedBoost {
public:
    explicit ScopedBoost(const char* reason = nullptr);
    ~ScopedBoost();
    ScopedBoost(const ScopedBoost&) = delete;
    ScopedBoost& operator=(const ScopedBoost&) = delete;

private:
    bool held_ = false;
};

}  // namespace cpu_boost

#endif  // CPU_BOOST_H_

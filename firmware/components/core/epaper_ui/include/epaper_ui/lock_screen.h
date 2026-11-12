#ifndef EPAPER_UI_LOCK_SCREEN_H_
#define EPAPER_UI_LOCK_SCREEN_H_

#include <string>
#include <vector>

#include "epaper_ui/status_bar.h"

struct EmbeddedImageAsset;

namespace epaper_ui {

// The ambient variant (#38): what a resting Keepsake shows instead of the todo list. By day a
// big clock, the date, the weather, the indoor climate and this month; at night a sleeping
// Mochi and tomorrow's first todo, without the clock (night is "no updates").
struct LockScreenAmbient {
    bool enabled = false;
    bool night = false;
    std::string clock_text = {};    // "9:41"
    std::string weather_text = {};  // "8C, light rain"; empty: no line
    std::string range_text = {};    // "High 9C, low 3C"
    std::string indoor_text = {};   // "Inside 21C, 45%"
    int year = 0;                   // the calendar; month 0 = none
    int month = 0;
    int today = 0;
    std::string night_note = {};    // "Tomorrow: water the fern"
    const EmbeddedImageAsset* mascot = nullptr;
};

struct LockScreenState {
    std::string weekday_text = {};
    std::string date_text = {};
    // The top few pending todos, already selected and prioritized (follow-up flagged ones
    // first) by the caller. Full transcript text, untruncated -- DrawLockScreen wraps and
    // caps each one to fit.
    std::vector<std::string> pending_todo_titles = {};
    // True total count of pending todos, which may be larger than pending_todo_titles.size().
    int pending_todo_count = 0;
    // True only for the one-shot freeze frame painted immediately before a full
    // power-off (see lock_screen_runtime::ShowForShutdown): swaps the bottom lock
    // icon for a power icon at the same size/position, so the frozen screen reads
    // as "powered down" rather than "locked."
    bool for_shutdown = false;
    LockScreenAmbient ambient = {};
};

void DrawLockScreen(uint8_t* framebuffer,
                    int raw_width,
                    int raw_height,
                    int portrait_width,
                    int portrait_height,
                    const LockScreenState& state,
                    const StatusBarState& status_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_LOCK_SCREEN_H_

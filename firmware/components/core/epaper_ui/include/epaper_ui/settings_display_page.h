#ifndef EPAPER_UI_SETTINGS_DISPLAY_PAGE_H_
#define EPAPER_UI_SETTINGS_DISPLAY_PAGE_H_

#include <cstdint>
#include <string_view>

#include "epaper_ui/button.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/select_input.h"
#include "epaper_ui/status_bar.h"

namespace epaper_ui {

enum class SettingsDisplayPageItemId : uint8_t {
    kNone = 0,
    kLockScreenInput,
    kButtonSoundsInput,
    kBack,
};

struct SettingsDisplayPageState {
    int navigation_focus_index = -1;
    std::string_view title_text = "Display & sound";
    // "Lock screen shows": the todos or a random photo (#148), a fixed-choice SelectModal.
    SelectInputState lock_screen_input = {};
    // "Button sounds": the clicks on rocker moves and presses, on or off (#163).
    SelectInputState button_sounds_input = {};
    // Returns to the Settings hub -- this page is reached only from there.
    ButtonState back = {};
};

UiRect SettingsDisplayPageItemBounds(int portrait_width,
                                   int portrait_height,
                                   const SettingsDisplayPageState& state,
                                   SettingsDisplayPageItemId item);
bool HitTestSettingsDisplayPageItem(int portrait_width,
                                  int portrait_height,
                                  const SettingsDisplayPageState& state,
                                  int x,
                                  int y,
                                  SettingsDisplayPageItemId* item);
void DrawSettingsDisplayPage(uint8_t* framebuffer,
                           int raw_width,
                           int raw_height,
                           int portrait_width,
                           int portrait_height,
                           const SettingsDisplayPageState& state,
                           const StatusBarState& status_bar_state,
                           const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SETTINGS_DISPLAY_PAGE_H_

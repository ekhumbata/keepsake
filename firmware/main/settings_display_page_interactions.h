#ifndef SETTINGS_DISPLAY_PAGE_INTERACTIONS_H_
#define SETTINGS_DISPLAY_PAGE_INTERACTIONS_H_

#include <cstdint>
#include <functional>

#include "page_action_result.h"
#include "settings_display_page_coordinator.h"

namespace settings_display_page_interactions {

enum class ActivateIntent : uint8_t {
    kNone = 0,
    kShowHome,
    kShowSettings,
    kShowLockScreenModal,
    kShowButtonSoundsModal,
};

struct ActivateResult {
    ActivateIntent intent = ActivateIntent::kNone;
    bool handled = false;
    bool play_activate_cue = false;
};

using FocusMoveResult = page_actions::FocusMoveOutcome;

struct ActivateCallbacks {
    std::function<void()> show_home;
    std::function<void()> show_settings;
    std::function<void()> show_lock_screen_modal;
    std::function<void()> show_button_sounds_modal;
};

ActivateResult HandlePrimaryActivate(const SettingsDisplayPageCoordinator& coordinator);
void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(SettingsDisplayPageCoordinator& coordinator, int delta);

}  // namespace settings_display_page_interactions

#endif  // SETTINGS_DISPLAY_PAGE_INTERACTIONS_H_

#include "settings_display_page_interactions.h"

namespace settings_display_page_interactions {
namespace {

using page_navigation::NavigationItemRole;

ActivateResult Intent(ActivateIntent intent)
{
    ActivateResult result = {};
    result.handled = true;
    result.play_activate_cue = true;
    result.intent = intent;
    return result;
}

}  // namespace

ActivateResult HandlePrimaryActivate(const SettingsDisplayPageCoordinator& coordinator)
{
    if (coordinator.IsRoleFocused(NavigationItemRole::kSettingsDisplayLockScreenInput)) {
        return Intent(ActivateIntent::kShowLockScreenModal);
    }
    if (coordinator.IsRoleFocused(NavigationItemRole::kSettingsDisplayButtonSoundsInput)) {
        return Intent(ActivateIntent::kShowButtonSoundsModal);
    }
    if (coordinator.IsRoleFocused(NavigationItemRole::kSettingsDisplayBackButton)) {
        return Intent(ActivateIntent::kShowSettings);  // only ever reached from the hub
    }
    if (coordinator.IsRoleFocused(NavigationItemRole::kFooterHome)) {
        return Intent(ActivateIntent::kShowHome);
    }
    return {};
}

void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks)
{
    const auto call = [](const std::function<void()>& callback) {
        if (callback) {
            callback();
        }
    };
    switch (result.intent) {
        case ActivateIntent::kShowHome:
            call(callbacks.show_home);
            break;
        case ActivateIntent::kShowSettings:
            call(callbacks.show_settings);
            break;
        case ActivateIntent::kShowLockScreenModal:
            call(callbacks.show_lock_screen_modal);
            break;
        case ActivateIntent::kShowButtonSoundsModal:
            call(callbacks.show_button_sounds_modal);
            break;
        case ActivateIntent::kNone:
        default:
            break;
    }
}

FocusMoveResult HandleMoveFocus(SettingsDisplayPageCoordinator& coordinator, int delta)
{
    FocusMoveResult result = {};
    if (!coordinator.MoveFocus(delta)) {
        return result;
    }
    result.handled = true;
    result.play_navigation_cue = true;
    result.apply_page_state = true;
    return result;
}

}  // namespace settings_display_page_interactions

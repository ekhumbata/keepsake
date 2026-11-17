#include "settings_display_page_coordinator.h"

SettingsDisplayPageCoordinator::SettingsDisplayPageCoordinator() = default;

void SettingsDisplayPageCoordinator::Show()
{
    focus_.Configure(navigation_model_.item_count, 0);
}

bool SettingsDisplayPageCoordinator::MoveFocus(int delta)
{
    return focus_.Move(delta);
}

bool SettingsDisplayPageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

bool SettingsDisplayPageCoordinator::IsRoleFocused(page_navigation::NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

epaper_ui::SettingsDisplayPageState SettingsDisplayPageCoordinator::BuildState(
    bool lock_screen_photo, bool button_sounds) const
{
    epaper_ui::SettingsDisplayPageState state = {};
    state.navigation_focus_index = focus_.index();
    state.lock_screen_input = {
        .label_text = "Lock screen shows",
        .value_text = lock_screen_photo ? "A photo" : "Todos",
        .focused = IsRoleFocused(
            page_navigation::NavigationItemRole::kSettingsDisplayLockScreenInput),
    };
    state.button_sounds_input = {
        .label_text = "Button sounds",
        .value_text = button_sounds ? "On" : "Off",
        .focused = IsRoleFocused(
            page_navigation::NavigationItemRole::kSettingsDisplayButtonSoundsInput),
    };
    state.back = {
        .label_text = "Back",
        .selected =
            IsRoleFocused(page_navigation::NavigationItemRole::kSettingsDisplayBackButton),
    };
    return state;
}

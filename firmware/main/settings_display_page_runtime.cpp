#include "settings_display_page_runtime.h"

#include <climits>
#include <cstddef>
#include <mutex>

#include "app_state_service.h"
#include "overlay_runtime.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/page_focus_projection.h"
#include "feedback_service.h"
#include "settings_display_page_coordinator.h"
#include "settings_display_page_interactions.h"
#include "ui_refresh_runtime.h"

namespace settings_display_page_runtime {
namespace {

// The lock screen picker (#148): index 0 = the todos, 1 = a photo.
constexpr const char* kLockScreenOptions[] = {"Todos", "A photo"};
// The button sounds picker (#163): index 0 = on, 1 = off.
constexpr const char* kButtonSoundsOptions[] = {"On", "Off"};

std::mutex s_mutex;
SettingsDisplayPageCoordinator s_coordinator = {};
bool s_button_sounds_modal_active = false;
bool s_lock_screen_modal_active = false;

footer_runtime::FooterFocusItem FooterItemForSelectedIndex(int selected_index)
{
    switch (selected_index) {
        case 1:
            return footer_runtime::FooterFocusItem::kSettings;
        case 2:
            return footer_runtime::FooterFocusItem::kWifi;
        case 3:
            return footer_runtime::FooterFocusItem::kTime;
        case 0:
            return footer_runtime::FooterFocusItem::kHome;
        case 4:
            return footer_runtime::FooterFocusItem::kSticky;
        default:
            return footer_runtime::FooterFocusItem::kNone;
    }
}

page_navigation::NavigationItemRole FooterRoleForFooterItem(footer_runtime::FooterFocusItem item)
{
    switch (item) {
        case footer_runtime::FooterFocusItem::kSettings:
            return page_navigation::NavigationItemRole::kFooterSettings;
        case footer_runtime::FooterFocusItem::kWifi:
            return page_navigation::NavigationItemRole::kFooterWifi;
        case footer_runtime::FooterFocusItem::kHome:
            return page_navigation::NavigationItemRole::kFooterHome;
        case footer_runtime::FooterFocusItem::kTime:
            return page_navigation::NavigationItemRole::kFooterTime;
        case footer_runtime::FooterFocusItem::kSticky:
            return page_navigation::NavigationItemRole::kFooterSticky;
        case footer_runtime::FooterFocusItem::kNone:
        case footer_runtime::FooterFocusItem::kFolder:
        case footer_runtime::FooterFocusItem::kMic:
        default:
            return page_navigation::NavigationItemRole::kUnknown;
    }
}

footer_runtime::ProjectionState BuildFooterProjectionStateLocked()
{
    const page_navigation::PageFocusProjection projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(), page_navigation::NavigationItemSection::kNone,
        s_coordinator.focus().index(), -1, -1);
    footer_runtime::ProjectionState state = {};
    state.focused_item = FooterItemForSelectedIndex(projection.footer_selected_index);
    return state;
}

bool FooterProjectionChangedForFocusIndexes(int old_focus_index, int new_focus_index)
{
    const page_navigation::PageFocusProjection old_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(), page_navigation::NavigationItemSection::kNone,
        old_focus_index, -1, -1);
    const page_navigation::PageFocusProjection new_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(), page_navigation::NavigationItemSection::kNone,
        new_focus_index, -1, -1);
    return FooterItemForSelectedIndex(old_projection.footer_selected_index) !=
           FooterItemForSelectedIndex(new_projection.footer_selected_index);
}

epaper_ui::SettingsDisplayPageState BuildStateLocked()
{
    return s_coordinator.BuildState(app_state_service::LockScreenShowsPhoto(),
                                    app_state_service::ButtonSoundsEnabled());
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetSettingsDisplayPageState(BuildStateLocked());
}

esp_err_t UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode refresh_mode)
{
    return UpdateDisplayStateAndRequestRefresh(
        display_service::RefreshRequest{.refresh_mode = refresh_mode});
}

esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request)
{
    return ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kSettingsDisplayPage,
                                        &UpdateDisplayState, refresh_request);
}

page_actions::FocusMoveOutcome MoveFocus(int delta)
{
    page_actions::FocusMoveOutcome result = {};
    int old_focus_index = -1;
    int new_focus_index = -1;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        old_focus_index = s_coordinator.focus().index();
        result = settings_display_page_interactions::HandleMoveFocus(s_coordinator, delta);
        if (!result.handled) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
    }

    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);
    return result;
}

settings_display_page_interactions::ActivateResult ActivateFocusedItem()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return settings_display_page_interactions::HandlePrimaryActivate(s_coordinator);
}

footer_runtime::ProjectionState BuildFooterProjectionState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return BuildFooterProjectionStateLocked();
}

page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item)
{
    page_actions::FocusUpdateOutcome result = {};
    const page_navigation::NavigationItemRole role = FooterRoleForFooterItem(item);
    if (role == page_navigation::NavigationItemRole::kUnknown) {
        return result;
    }

    int old_focus_index = -1;
    int new_focus_index = -1;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const int focus_index = s_coordinator.navigation_model().IndexOfRole(role);
        if (focus_index < 0) {
            return result;
        }
        old_focus_index = s_coordinator.focus().index();
        if (!s_coordinator.SetFocusIndex(focus_index)) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
    }

    result.handled = true;
    result.apply_page_state = true;
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);
    return result;
}

void ResetFocus()
{
    footer_runtime::ProjectionState projection = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.Show();
        projection = BuildFooterProjectionStateLocked();
    }
    footer_runtime::SetProjectionState(projection);
}

esp_err_t ShowButtonSoundsModal()
{
    epaper_ui::SelectModalState state = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        state.visible = true;
        state.title_text = "Button sounds";
        for (const char* label : kButtonSoundsOptions) {
            state.items.push_back({.label_text = label});
        }
        state.selected_index = app_state_service::ButtonSoundsEnabled() ? 0 : 1;
        s_button_sounds_modal_active = true;
        s_lock_screen_modal_active = false;
    }
    return overlay_runtime::ShowSelectModal(state);
}

esp_err_t ShowLockScreenModal()
{
    epaper_ui::SelectModalState state = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        state.visible = true;
        state.title_text = "Lock screen shows";
        for (const char* label : kLockScreenOptions) {
            state.items.push_back({.label_text = label});
        }
        state.selected_index = app_state_service::LockScreenShowsPhoto() ? 1 : 0;
        s_lock_screen_modal_active = true;
        s_button_sounds_modal_active = false;
    }
    return overlay_runtime::ShowSelectModal(state);
}

bool HandleSelectModalSubmit(int selected_index)
{
    bool sounds_active = false;
    bool lock_screen_active = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        sounds_active = s_button_sounds_modal_active;
        lock_screen_active = s_lock_screen_modal_active;
        s_button_sounds_modal_active = false;
        s_lock_screen_modal_active = false;
    }
    if (lock_screen_active) {
        if (selected_index >= 0 &&
            selected_index < static_cast<int>(std::size(kLockScreenOptions))) {
            app_state_service::SetLockScreenShowsPhoto(selected_index == 1);
        }
        (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
        return true;
    }
    if (!sounds_active) {
        return false;
    }
    if (selected_index >= 0 &&
        selected_index < static_cast<int>(std::size(kButtonSoundsOptions))) {
        const bool on = selected_index == 0;
        app_state_service::SetButtonSoundsEnabled(on);
        feedback_service::SetButtonSoundsEnabled(on);
    }
    (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    return true;
}

void ClearPendingSelectModal()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_button_sounds_modal_active = false;
    s_lock_screen_modal_active = false;
}

}  // namespace settings_display_page_runtime

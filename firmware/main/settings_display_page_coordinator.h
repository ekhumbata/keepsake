#ifndef SETTINGS_DISPLAY_PAGE_COORDINATOR_H_
#define SETTINGS_DISPLAY_PAGE_COORDINATOR_H_

#include "epaper_ui/settings_display_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"

// Settings -> Display & sound (#164): what the lock screen shows, and button sounds.
class SettingsDisplayPageCoordinator {
public:
    SettingsDisplayPageCoordinator();

    void Show();
    bool MoveFocus(int delta);
    bool SetFocusIndex(int index);
    bool IsRoleFocused(page_navigation::NavigationItemRole role) const;

    epaper_ui::SettingsDisplayPageState BuildState(bool lock_screen_photo,
                                                   bool button_sounds) const;

    const page_navigation::NavigationModel& navigation_model() const { return navigation_model_; }
    const page_navigation::RovingFocus& focus() const { return focus_; }

private:
    page_navigation::NavigationModel navigation_model_ =
        page_navigation::BuildSettingsDisplayPageNavigationModel();
    page_navigation::RovingFocus focus_{navigation_model_.item_count, 0};
};

#endif  // SETTINGS_DISPLAY_PAGE_COORDINATOR_H_

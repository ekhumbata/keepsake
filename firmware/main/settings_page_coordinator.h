#ifndef SETTINGS_PAGE_COORDINATOR_H_
#define SETTINGS_PAGE_COORDINATOR_H_

#include "epaper_ui/settings_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"

// Settings is a hub: 4 headings (Network/Time/Storage/Todos), each navigating to a dedicated
// sub-page, plus Manual (replay onboarding) as a direct action rather than a heading.
class SettingsPageCoordinator {
public:
    SettingsPageCoordinator();

    void Show();
    bool MoveFocus(int delta);
    bool SetFocusIndex(int index);
    bool IsRoleFocused(page_navigation::NavigationItemRole role) const;

    epaper_ui::SettingsPageState BuildState() const;

    const page_navigation::NavigationModel& navigation_model() const { return navigation_model_; }
    const page_navigation::RovingFocus& focus() const { return focus_; }

private:
    page_navigation::NavigationModel navigation_model_ =
        page_navigation::BuildSettingsPageNavigationModel();
    page_navigation::RovingFocus focus_{navigation_model_.item_count, 0};
};

#endif  // SETTINGS_PAGE_COORDINATOR_H_

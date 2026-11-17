#ifndef EPAPER_UI_SETTINGS_ABOUT_PAGE_H_
#define EPAPER_UI_SETTINGS_ABOUT_PAGE_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "epaper_ui/button.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/status_bar.h"

namespace epaper_ui {

enum class SettingsAboutPageItemId : uint8_t {
    kNone = 0,
    kUpdate,
    kBack,
};

struct SettingsAboutPageState {
    int navigation_focus_index = -1;
    std::string_view title_text = "About";
    // e.g. "Version 0.7.0" -- built at BuildState() time from esp_app_get_description().
    std::string version_text;
    // Updates over the air (#40): one plain sentence, and the button under it ("Check for
    // updates", "Restart to update", ...).
    std::string update_status_text;
    ButtonState update = {};
    // A personal note from the provisioning file (#149), shown in body text. Empty = none.
    std::string personal_note;
    ButtonState back = {};
    // The upstream credits and licence, one small line under Back (#149).
    std::string_view acknowledgment_text =
        "Built on Chroninkle (mach1na) and Followup (ALXV Labs). GNU GPL v3.";
};

UiRect SettingsAboutPageItemBounds(int portrait_width,
                                   int portrait_height,
                                   const SettingsAboutPageState& state,
                                   SettingsAboutPageItemId item);
bool HitTestSettingsAboutPageItem(int portrait_width,
                                  int portrait_height,
                                  const SettingsAboutPageState& state,
                                  int x,
                                  int y,
                                  SettingsAboutPageItemId* item);
void DrawSettingsAboutPage(uint8_t* framebuffer,
                           int raw_width,
                           int raw_height,
                           int portrait_width,
                           int portrait_height,
                           const SettingsAboutPageState& state,
                           const StatusBarState& status_bar_state,
                           const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SETTINGS_ABOUT_PAGE_H_

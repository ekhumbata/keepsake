#ifndef PHOTO_PAGE_RUNTIME_H_
#define PHOTO_PAGE_RUNTIME_H_

#include "display_service.h"
#include "esp_err.h"
#include "footer_runtime.h"
#include "page_action_result.h"

// The photo viewer (#136): one photo full screen, rocker up/down to flip (wrapping), rocker
// press to hide/show the caption, a quick Record press back to Home.
namespace photo_page_runtime {

// Rescans /photos and opens the photo last looked at. Call before showing the screen.
esp_err_t Open();

esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(const display_service::RefreshRequest& request);

// UP/DOWN flip photos (delta > 0 next). Blocks while an unprepared photo is prepared.
page_actions::FocusMoveOutcome MoveFocus(int delta);
bool ToggleCaption();

footer_runtime::ProjectionState BuildFooterProjectionState();  // always empty
void ResetFocus();

// Back to Home, after input dispatch (app_shell polls ConsumePendingBack).
void RequestBack();
bool ConsumePendingBack();

}  // namespace photo_page_runtime

#endif  // PHOTO_PAGE_RUNTIME_H_

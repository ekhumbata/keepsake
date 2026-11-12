#include "photo_page_runtime.h"

#include <atomic>
#include <mutex>
#include <string>

#include "cpu_boost.h"
#include "epaper_ui/book_page.h"
#include "epaper_ui/photo_page.h"
#include "esp_log.h"
#include "photo_service.h"
#include "project_assets.h"
#include "ui_refresh_runtime.h"

namespace photo_page_runtime {
namespace {

constexpr const char* kTag = "PhotoPage";

std::mutex s_mutex;
epaper_ui::PhotoPageState s_state;
int s_index = 0;
int s_count = 0;
bool s_show_caption = true;
std::atomic<bool> s_back_requested{false};

std::string FormatPosition(int index, int count)
{
    return std::to_string(index + 1) + " / " + std::to_string(count);
}

// Builds the state for photo `index` (loading it). Holds no lock while loading.
epaper_ui::PhotoPageState BuildState(int index, int count, bool show_caption)
{
    epaper_ui::PhotoPageState state;
    state.show_caption = show_caption;
    if (count == 0) {
        state.message_title = "No photos yet";
        state.message_body = "Drop some into the photos folder on the SD card (Settings > Storage "
                             "> Enable OTG).";
        state.message_image = project_assets::GetImage(EmbeddedImageId::kMascotMochiHappy);
        return state;
    }
    state.position_text = FormatPosition(index, count);
    state.caption_text = epaper_ui::ToDisplayText(photo_service::CaptionAt(index).Text());
    cpu_boost::ScopedBoost boost;  // someone is waiting for this photo
    const photo_service::Loaded loaded = photo_service::Load(index);
    if (loaded.result == photo::Result::kOk && loaded.bitmap) {
        state.photo = loaded.bitmap;
        return state;
    }
    state.message_image = project_assets::GetImage(EmbeddedImageId::kMascotSleepy);
    state.message_title = "Mochi can't open this one";
    switch (loaded.result) {
        case photo::Result::kUnsupported:
            state.message_body = "It's a " + loaded.format +
                                 " photo. Save it as JPEG and drop it in again.";
            break;
        case photo::Result::kTooBig:
            state.message_body = "It's too big. Save a smaller copy (or use the photo tool) and "
                                 "drop it in again.";
            break;
        default:
            state.message_body = "The file looks damaged. Try copying it again.";
            break;
    }
    return state;
}

void Show(int index)
{
    int count = 0;
    bool show_caption = true;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        count = s_count;
        show_caption = s_show_caption;
    }
    const epaper_ui::PhotoPageState state = BuildState(index, count, show_caption);
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_index = index;
        s_state = state;
    }
    if (count > 0) {
        photo_service::RememberIndex(index);
        // The neighbours, so the next flip is quick.
        photo_service::Prefetch((index + 1) % count);
        photo_service::Prefetch((index + count - 1) % count);
    }
}

}  // namespace

esp_err_t Open()
{
    const int count = photo_service::Rescan();
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_count = count;
        s_show_caption = true;
    }
    Show(count > 0 ? photo_service::LastIndex() : 0);
    ESP_LOGI(kTag, "opened at photo %d of %d", s_index + 1, count);
    return UpdateDisplayState();
}

esp_err_t UpdateDisplayState()
{
    epaper_ui::PhotoPageState state;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        state = s_state;
    }
    return display_service::SetPhotoPageState(state);
}

esp_err_t UpdateDisplayStateAndRequestRefresh(const display_service::RefreshRequest& request)
{
    return ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kPhotoPage,
                                        &UpdateDisplayState, request);
}

page_actions::FocusMoveOutcome MoveFocus(int delta)
{
    page_actions::FocusMoveOutcome outcome = {};
    int index = 0;
    int count = 0;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        index = s_index;
        count = s_count;
    }
    if (count <= 1 || delta == 0) {
        return outcome;
    }
    Show(((index + (delta > 0 ? 1 : -1)) % count + count) % count);
    outcome.handled = true;
    outcome.play_navigation_cue = true;
    outcome.apply_page_state = true;
    return outcome;
}

bool ToggleCaption()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_count == 0) {
        return false;
    }
    s_show_caption = !s_show_caption;
    s_state.show_caption = s_show_caption;
    return true;
}

footer_runtime::ProjectionState BuildFooterProjectionState()
{
    return {};
}

void ResetFocus() {}

void RequestBack()
{
    s_back_requested = true;
}

bool ConsumePendingBack()
{
    return s_back_requested.exchange(false);
}

}  // namespace photo_page_runtime

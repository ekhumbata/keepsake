#include "book_reader_page_runtime.h"

#include <atomic>
#include <mutex>
#include <vector>

#include "book_reader_page_coordinator.h"
#include "esp_log.h"
#include "overlay_runtime.h"
#include "text_reader_service.h"
#include "ui_refresh_runtime.h"

namespace book_reader_page_runtime {
namespace {

std::mutex s_mutex;
BookReaderPageCoordinator s_coordinator = {};
std::atomic<bool> s_pending_back{false};

enum class MenuAction : uint8_t { kContents, kBackToBooks, kClose };
enum class MenuStage : uint8_t { kNone, kMenu, kContents };
MenuStage s_menu_stage = MenuStage::kNone;
std::vector<MenuAction> s_menu_actions;

epaper_ui::BookReaderPageState BuildStateLocked()
{
    return s_coordinator.BuildState();
}

void SavePositionLocked()
{
    s_coordinator.SavePosition();
}

// Moves the book to the top of the shelf with its current progress. Called on open and on
// leaving the reader, not per page turn (that's one SD write per turn already, for .pos).
void RecordOpenedLocked()
{
    if (!s_coordinator.filename().empty()) {
        text_reader_service::RecordOpened(s_coordinator.filename(),
                                          s_coordinator.ProgressPercent());
    }
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetBookReaderPageState(BuildStateLocked());
}

esp_err_t UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode refresh_mode)
{
    return UpdateDisplayStateAndRequestRefresh(
        display_service::RefreshRequest{.refresh_mode = refresh_mode});
}

esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request)
{
    return ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kBookReaderPage,
                                        &UpdateDisplayState, refresh_request);
}

esp_err_t OpenBook(const std::string& filename)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.Show(filename, display_service::PortraitWidth(),
                           display_service::PortraitHeight());
        SavePositionLocked();
        RecordOpenedLocked();
    }
    return UpdateDisplayState();
}

page_actions::FocusMoveOutcome MoveFocus(int delta)
{
    page_actions::FocusMoveOutcome result = {};
    bool turned = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        turned = s_coordinator.TurnPage(delta, display_service::PortraitWidth(),
                                        display_service::PortraitHeight());
        if (turned) {
            SavePositionLocked();
        }
    }
    if (!turned) {
        return result;
    }
    result.handled = true;
    result.play_navigation_cue = true;
    result.apply_page_state = true;
    result.sync_footer_projection = false;
    return result;
}

footer_runtime::ProjectionState BuildFooterProjectionState()
{
    return {};
}

void ResetFocus()
{
    // Nothing to reset here: opening a specific book happens via OpenBook, called explicitly
    // right before this screen is shown (see app_shell's ShowBookReaderScreenIfRequested) --
    // ResetFocusForScreen's generic no-argument signature can't carry which file to open.
}

bool ExitActiveControl()
{
    RequestBack();
    return true;
}

void RequestBack()
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        RecordOpenedLocked();
    }
    s_pending_back.store(true, std::memory_order_relaxed);
}

bool ConsumePendingBack()
{
    return s_pending_back.exchange(false, std::memory_order_relaxed);
}

bool ShowMenu()
{
    epaper_ui::SelectModalState modal = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        modal.title_text = s_coordinator.MenuTitle();
        s_menu_actions.clear();
        if (!s_coordinator.ContentsLabels().empty()) {
            modal.items.push_back({"Contents"});
            s_menu_actions.push_back(MenuAction::kContents);
        }
        modal.items.push_back({"Back to books"});
        s_menu_actions.push_back(MenuAction::kBackToBooks);
        modal.items.push_back({"Close"});
        s_menu_actions.push_back(MenuAction::kClose);
        modal.selected_index = 0;
        s_menu_stage = MenuStage::kMenu;
    }
    const esp_err_t err = overlay_runtime::ShowSelectModal(modal);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_menu_stage = MenuStage::kNone;
        ESP_LOGW("BookReaderRuntime", "Show reader menu failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

namespace {

bool ShowContentsLocked(epaper_ui::SelectModalState* modal)
{
    const std::vector<std::string> labels = s_coordinator.ContentsLabels();
    if (labels.empty()) {
        return false;
    }
    modal->title_text = "Contents";
    for (const std::string& label : labels) {
        modal->items.push_back({label});
    }
    modal->selected_index = s_coordinator.CurrentContentsIndex();
    s_menu_stage = MenuStage::kContents;
    return true;
}

}  // namespace

bool HandleMenuSelection(int selected_index)
{
    MenuStage stage = MenuStage::kNone;
    MenuAction action = MenuAction::kClose;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        stage = s_menu_stage;
        s_menu_stage = MenuStage::kNone;
        if (stage == MenuStage::kNone) {
            return false;
        }
        if (stage == MenuStage::kMenu) {
            if (selected_index < 0 || selected_index >= static_cast<int>(s_menu_actions.size())) {
                return true;  // dismissed
            }
            action = s_menu_actions[static_cast<size_t>(selected_index)];
        }
    }

    if (stage == MenuStage::kContents) {
        bool moved = false;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            if (selected_index >= 0) {
                moved = s_coordinator.GoToContentsEntry(static_cast<size_t>(selected_index));
                if (moved) {
                    SavePositionLocked();
                }
            }
        }
        if (moved) {
            (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
        }
        return true;
    }

    switch (action) {
        case MenuAction::kContents: {
            epaper_ui::SelectModalState modal = {};
            bool show = false;
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                show = ShowContentsLocked(&modal);
            }
            if (show) {
                const esp_err_t err = overlay_runtime::ShowSelectModal(modal);
                if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_menu_stage = MenuStage::kNone;
                }
            }
            break;
        }
        case MenuAction::kBackToBooks:
            RequestBack();
            break;
        case MenuAction::kClose:
        default:
            break;
    }
    return true;
}

}  // namespace book_reader_page_runtime

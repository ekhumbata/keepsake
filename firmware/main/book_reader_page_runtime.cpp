#include "book_reader_page_runtime.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "app_state_service.h"
#include "book_reader_page_coordinator.h"
#include "cpu_boost.h"
#include "epaper_ui/book_page.h"
#include "epaper_ui/font_renderer.h"
#include "esp_log.h"
#include "library/library.h"
#include "overlay_runtime.h"
#include "text_reader_service.h"
#include "ui_refresh_runtime.h"

namespace book_reader_page_runtime {
namespace {

std::mutex s_mutex;
BookReaderPageCoordinator s_coordinator = {};
std::atomic<bool> s_pending_back{false};

enum class MenuAction : uint8_t {
    kContents,
    kToggleBookmark,
    kBookmarks,
    kGoTo,
    kTextSize,
    kBackToBooks,
    kClose,
};
enum class MenuStage : uint8_t { kNone, kMenu, kContents, kBookmarks, kGoTo, kTextSize };
// Go to... offers 0%, 10%, ... 100%.
constexpr int kGoToStepPercent = 10;
constexpr int kGoToChoices = 100 / kGoToStepPercent + 1;
MenuStage s_menu_stage = MenuStage::kNone;
std::vector<MenuAction> s_menu_actions;

// The open book's bookmarks (#28), loaded on open and saved on every change.
std::vector<library::Bookmark> s_bookmarks;

epaper_ui::BookReaderPageState BuildStateLocked()
{
    epaper_ui::BookReaderPageState state = s_coordinator.BuildState();
    state.bookmarked = s_coordinator.BookmarkOnPage(s_bookmarks) >= 0;
    return state;
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
        epaper_ui::SetBookTextSize(
            static_cast<epaper_ui::BookTextSize>(app_state_service::ReaderTextSize()));
        s_coordinator.Show(filename, display_service::PortraitWidth(),
                           display_service::PortraitHeight());
        s_bookmarks = text_reader_service::LoadBookmarks(filename);
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

page_actions::FocusMoveOutcome JumpChapter(int direction)
{
    page_actions::FocusMoveOutcome result = {};
    bool moved = false;
    {
        const cpu_boost::ScopedBoost boost("chapter");
        std::lock_guard<std::mutex> lock(s_mutex);
        moved = s_coordinator.JumpChapter(direction);
        if (moved) {
            SavePositionLocked();
        }
    }
    if (!moved) {
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
        if (s_coordinator.CanBookmark()) {
            const bool on_page = s_coordinator.BookmarkOnPage(s_bookmarks) >= 0;
            modal.items.push_back({on_page ? "Remove bookmark" : "Add bookmark"});
            s_menu_actions.push_back(MenuAction::kToggleBookmark);
        }
        if (!s_bookmarks.empty()) {
            modal.items.push_back({"Bookmarks (" + std::to_string(s_bookmarks.size()) + ")"});
            s_menu_actions.push_back(MenuAction::kBookmarks);
        }
        modal.items.push_back({"Go to..."});
        s_menu_actions.push_back(MenuAction::kGoTo);
        modal.items.push_back(
            {std::string("Text size: ") + epaper_ui::BookTextSizeName(epaper_ui::GetBookTextSize())});
        s_menu_actions.push_back(MenuAction::kTextSize);
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

void ShowBookmarksLocked(epaper_ui::SelectModalState* modal)
{
    modal->title_text = "Bookmarks";
    const library::Bookmark here = s_coordinator.CurrentBookmark();
    int nearest = 0;
    for (size_t i = 0; i < s_bookmarks.size(); ++i) {
        modal->items.push_back(
            {epaper_ui::ToDisplayText(library::BookmarkListLabel(s_bookmarks[i]))});
        if (!library::BookmarkBefore(here, s_bookmarks[i])) {
            nearest = static_cast<int>(i);  // the last one at or before this page
        }
    }
    modal->selected_index = nearest;
    s_menu_stage = MenuStage::kBookmarks;
}

// Adds a bookmark for this page, or removes the one that's on it.
void ToggleBookmarkLocked()
{
    const int on_page = s_coordinator.BookmarkOnPage(s_bookmarks);
    if (on_page >= 0) {
        s_bookmarks.erase(s_bookmarks.begin() + on_page);
    } else if (!library::AddBookmark(&s_bookmarks, s_coordinator.CurrentBookmark())) {
        ESP_LOGW("BookReaderRuntime", "Bookmark list is full");
        return;
    }
    text_reader_service::SaveBookmarks(s_coordinator.filename(), s_bookmarks);
}

void ShowGoToLocked(epaper_ui::SelectModalState* modal)
{
    modal->title_text = "Go to";
    for (int i = 0; i < kGoToChoices; ++i) {
        modal->items.push_back({std::to_string(i * kGoToStepPercent) + "%"});
    }
    const int now = std::max(0, s_coordinator.ProgressPercent());
    modal->selected_index =
        std::min(kGoToChoices - 1, (now + kGoToStepPercent / 2) / kGoToStepPercent);
    s_menu_stage = MenuStage::kGoTo;
}

void ShowTextSizesLocked(epaper_ui::SelectModalState* modal)
{
    modal->title_text = "Text size";
    for (int i = 0; i < epaper_ui::kBookTextSizeCount; ++i) {
        modal->items.push_back(
            {epaper_ui::BookTextSizeName(static_cast<epaper_ui::BookTextSize>(i))});
    }
    modal->selected_index = static_cast<int>(epaper_ui::GetBookTextSize());
    s_menu_stage = MenuStage::kTextSize;
}

// Shows a follow-up menu; if it can't be shown, nothing is waiting on a selection any more.
void ShowSubmenu(const epaper_ui::SelectModalState& modal)
{
    const esp_err_t err = overlay_runtime::ShowSelectModal(modal);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_menu_stage = MenuStage::kNone;
    }
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

    if (stage == MenuStage::kBookmarks) {
        bool moved = false;
        {
            const cpu_boost::ScopedBoost boost("bookmark");
            std::lock_guard<std::mutex> lock(s_mutex);
            if (selected_index >= 0 && selected_index < static_cast<int>(s_bookmarks.size())) {
                moved = s_coordinator.GoToBookmark(
                    s_bookmarks[static_cast<size_t>(selected_index)],
                    display_service::PortraitWidth(), display_service::PortraitHeight());
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

    if (stage == MenuStage::kGoTo) {
        if (selected_index < 0 || selected_index >= kGoToChoices) {
            return true;  // dismissed
        }
        bool moved = false;
        {
            const cpu_boost::ScopedBoost boost("go to");
            std::lock_guard<std::mutex> lock(s_mutex);
            moved = s_coordinator.GoToPercent(selected_index * kGoToStepPercent,
                                              display_service::PortraitWidth(),
                                              display_service::PortraitHeight());
            if (moved) {
                SavePositionLocked();
            }
        }
        if (moved) {
            (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
        }
        return true;
    }

    if (stage == MenuStage::kTextSize) {
        if (selected_index < 0 || selected_index >= epaper_ui::kBookTextSizeCount ||
            selected_index == static_cast<int>(epaper_ui::GetBookTextSize())) {
            return true;  // dismissed, or no change
        }
        {
            // Re-laying out the chapter is work the reader is waiting on.
            const cpu_boost::ScopedBoost boost("text size");
            std::lock_guard<std::mutex> lock(s_mutex);
            epaper_ui::SetBookTextSize(static_cast<epaper_ui::BookTextSize>(selected_index));
            app_state_service::SetReaderTextSize(static_cast<uint8_t>(selected_index));
            s_coordinator.Relayout(display_service::PortraitWidth(),
                                   display_service::PortraitHeight());
            SavePositionLocked();
        }
        (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
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
                ShowSubmenu(modal);
            }
            break;
        }
        case MenuAction::kToggleBookmark: {
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                ToggleBookmarkLocked();
            }
            // The ribbon appears or goes away.
            (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
            break;
        }
        case MenuAction::kBookmarks: {
            epaper_ui::SelectModalState modal = {};
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                ShowBookmarksLocked(&modal);
            }
            ShowSubmenu(modal);
            break;
        }
        case MenuAction::kGoTo: {
            epaper_ui::SelectModalState modal = {};
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                ShowGoToLocked(&modal);
            }
            ShowSubmenu(modal);
            break;
        }
        case MenuAction::kTextSize: {
            epaper_ui::SelectModalState modal = {};
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                ShowTextSizesLocked(&modal);
            }
            ShowSubmenu(modal);
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

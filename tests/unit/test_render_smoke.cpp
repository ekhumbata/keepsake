// Render smoke tests: draw real pages into a panel-sized framebuffer under ASan/UBSan.
// They catch out-of-bounds writes and crashes, and check that a page actually draws
// something; pixel-exact golden images are the sim gallery's job (#14).
#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "epaper_ui/book_list_page.h"
#include "epaper_ui/book_reader_page.h"
#include "epaper_ui/dashboard_page.h"
#include "epaper_ui/lock_screen.h"

namespace {

// The SSD1677 is 800x480 landscape ("raw"); the UI is laid out in 480x800 portrait.
constexpr int kRawWidth = 800;
constexpr int kRawHeight = 480;
constexpr int kPortraitWidth = 480;
constexpr int kPortraitHeight = 800;
constexpr size_t kFramebufferBytes = kRawWidth * kRawHeight / 8;

// Guard bytes on both sides so a stray write just outside the buffer is caught even
// without ASan (ASan also catches it, since the vector owns the whole allocation).
constexpr size_t kGuardBytes = 64;
constexpr uint8_t kGuardValue = 0xA5;

struct Canvas {
    std::vector<uint8_t> storage =
        std::vector<uint8_t>(kFramebufferBytes + 2 * kGuardBytes, kGuardValue);

    uint8_t* framebuffer() { return storage.data() + kGuardBytes; }

    bool GuardsIntact() const
    {
        for (size_t i = 0; i < kGuardBytes; ++i) {
            if (storage[i] != kGuardValue ||
                storage[storage.size() - 1 - i] != kGuardValue) {
                return false;
            }
        }
        return true;
    }

    // 1 bits are white on this panel, so a fully white page is all 0xFF.
    size_t BlackPixelBytes()
    {
        size_t count = 0;
        for (size_t i = 0; i < kFramebufferBytes; ++i) {
            if (framebuffer()[i] != 0xFF) {
                ++count;
            }
        }
        return count;
    }
};

epaper_ui::StatusBarState MakeStatusBar()
{
    epaper_ui::StatusBarState status = {};
    status.battery.percent = 73;
    status.time_text = "9:41";
    return status;
}

epaper_ui::GlobalFooterState MakeFooter()
{
    epaper_ui::GlobalFooterState footer = {};
    footer.visible = true;
    return footer;
}

}  // namespace

TEST_CASE("the book reader page draws inside the framebuffer")
{
    Canvas canvas;
    epaper_ui::BookReaderPageState state = {};
    state.title_text = "A Very Long Book Title That Will Certainly Need To Be Shortened To Fit";
    state.page_number = 12;
    state.page_lines = epaper_ui::PaginateBookText(
                           kPortraitWidth, kPortraitHeight,
                           "The reader draws whatever lines it is handed, one per row, until "
                           "the content area is full. This sentence is only here to fill a "
                           "few of those rows with something to look at.")
                           .lines;
    // More lines than fit: the page must stop at its content area, not run off the panel.
    for (int i = 0; i < 100; ++i) {
        state.page_lines.push_back("overflow line " + std::to_string(i));
    }

    epaper_ui::DrawBookReaderPage(canvas.framebuffer(), kRawWidth, kRawHeight, kPortraitWidth,
                                  kPortraitHeight, state, MakeStatusBar(), MakeFooter());
    CHECK(canvas.GuardsIntact());
    CHECK(canvas.BlackPixelBytes() > 0);
}

TEST_CASE("the book list page draws with an empty and a full shelf")
{
    for (int count : {0, 3, 50}) {
        CAPTURE(count);
        Canvas canvas;
        epaper_ui::BookListPageState state = {};
        for (int i = 0; i < count; ++i) {
            epaper_ui::SelectItemState item = {};
            item.label_text = "book_" + std::to_string(i) + "_with_a_rather_long_file_name.txt";
            state.books_list.items.push_back(item);
        }
        state.books_list.focused = count > 0;
        state.navigation_focus_index = count > 0 ? 0 : -1;
        epaper_ui::DrawBookListPage(canvas.framebuffer(), kRawWidth, kRawHeight, kPortraitWidth,
                                    kPortraitHeight, state, MakeStatusBar(), MakeFooter());
        CHECK(canvas.GuardsIntact());
        CHECK(canvas.BlackPixelBytes() > 0);
    }
}

TEST_CASE("the lock screen draws, including its shutdown variant")
{
    for (bool for_shutdown : {false, true}) {
        CAPTURE(for_shutdown);
        Canvas canvas;
        epaper_ui::LockScreenState state = {};
        state.weekday_text = "Wednesday";
        state.date_text = "November 11";
        state.pending_todo_titles = {
            "Water the plants",
            "A much longer todo whose text goes on and on so that it has to wrap onto more "
            "than one line and then be capped",
            "Return the library book",
        };
        state.pending_todo_count = 7;
        state.for_shutdown = for_shutdown;
        epaper_ui::DrawLockScreen(canvas.framebuffer(), kRawWidth, kRawHeight, kPortraitWidth,
                                  kPortraitHeight, state, MakeStatusBar());
        CHECK(canvas.GuardsIntact());
        CHECK(canvas.BlackPixelBytes() > 0);
    }
}

TEST_CASE("the home dashboard draws with every menu item focused in turn")
{
    for (int focus = -1; focus < 8; ++focus) {
        CAPTURE(focus);
        Canvas canvas;
        epaper_ui::DashboardPageState state = {};
        state.navigation_focus_index = focus;
        epaper_ui::DrawDashboardPage(canvas.framebuffer(), kRawWidth, kRawHeight, kPortraitWidth,
                                     kPortraitHeight, state, MakeStatusBar(), MakeFooter());
        CHECK(canvas.GuardsIntact());
        CHECK(canvas.BlackPixelBytes() > 0);
    }
}

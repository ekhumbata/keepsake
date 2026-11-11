#ifndef EPAPER_UI_BOOK_LIST_PAGE_H_
#define EPAPER_UI_BOOK_LIST_PAGE_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "cover_image/cover_image.h"

#include "epaper_ui/global_footer.h"
#include "epaper_ui/select_list.h"
#include "epaper_ui/status_bar.h"

namespace epaper_ui {

// "Continue reading" (#27): the book being read, shown above the shelf with its cover.
struct ContinueReadingState {
    bool visible = false;
    std::string title_text = {};
    std::string author_text = {};
    int progress_percent = 0;
    // Dithered cover thumbnail, at most kContinueCoverWidth x kContinueCoverHeight; empty for
    // books without one (a plain placeholder is drawn).
    cover_image::Thumbnail cover = {};

    bool operator==(const ContinueReadingState& other) const = default;
};

inline constexpr int kContinueCoverWidth = 84;
inline constexpr int kContinueCoverHeight = 126;

struct BookListPageState {
    int navigation_focus_index = -1;
    ContinueReadingState continue_reading = {};
    std::string_view title_text = "Books";
    // One row per .txt file in the configured folder (label_text = filename). Picking one
    // opens the reader at that book's saved (or start-of-file) position.
    SelectListState books_list = {};

    bool operator==(const BookListPageState& other) const = default;
};

UiRect BookListListBounds(int portrait_width, int portrait_height, const BookListPageState& state);
void DrawBookListPage(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      const BookListPageState& state,
                      const StatusBarState& status_bar_state,
                      const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_BOOK_LIST_PAGE_H_

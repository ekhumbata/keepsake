#ifndef EPAPER_UI_BOOK_READER_PAGE_H_
#define EPAPER_UI_BOOK_READER_PAGE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "book_layout/book_layout.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/overlay_geometry.h"
#include "epaper_ui/status_bar.h"

namespace epaper_ui {

struct BookReaderPageState {
    std::string title_text = "Reader";
    // Pre-wrapped lines for the current page, from PaginateBookText -- this page just draws
    // them rather than re-wrapping.
    std::vector<std::string> page_lines = {};
    // In-session page counter (1-based); not persisted, resets to 1 each time a book is opened.
    int page_number = 1;
    // EPUBs: a laid-out page (book_layout) drawn instead of page_lines, and the indicator
    // text to show instead of "Page N" (e.g. "Chapter 3 - 4/12 - 42%").
    bool use_layout_page = false;
    book_layout::Page layout_page = {};
    std::string page_label = {};

    bool operator==(const BookReaderPageState& other) const = default;
};

struct BookReaderPageLayout {
    std::vector<std::string> lines = {};
    // Bytes of the input `text` consumed producing `lines` -- main/'s coordinator uses this to
    // know where in the file the *next* page should start reading from, since it feeds this
    // function whatever chunk it has read off SD rather than the whole book at once.
    size_t consumed_bytes = 0;
    // True once `text` ran out before the page's line budget was filled -- the coordinator
    // should read another chunk, append it, and call again rather than treating this as the
    // end of the book (that's decided by whether the file itself has more bytes past here).
    bool text_exhausted = false;
};

// Greedily word-wraps `text` (word-wrap only; this is a reflowing text reader, not a formatted
// one -- original line breaks are not preserved) into whatever whole lines fit the reader's
// content area, stopping once that line budget is filled or `text` runs out.
BookReaderPageLayout PaginateBookText(int portrait_width, int portrait_height,
                                      const std::string& text);

// The rectangle page text is drawn into (portrait coordinates): what EPUB layout targets.
UiRect BookReaderContentBounds(int portrait_width, int portrait_height);

void DrawBookReaderPage(uint8_t* framebuffer,
                        int raw_width,
                        int raw_height,
                        int portrait_width,
                        int portrait_height,
                        const BookReaderPageState& state,
                        const StatusBarState& status_bar_state,
                        const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_BOOK_READER_PAGE_H_

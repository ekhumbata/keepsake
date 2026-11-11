// libFuzzer target: PaginateBookText on arbitrary bytes (any .txt the friend copies over USB,
// cut at any chunk boundary). Checks the invariants the reader coordinator relies on.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "epaper_ui/book_reader_page.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1) {
        return 0;
    }
    // First byte picks one of a few page heights, so short and tall pages both get covered.
    static constexpr int kHeights[] = {800, 600, 300, 120};
    const int height = kHeights[data[0] % 4];
    const std::string text(reinterpret_cast<const char*>(data + 1), size - 1);

    const auto layout = epaper_ui::PaginateBookText(480, height, text);
    if (layout.consumed_bytes > text.size()) {
        std::abort();
    }
    // Not exhausted means there is text left past the page, so the reader can page on.
    if (!layout.text_exhausted && layout.consumed_bytes >= text.size()) {
        std::abort();
    }
    // A page that drew lines consumed bytes and set the flag to match. A page too short for
    // any line (the 120 px case) legitimately reports "exhausted" having consumed nothing.
    if (!layout.lines.empty()) {
        if (layout.consumed_bytes == 0 ||
            layout.text_exhausted != (layout.consumed_bytes >= text.size())) {
            std::abort();
        }
    }
    return 0;
}

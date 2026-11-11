// libFuzzer target: the small files the reader keeps on the SD card (.library index and
// per-book bookmarks). They can be damaged by a pulled card or edited on a computer.
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "library/library.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::string_view text(reinterpret_cast<const char*>(data), size);

    const auto records = library::ParseIndex(text);
    if (library::ParseIndex(library::SerializeIndex(records)) != records) {
        __builtin_trap();  // whatever parses must survive a round trip
    }

    const auto bookmarks = library::ParseBookmarks(text);
    if (bookmarks.size() > library::kMaxBookmarks) {
        __builtin_trap();
    }
    for (size_t i = 1; i < bookmarks.size(); ++i) {
        if (!library::BookmarkBefore(bookmarks[i - 1], bookmarks[i])) {
            __builtin_trap();  // strictly in reading order, no duplicates
        }
    }
    if (library::ParseBookmarks(library::SerializeBookmarks(bookmarks)) != bookmarks) {
        __builtin_trap();
    }
    for (const auto& bookmark : bookmarks) {
        (void)library::BookmarkListLabel(bookmark);
    }
    return 0;
}

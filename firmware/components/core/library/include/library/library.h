#ifndef LIBRARY_LIBRARY_H_
#define LIBRARY_LIBRARY_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// What the Books screen knows about each book, and how it's cached on the SD card (#27).
// Portable: text_reader_service does the file system work and calls into this.
namespace library {

struct BookRecord {
    std::string path;            // relative to the books folder, '/'-separated ("Austen/Emma.epub")
    std::string title;           // from the book's metadata, or made from the file name
    std::string author;          // may be empty
    uint64_t size_bytes = 0;     // with modified_time, tells whether the cached metadata is stale
    int64_t modified_time = 0;   // seconds since the epoch, as the file system reports it
    int64_t last_opened = 0;     // seconds since the epoch; 0 = never opened
    int progress_percent = -1;   // -1 = not started

    bool operator==(const BookRecord& other) const = default;
};

// "the_lighthouse-keepers_cat.epub" -> "The lighthouse keepers cat": drops folders and the
// extension, turns '_' and '-' into spaces, collapses spaces, capitalizes the first letter.
std::string TitleFromPath(std::string_view path);

// Lower-cased extension without the dot ("epub", "txt"), or "".
std::string Extension(std::string_view path);

// The index file: one record per line, tab-separated, with \t \n \\ escaped, under a version
// header. Unknown versions and malformed lines are skipped, never fatal: the index is only a
// cache and can always be rebuilt from the books themselves.
std::string SerializeIndex(const std::vector<BookRecord>& records);
std::vector<BookRecord> ParseIndex(std::string_view text);

// Shelf order: books being read (most recently opened first), then the rest by title,
// case-insensitively, then by path.
void SortForShelf(std::vector<BookRecord>* records);

// "Emma - 42%" for a started book, "Emma" otherwise. Progress shows as "Done" at 100%.
std::string ShelfLabel(const BookRecord& record);

// Cover cache file name (#27) for a book as it is now: changes when the book's size or
// modification time changes (or the thumbnail size), so a replaced book gets a fresh cover.
// "<16 hex digits>.kcov".
std::string CoverCacheName(std::string_view path, uint64_t size_bytes, int64_t modified_time,
                           int width, int height);

// A bookmark (#28): a place in one book. For an EPUB the place is spine item + block + byte
// offset in the block (book_reader::ReadingPosition); for a .txt it's the byte offset alone
// (spine and block 0). Either way, places sort in reading order.
struct Bookmark {
    uint32_t spine = 0;
    uint32_t block = 0;
    uint64_t offset = 0;
    int percent = 0;      // how far through the book, when it was made
    std::string label;    // what to show in the list: chapter title or the page's first words

    bool operator==(const Bookmark& other) const = default;
};

inline constexpr size_t kMaxBookmarks = 100;

// True when `a` comes before `b` in the book.
bool BookmarkBefore(const Bookmark& a, const Bookmark& b);

// Adds `bookmark` in reading order, replacing one at the same place. False when the list is
// already at kMaxBookmarks (and the place is new).
bool AddBookmark(std::vector<Bookmark>* bookmarks, Bookmark bookmark);

// "41% - Chapter Two", or "41%" without a label.
std::string BookmarkListLabel(const Bookmark& bookmark);

// The per-book bookmarks file: a version header, then one bookmark per line, tab-separated,
// escaped like the index. Malformed lines are skipped; the result is in reading order, at
// most kMaxBookmarks.
std::string SerializeBookmarks(const std::vector<Bookmark>& bookmarks);
std::vector<Bookmark> ParseBookmarks(std::string_view text);

}  // namespace library

#endif  // LIBRARY_LIBRARY_H_

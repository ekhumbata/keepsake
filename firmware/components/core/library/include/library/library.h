#ifndef LIBRARY_LIBRARY_H_
#define LIBRARY_LIBRARY_H_

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

}  // namespace library

#endif  // LIBRARY_LIBRARY_H_

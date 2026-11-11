#ifndef TEXT_READER_SERVICE_H_
#define TEXT_READER_SERVICE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "esp_err.h"

namespace text_reader_service {

struct BookEntry {
    // Path relative to the books folder ("Austen/Emma.epub"); what every other call takes.
    std::string filename = {};
    uint64_t size_bytes = 0;
    // From the book's metadata (EPUB) or its file name, cached in the folder's ".library".
    std::string title = {};
    std::string author = {};
    int progress_percent = -1;  // -1 = not started
    int64_t last_opened = 0;    // seconds since the epoch; 0 = never
};

// Ensures the configured book folder exists on the SD card. Safe to call repeatedly, and
// safe to call when the card isn't mounted yet -- the next mount just leaves the folder
// missing until this (or ListBooks) is called again.
esp_err_t Init();

// Book files (".txt" and ".epub") in the configured folder and its subfolders (up to 4 levels;
// hidden files and folders skipped), in shelf order: books being read first (most recently
// opened first), then by title. Titles and authors come from a cached index (".library" in
// the folder), refreshed for any book that is new or changed since it was cached. Empty when
// the folder is missing or the card isn't mounted.
std::vector<BookEntry> ListBooks();

// Records that `filename` was just read, with how far through it the reader is (0..100), so
// the shelf can put it first and show progress. Cheap to call when leaving the reader.
void RecordOpened(const std::string& filename, int progress_percent);

// Reads up to `length` bytes of `filename` (a bare filename from ListBooks, resolved
// against the configured folder) starting at `offset` into `buffer`. Returns the number
// of bytes actually read -- 0 at/after end of file, or on any error.
size_t ReadChunk(const std::string& filename, size_t offset, char* buffer, size_t length);

uint64_t GetFileSize(const std::string& filename);

// Sidecar "<filename>.pos" position, in bytes, next to the book's own entry in the
// folder. Returns 0 (start of file) when no sidecar exists yet, or when the saved offset
// is out of range for the file's current size -- the caller re-paginates from wherever
// this clamps to.
size_t LoadPosition(const std::string& filename);
void SavePosition(const std::string& filename, size_t offset);

// EPUBs: true for a ".epub" filename.
bool IsEpub(const std::string& filename);
// Absolute path of a book from ListBooks (for the EPUB engine, which opens files itself).
std::string FullPath(const std::string& filename);

// An EPUB's place, independent of font size: spine item + block + byte offset in the block
// (book_reader::ReadingPosition). Stored in the same "<filename>.pos" sidecar as
// "epub <spine> <block> <offset>"; LoadPosition() reads that line as 0, so the formats
// can't be confused.
struct EpubPosition {
    uint32_t spine = 0;
    uint32_t block = 0;
    uint32_t offset = 0;
};
bool LoadEpubPosition(const std::string& filename, EpubPosition* out);
void SaveEpubPosition(const std::string& filename, const EpubPosition& position);

// Removes the book and its position sidecar (if any) from the configured folder. Returns
// true if the book file was removed or was already gone; false on an actual I/O error.
bool DeleteBook(const std::string& filename);

}  // namespace text_reader_service

#endif  // TEXT_READER_SERVICE_H_

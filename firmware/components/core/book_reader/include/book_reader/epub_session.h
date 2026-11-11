#ifndef BOOK_READER_EPUB_SESSION_H_
#define BOOK_READER_EPUB_SESSION_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "book_layout/book_layout.h"
#include "epub_engine/epub_book.h"

// One open EPUB in the reader: the book, the current chapter laid out into pages, and where
// the reader is. Portable (no ESP-IDF), so it's tested on the host against the fixture shelf;
// firmware/main's reader coordinator is a thin adapter around it.
namespace book_reader {

// Font-independent place in a book: spine item + book_layout::Position inside it.
struct ReadingPosition {
    uint32_t spine = 0;
    uint32_t block = 0;
    uint32_t offset = 0;

    bool operator==(const ReadingPosition& other) const = default;
};

class EpubSession {
public:
    // Opens `path` and lays out the chapter containing `start` (clamped into range), landing
    // on the page that contains it. The measurer must outlive the session.
    epub::OpenError Open(const std::string& path, const book_layout::TextMeasurer* measurer,
                         const book_layout::LayoutOptions& options, ReadingPosition start = {});
    // Same, from an already-open source (tests, fuzzers).
    epub::OpenError Open(std::unique_ptr<epub::ByteSource> source,
                         const book_layout::TextMeasurer* measurer,
                         const book_layout::LayoutOptions& options, ReadingPosition start = {});

    bool is_open() const { return book_ != nullptr; }

    // Next/previous page, crossing into the next/previous readable chapter (skipping
    // linear="no" items and chapters with nothing on them). False at the ends of the book.
    bool NextPage();
    bool PreviousPage();

    // Jumps to a table-of-contents entry. False for an entry outside the reading order.
    bool GoToTocEntry(size_t toc_index);

    const book_layout::Page& page() const;
    ReadingPosition position() const;  // start of the current page: what to save
    size_t spine_index() const { return spine_index_; }
    size_t page_index() const { return page_index_; }
    size_t page_count() const { return pages_.size(); }
    // The TOC title for the current chapter (the last TOC entry at or before it), or "".
    std::string chapter_title() const;
    // Rough whole-book progress, 0..100, from the spine position and page within the chapter.
    int percent() const;
    // True when the current chapter couldn't be read and is shown as a short notice instead.
    bool chapter_failed() const { return chapter_failed_; }

    const epub::BookMetadata& metadata() const;
    const std::vector<epub::TocEntry>& toc() const;

private:
    bool LoadChapter(size_t spine_index);
    // The next readable spine item in `direction` (+1/-1) from `from`, or false at the end.
    bool FindReadable(size_t from, int direction, size_t* out);

    std::unique_ptr<epub::EpubBook> book_;
    const book_layout::TextMeasurer* measurer_ = nullptr;
    book_layout::LayoutOptions options_;
    std::vector<book_layout::Page> pages_;
    size_t spine_index_ = 0;
    size_t page_index_ = 0;
    bool chapter_failed_ = false;
};

}  // namespace book_reader

#endif  // BOOK_READER_EPUB_SESSION_H_

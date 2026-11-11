#include "book_reader_page_coordinator.h"

#include <algorithm>

#include "book_formats/book_formats.h"
#include "text_reader_service.h"

namespace {

// Bounded per page: at most this many SD reads are made trying to fill one page, so a
// pathological file (e.g. no whitespace at all for many kilobytes) can't spin reading forever --
// the page is simply cut short once the budget is spent.
constexpr size_t kReadChunkBytes = 4096;
constexpr int kMaxChunkReadsPerPage = 4;
// Go to 100% in a .txt lands about this far before the end, roughly the last page.
constexpr uint64_t kEndBackoffBytes = 1500;

}  // namespace

BookReaderPageCoordinator::BookReaderPageCoordinator() = default;

void BookReaderPageCoordinator::BuildPageAt(size_t offset, int portrait_width, int portrait_height)
{
    std::string buffer;
    size_t read_offset = offset;
    epaper_ui::BookReaderPageLayout layout = {};

    // Heap-allocated rather than a stack buffer: this runs on whatever task dispatches button
    // input, and that task's stack budget isn't sized with a 4 KB local in mind.
    std::vector<char> chunk(kReadChunkBytes);
    for (int attempt = 0; attempt < kMaxChunkReadsPerPage; ++attempt) {
        const size_t bytes_read =
            text_reader_service::ReadChunk(filename_, read_offset, chunk.data(), chunk.size());
        if (bytes_read == 0) {
            break;  // end of file
        }
        buffer.append(chunk.data(), bytes_read);
        read_offset += bytes_read;

        layout = epaper_ui::PaginateBookText(portrait_width, portrait_height, buffer);
        const bool hit_end_of_file = bytes_read < chunk.size();
        if (!layout.text_exhausted || hit_end_of_file) {
            break;  // page filled, or there's nothing more to read regardless
        }
        // Page not yet full and more of the file might follow -- fetch another chunk.
    }

    page_lines_ = layout.lines;
    next_page_start_offset_ = offset + layout.consumed_bytes;
    has_more_after_ = next_page_start_offset_ < text_reader_service::GetFileSize(filename_);
}

void BookReaderPageCoordinator::Show(const std::string& filename, int portrait_width,
                                     int portrait_height)
{
    filename_ = filename;
    back_stack_.clear();
    epub_ = text_reader_service::IsEpub(filename);
    if (epub_) {
        ShowEpub(portrait_width, portrait_height);
        return;
    }
    page_start_offset_ = text_reader_service::LoadPosition(filename);

    // Rebuild the back-stack for pages already read (0 .. saved position) by re-deriving each
    // one the same deterministic way a fresh read would, so paging backward works immediately
    // after reopening a book -- not just for pages turned this session. No persisted index:
    // pagination only depends on the fixed font/screen geometry, so replaying it here always
    // reproduces the same page-start offsets a prior session landed on. Cost is one-time on
    // open and scales with how far into the book the reader has actually gotten, not with the
    // book's total size.
    page_number_ = 1;
    size_t offset = 0;
    while (offset < page_start_offset_) {
        back_stack_.push_back(offset);
        BuildPageAt(offset, portrait_width, portrait_height);
        if (next_page_start_offset_ <= offset) {
            break;  // no forward progress -- stop rather than loop forever
        }
        offset = next_page_start_offset_;
        page_number_ += 1;
    }

    BuildPageAt(page_start_offset_, portrait_width, portrait_height);
}

namespace {

book_layout::LayoutOptions EpubLayoutOptions(int portrait_width, int portrait_height)
{
    const epaper_ui::UiRect content =
        epaper_ui::BookReaderContentBounds(portrait_width, portrait_height);
    book_layout::LayoutOptions options;
    options.width = content.width;
    options.height = content.height;
    return options;
}

}  // namespace

void BookReaderPageCoordinator::Relayout(int portrait_width, int portrait_height)
{
    if (filename_.empty()) {
        return;
    }
    if (epub_ && !epub_failed_ && epub_session_.is_open()) {
        epub_session_.Relayout(EpubLayoutOptions(portrait_width, portrait_height));
        page_number_ = static_cast<int>(epub_session_.page_index()) + 1;
        return;
    }
    // .txt (and an EPUB showing its "can't open" message): reopen from the saved place. The
    // page starts exactly there; the pages before it are re-derived at the new size.
    SavePosition();
    Show(filename_, portrait_width, portrait_height);
}

bool BookReaderPageCoordinator::GoToPercent(int percent, int portrait_width, int portrait_height)
{
    if (filename_.empty()) {
        return false;
    }
    percent = std::clamp(percent, 0, 100);
    if (epub_) {
        if (epub_failed_ || !epub_session_.GoToPercent(percent)) {
            return false;
        }
        page_number_ = static_cast<int>(epub_session_.page_index()) + 1;
        return true;
    }
    const uint64_t size = text_reader_service::GetFileSize(filename_);
    uint64_t target = size * static_cast<uint64_t>(percent) / 100u;
    if (target + kEndBackoffBytes > size) {
        // 100% (or close): back off about a page so there's something left to read.
        target = size > kEndBackoffBytes ? size - kEndBackoffBytes : 0;
    }
    size_t offset = static_cast<size_t>(target);
    if (offset > 0) {
        // Start on a word: skip to just past the next space or line break.
        char chunk[256];
        const size_t got = text_reader_service::ReadChunk(filename_, offset, chunk, sizeof(chunk));
        for (size_t i = 0; i < got; ++i) {
            if (chunk[i] == ' ' || chunk[i] == '\n' || chunk[i] == '\t') {
                offset += i + 1;
                break;
            }
        }
    }
    text_reader_service::SavePosition(filename_, offset);
    Show(filename_, portrait_width, portrait_height);
    return true;
}

library::Bookmark BookReaderPageCoordinator::CurrentBookmark() const
{
    library::Bookmark bookmark;
    bookmark.percent = std::max(0, ProgressPercent());
    if (epub_ && !epub_failed_) {
        const book_reader::ReadingPosition at = epub_session_.position();
        bookmark.spine = at.spine;
        bookmark.block = at.block;
        bookmark.offset = at.offset;
        bookmark.label = epaper_ui::ToDisplayText(epub_session_.chapter_title());
        return bookmark;
    }
    bookmark.offset = page_start_offset_;
    if (!page_lines_.empty()) {
        bookmark.label = page_lines_.front();
        constexpr size_t kMaxLabel = 40;
        if (bookmark.label.size() > kMaxLabel) {
            bookmark.label = bookmark.label.substr(0, kMaxLabel) + "...";
        }
    }
    return bookmark;
}

int BookReaderPageCoordinator::BookmarkOnPage(
    const std::vector<library::Bookmark>& bookmarks) const
{
    for (size_t i = 0; i < bookmarks.size(); ++i) {
        const library::Bookmark& b = bookmarks[i];
        bool here = false;
        if (epub_) {
            here = !epub_failed_ &&
                   epub_session_.IsOnCurrentPage({b.spine, b.block, static_cast<uint32_t>(b.offset)});
        } else {
            const uint64_t end = has_more_after_ ? next_page_start_offset_
                                                 : text_reader_service::GetFileSize(filename_) + 1;
            here = b.offset >= page_start_offset_ && b.offset < end;
        }
        if (here) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool BookReaderPageCoordinator::GoToBookmark(const library::Bookmark& bookmark,
                                             int portrait_width, int portrait_height)
{
    if (filename_.empty()) {
        return false;
    }
    if (epub_) {
        if (epub_failed_ ||
            !epub_session_.GoTo({bookmark.spine, bookmark.block,
                                 static_cast<uint32_t>(bookmark.offset)})) {
            return false;
        }
        page_number_ = static_cast<int>(epub_session_.page_index()) + 1;
        return true;
    }
    text_reader_service::SavePosition(filename_, static_cast<size_t>(bookmark.offset));
    Show(filename_, portrait_width, portrait_height);
    return true;
}

void BookReaderPageCoordinator::ShowEpub(int portrait_width, int portrait_height)
{
    const book_layout::LayoutOptions options = EpubLayoutOptions(portrait_width, portrait_height);

    text_reader_service::EpubPosition saved = {};
    book_reader::ReadingPosition start = {};
    if (text_reader_service::LoadEpubPosition(filename_, &saved)) {
        start = {saved.spine, saved.block, saved.offset};
    }
    page_number_ = 1;
    const epub::OpenError error =
        epub_session_.Open(text_reader_service::FullPath(filename_), &measurer_, options, start);
    epub_failed_ = error != epub::OpenError::kNone;
    page_lines_.clear();
    if (epub_failed_) {
        const book_formats::Message message =
            book_formats::MessageFor(book_formats::ProblemForOpenError(error));
        page_lines_ = epaper_ui::PaginateBookText(
                          portrait_width, portrait_height,
                          std::string(message.title) + "\n\n" + message.body)
                          .lines;
    }
}

void BookReaderPageCoordinator::SavePosition() const
{
    if (!epub_) {
        text_reader_service::SavePosition(filename_, page_start_offset_);
        return;
    }
    if (epub_failed_ || !epub_session_.is_open()) {
        return;
    }
    const book_reader::ReadingPosition position = epub_session_.position();
    text_reader_service::SaveEpubPosition(filename_,
                                          {position.spine, position.block, position.offset});
}

int BookReaderPageCoordinator::ProgressPercent() const
{
    if (epub_) {
        return epub_failed_ ? -1 : epub_session_.percent();
    }
    const uint64_t size = text_reader_service::GetFileSize(filename_);
    if (size == 0) {
        return 0;
    }
    const uint64_t end = has_more_after_ ? next_page_start_offset_ : size;
    return static_cast<int>(std::min<uint64_t>(100, end * 100 / size));
}

std::string BookReaderPageCoordinator::MenuTitle() const
{
    if (epub_ && !epub_failed_ && !epub_session_.metadata().title.empty()) {
        return epaper_ui::ToDisplayText(epub_session_.metadata().title);
    }
    return filename_;
}

std::vector<std::string> BookReaderPageCoordinator::ContentsLabels() const
{
    std::vector<std::string> labels;
    if (!epub_ || epub_failed_) {
        return labels;
    }
    for (const epub::TocEntry& entry : epub_session_.toc()) {
        if (labels.size() >= kMaxContentsEntries) {
            break;
        }
        labels.push_back(std::string(static_cast<size_t>(std::min(entry.depth, 3)) * 2, ' ') +
                         epaper_ui::ToDisplayText(entry.title));
    }
    return labels;
}

int BookReaderPageCoordinator::CurrentContentsIndex() const
{
    if (!epub_ || epub_failed_) {
        return 0;
    }
    const auto& toc = epub_session_.toc();
    int best = 0;
    for (size_t i = 0; i < toc.size() && i < kMaxContentsEntries; ++i) {
        if (toc[i].spine_index >= 0 &&
            toc[i].spine_index <= static_cast<int>(epub_session_.spine_index())) {
            best = static_cast<int>(i);
        }
    }
    return best;
}

bool BookReaderPageCoordinator::GoToContentsEntry(size_t index)
{
    if (!epub_ || epub_failed_ || index >= kMaxContentsEntries) {
        return false;
    }
    if (!epub_session_.GoToTocEntry(index)) {
        return false;
    }
    page_number_ = static_cast<int>(epub_session_.page_index()) + 1;
    return true;
}

bool BookReaderPageCoordinator::TurnPage(int delta, int portrait_width, int portrait_height)
{
    if (epub_) {
        if (epub_failed_ || delta == 0) {
            return false;
        }
        const bool turned = delta > 0 ? epub_session_.NextPage() : epub_session_.PreviousPage();
        if (turned) {
            page_number_ = std::max(1, page_number_ + (delta > 0 ? 1 : -1));
        }
        return turned;
    }
    if (delta > 0) {
        if (!has_more_after_) {
            return false;
        }
        back_stack_.push_back(page_start_offset_);
        page_start_offset_ = next_page_start_offset_;
        BuildPageAt(page_start_offset_, portrait_width, portrait_height);
        page_number_ += 1;
        return true;
    }
    if (delta < 0) {
        if (back_stack_.empty()) {
            return false;
        }
        page_start_offset_ = back_stack_.back();
        back_stack_.pop_back();
        BuildPageAt(page_start_offset_, portrait_width, portrait_height);
        page_number_ = std::max(1, page_number_ - 1);
        return true;
    }
    return false;
}

epaper_ui::BookReaderPageState BookReaderPageCoordinator::BuildState() const
{
    epaper_ui::BookReaderPageState state = {};
    state.title_text = filename_;
    state.page_lines = page_lines_;
    state.page_number = page_number_;
    if (epub_ && !epub_failed_) {
        if (!epub_session_.metadata().title.empty()) {
            state.title_text = epaper_ui::ToDisplayText(epub_session_.metadata().title);
        }
        state.use_layout_page = true;
        state.layout_page = epub_session_.page();
        std::string label;
        const std::string chapter = epaper_ui::ToDisplayText(epub_session_.chapter_title());
        if (!chapter.empty()) {
            label = chapter + " - ";
        }
        label += std::to_string(epub_session_.page_index() + 1) + "/" +
                 std::to_string(epub_session_.page_count()) + " - " +
                 std::to_string(epub_session_.percent()) + "%";
        state.page_label = std::move(label);
    }
    return state;
}

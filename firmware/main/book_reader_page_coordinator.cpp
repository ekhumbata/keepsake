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

void BookReaderPageCoordinator::ShowEpub(int portrait_width, int portrait_height)
{
    const epaper_ui::UiRect content =
        epaper_ui::BookReaderContentBounds(portrait_width, portrait_height);
    book_layout::LayoutOptions options;
    options.width = content.width;
    options.height = content.height;

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

#include "book_reader/epub_session.h"

#include <algorithm>
#include <cmath>

namespace book_reader {
namespace {

const book_layout::Page& EmptyPage()
{
    static const book_layout::Page kEmpty;
    return kEmpty;
}

bool IsEmpty(const std::vector<book_layout::Page>& pages)
{
    return pages.empty() || (pages.size() == 1 && pages[0].lines.empty());
}

}  // namespace

epub::OpenError EpubSession::Open(const std::string& path, const book_layout::TextMeasurer* measurer,
                                  const book_layout::LayoutOptions& options, ReadingPosition start)
{
    auto source = std::make_unique<epub::FileByteSource>(path);
    if (!source->is_open()) {
        book_.reset();
        return epub::OpenError::kIoError;
    }
    return Open(std::move(source), measurer, options, start);
}

epub::OpenError EpubSession::Open(std::unique_ptr<epub::ByteSource> source,
                                  const book_layout::TextMeasurer* measurer,
                                  const book_layout::LayoutOptions& options, ReadingPosition start)
{
    book_.reset();
    pages_.clear();
    measurer_ = measurer;
    options_ = options;
    if (measurer == nullptr) {
        return epub::OpenError::kIoError;
    }
    epub::OpenError error = epub::OpenError::kNone;
    book_ = epub::EpubBook::Open(std::move(source), &error);
    if (book_ == nullptr) {
        return error;
    }
    start_bytes_.clear();
    total_bytes_ = 0;
    for (const epub::SpineItem& item : book_->spine()) {
        start_bytes_.push_back(total_bytes_);
        if (item.linear) {
            total_bytes_ += std::max<uint32_t>(1, item.bytes);
        }
    }

    const size_t last = book_->spine().size() - 1;
    size_t spine = std::min<size_t>(start.spine, last);
    // A saved position on a non-linear or empty item moves forward to readable text (or
    // backward from the end), so reopening never lands on a blank page.
    if (!LoadChapter(spine) || (IsEmpty(pages_) && !chapter_failed_)) {
        size_t readable = spine;
        if (FindReadable(spine, +1, &readable) || FindReadable(spine, -1, &readable)) {
            LoadChapter(readable);
            start = ReadingPosition{static_cast<uint32_t>(readable), 0, 0};
        }
    }
    if (start.spine == spine_index_) {
        page_index_ = book_layout::PageForPosition(pages_, {start.block, start.offset});
    }
    return epub::OpenError::kNone;
}

bool EpubSession::GoTo(ReadingPosition place)
{
    if (!is_open() || place.spine >= book_->spine().size()) {
        return false;
    }
    LoadChapter(place.spine);
    if (!pages_.empty()) {
        page_index_ = book_layout::PageForPosition(pages_, {place.block, place.offset});
    }
    return true;
}

bool EpubSession::IsOnCurrentPage(ReadingPosition place) const
{
    if (!is_open() || pages_.empty() || place.spine != spine_index_) {
        return false;
    }
    return book_layout::PageForPosition(pages_, {place.block, place.offset}) == page_index_;
}

bool EpubSession::Relayout(const book_layout::LayoutOptions& options)
{
    if (!book_) {
        return false;
    }
    const ReadingPosition at = position();
    options_ = options;
    LoadChapter(at.spine);
    if (!pages_.empty()) {
        page_index_ = book_layout::PageForPosition(pages_, {at.block, at.offset});
    }
    return true;
}

bool EpubSession::LoadChapter(size_t spine_index)
{
    spine_index_ = spine_index;
    page_index_ = 0;
    chapter_failed_ = false;
    epub::Chapter chapter;
    if (!book_->LoadChapter(spine_index, &chapter)) {
        // Shown in place of the chapter, so the rest of the book stays readable.
        chapter_failed_ = true;
        chapter = epub::Chapter{};
        epub::Block notice;
        notice.runs.push_back(
            epub::TextRun{"This part of the book couldn't be read. Turn the page to keep going.",
                          epub::kStyleItalic});
        chapter.blocks.push_back(std::move(notice));
    }
    pages_ = book_layout::PaginateChapter(chapter, *measurer_, options_);
    return !chapter_failed_;
}

bool EpubSession::FindReadable(size_t from, int direction, size_t* out)
{
    const auto& spine = book_->spine();
    size_t index = from;
    while (true) {
        if (direction > 0) {
            if (index + 1 >= spine.size()) {
                return false;
            }
            ++index;
        } else {
            if (index == 0) {
                return false;
            }
            --index;
        }
        if (!spine[index].linear) {
            continue;
        }
        // Peek: a chapter that lays out to nothing (an empty file) is skipped too.
        epub::Chapter chapter;
        if (book_->LoadChapter(index, &chapter) && chapter.blocks.empty()) {
            continue;
        }
        *out = index;
        return true;
    }
}

bool EpubSession::NextPage()
{
    if (!is_open()) {
        return false;
    }
    if (page_index_ + 1 < pages_.size()) {
        ++page_index_;
        return true;
    }
    size_t next = 0;
    if (!FindReadable(spine_index_, +1, &next)) {
        return false;
    }
    LoadChapter(next);
    return true;
}

bool EpubSession::PreviousPage()
{
    if (!is_open()) {
        return false;
    }
    if (page_index_ > 0) {
        --page_index_;
        return true;
    }
    size_t previous = 0;
    if (!FindReadable(spine_index_, -1, &previous)) {
        return false;
    }
    LoadChapter(previous);
    page_index_ = pages_.empty() ? 0 : pages_.size() - 1;
    return true;
}

bool EpubSession::GoToTocEntry(size_t toc_index)
{
    if (!is_open() || toc_index >= book_->toc().size()) {
        return false;
    }
    const epub::TocEntry& entry = book_->toc()[toc_index];
    if (entry.spine_index < 0) {
        return false;
    }
    epub::Chapter chapter;
    const size_t spine = static_cast<size_t>(entry.spine_index);
    LoadChapter(spine);
    if (!entry.fragment.empty() && !chapter_failed_ && book_->LoadChapter(spine, &chapter)) {
        const size_t block = chapter.AnchorBlock(entry.fragment);
        page_index_ = book_layout::PageForPosition(pages_, {block, 0});
    }
    return true;
}

const book_layout::Page& EpubSession::page() const
{
    return page_index_ < pages_.size() ? pages_[page_index_] : EmptyPage();
}

ReadingPosition EpubSession::position() const
{
    const book_layout::Position start = book_layout::PageStart(page());
    return ReadingPosition{static_cast<uint32_t>(spine_index_), static_cast<uint32_t>(start.block),
                           static_cast<uint32_t>(start.offset)};
}

std::string EpubSession::chapter_title() const
{
    if (!is_open()) {
        return {};
    }
    // The latest spine item at or before this one that the TOC points at; its first TOC
    // entry (usually the chapter heading rather than a sub-section) names the chapter.
    int best_spine = -1;
    for (const epub::TocEntry& entry : book_->toc()) {
        if (entry.spine_index <= static_cast<int>(spine_index_) && entry.spine_index > best_spine) {
            best_spine = entry.spine_index;
        }
    }
    for (const epub::TocEntry& entry : book_->toc()) {
        if (best_spine >= 0 && entry.spine_index == best_spine) {
            return entry.title;
        }
    }
    return {};
}

double EpubSession::ShareBefore(size_t spine_index) const
{
    if (total_bytes_ == 0 || spine_index >= start_bytes_.size()) {
        return static_cast<double>(spine_index) / static_cast<double>(book_->spine().size());
    }
    return static_cast<double>(start_bytes_[spine_index]) / static_cast<double>(total_bytes_);
}

double EpubSession::Share(size_t spine_index) const
{
    if (total_bytes_ == 0 || spine_index >= start_bytes_.size()) {
        return 1.0 / static_cast<double>(book_->spine().size());
    }
    const uint64_t end =
        spine_index + 1 < start_bytes_.size() ? start_bytes_[spine_index + 1] : total_bytes_;
    return static_cast<double>(end - start_bytes_[spine_index]) /
           static_cast<double>(total_bytes_);
}

int EpubSession::percent() const
{
    if (!is_open() || book_->spine().empty()) {
        return 0;
    }
    const double within = pages_.empty() ? 0.0
                                         : static_cast<double>(page_index_ + 1) /
                                               static_cast<double>(pages_.size());
    const double done = ShareBefore(spine_index_) + within * Share(spine_index_);
    return std::clamp(static_cast<int>(done * 100.0 + 0.5), 0, 100);
}

bool EpubSession::GoToPercent(int percent)
{
    if (!is_open() || book_->spine().empty()) {
        return false;
    }
    const double target = std::clamp(percent, 0, 100) / 100.0;
    const size_t count = book_->spine().size();
    // The last chapter that starts at or before the target (and has a share of the book).
    size_t spine = 0;
    for (size_t i = 0; i < count; ++i) {
        if (Share(i) > 0.0 && ShareBefore(i) <= target) {
            spine = i;
        }
    }
    const double share = Share(spine);
    const double within =
        share > 0.0 ? std::clamp((target - ShareBefore(spine)) / share, 0.0, 1.0) : 0.0;

    if (!LoadChapter(spine) || (IsEmpty(pages_) && !chapter_failed_)) {
        size_t readable = spine;
        if (!FindReadable(spine, +1, &readable) && !FindReadable(spine, -1, &readable)) {
            return true;  // nothing readable anywhere: stay on what's loaded
        }
        LoadChapter(readable);
        return true;  // landed on a neighbour: its first page
    }
    if (!pages_.empty()) {
        // percent() counts page i as (i + 1) / n of the chapter, so pick the page whose count
        // first reaches the target.
        const double pages = static_cast<double>(pages_.size());
        const double index = std::ceil(within * pages) - 1.0;
        page_index_ = static_cast<size_t>(std::clamp(index, 0.0, pages - 1.0));
    }
    return true;
}

const epub::BookMetadata& EpubSession::metadata() const
{
    static const epub::BookMetadata kNone;
    return book_ != nullptr ? book_->metadata() : kNone;
}

const std::vector<epub::TocEntry>& EpubSession::toc() const
{
    static const std::vector<epub::TocEntry> kNone;
    return book_ != nullptr ? book_->toc() : kNone;
}

}  // namespace book_reader

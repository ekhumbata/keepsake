// book_reader::EpubSession against the generated test shelf, with a fixed-width measurer.
#include <doctest/doctest.h>

#include <string>

#include "book_reader/epub_session.h"

using book_reader::EpubSession;
using book_reader::ReadingPosition;

namespace {

class FixedMeasurer final : public book_layout::TextMeasurer {
public:
    int Width(std::string_view text, uint8_t, book_layout::TextRole) const override
    {
        return static_cast<int>(text.size()) * 10;
    }
    int LineHeight(book_layout::TextRole) const override { return 20; }
};

const FixedMeasurer kMeasurer;

book_layout::LayoutOptions TinyPage()
{
    book_layout::LayoutOptions options;
    options.width = 200;
    options.height = 100;  // 5 lines: chapters span several pages
    options.paragraph_gap = 0;
    options.heading_gap = 0;
    return options;
}

std::string Fixture(const char* name)
{
    return std::string(KEEPSAKE_FIXTURE_DIR) + "/" + name;
}

std::string FirstLine(const EpubSession& session)
{
    std::string text;
    if (!session.page().lines.empty()) {
        for (const auto& span : session.page().lines[0].spans) {
            text += span.text;
        }
    }
    return text;
}

std::string PageText(const EpubSession& session)
{
    std::string text;
    for (const auto& line : session.page().lines) {
        for (const auto& span : line.spans) {
            text += span.text;
        }
        text += ' ';
    }
    return text;
}

}  // namespace

TEST_CASE("paging runs through the whole book and back")
{
    EpubSession session;
    REQUIRE(session.Open(Fixture("epub2_ncx.epub"), &kMeasurer, TinyPage()) == epub::OpenError::kNone);
    CHECK(session.spine_index() == 0);
    CHECK(FirstLine(session) == "Chapter One");
    CHECK(session.chapter_title() == "Chapter One");

    int forward = 0;
    int percent = session.percent();
    while (session.NextPage()) {
        ++forward;
        CHECK(session.percent() >= percent);
        percent = session.percent();
        REQUIRE(forward < 200);
    }
    CHECK(session.spine_index() == 1);
    CHECK(session.chapter_title() == "Chapter Two");
    CHECK(session.percent() == 100);
    CHECK(forward > 3);

    int back = 0;
    while (session.PreviousPage()) {
        ++back;
        REQUIRE(back < 200);
    }
    CHECK(back == forward);
    CHECK(session.spine_index() == 0);
    CHECK(session.page_index() == 0);
}

TEST_CASE("a saved position reopens on the same page")
{
    EpubSession session;
    REQUIRE(session.Open(Fixture("epub2_ncx.epub"), &kMeasurer, TinyPage()) == epub::OpenError::kNone);
    for (int i = 0; i < 4; ++i) {
        REQUIRE(session.NextPage());
    }
    const ReadingPosition saved = session.position();
    const std::string line = FirstLine(session);

    EpubSession reopened;
    REQUIRE(reopened.Open(Fixture("epub2_ncx.epub"), &kMeasurer, TinyPage(), saved) ==
            epub::OpenError::kNone);
    CHECK(reopened.position() == saved);
    CHECK(FirstLine(reopened) == line);

    // An out-of-range saved spine clamps instead of failing.
    EpubSession clamped;
    REQUIRE(clamped.Open(Fixture("epub2_ncx.epub"), &kMeasurer, TinyPage(), {99, 0, 0}) ==
            epub::OpenError::kNone);
    CHECK(clamped.spine_index() == 1);
}

TEST_CASE("non-linear items are skipped and TOC jumps land on their anchor")
{
    EpubSession session;
    REQUIRE(session.Open(Fixture("epub3_nav.epub"), &kMeasurer, TinyPage()) == epub::OpenError::kNone);
    while (session.spine_index() == 0) {
        REQUIRE(session.NextPage());
    }
    CHECK(session.spine_index() == 2);  // spine 1 is the linear="no" notes page

    REQUIRE(session.GoToTocEntry(1));  // "A Second Part" -> chapter 1.xhtml#part2
    CHECK(session.spine_index() == 0);
    CHECK(session.page_index() > 0);
    CHECK(FirstLine(session).find("Second") != std::string::npos);

    CHECK_FALSE(session.GoToTocEntry(99));
}

TEST_CASE("a damaged chapter becomes a notice page and the book keeps going")
{
    EpubSession session;
    REQUIRE(session.Open(Fixture("bad_crc.epub"), &kMeasurer, TinyPage()) == epub::OpenError::kNone);
    while (session.spine_index() == 0) {
        REQUIRE(session.NextPage());
    }
    CHECK(session.spine_index() == 1);
    CHECK(session.chapter_failed());
    CHECK(PageText(session).find("couldn't") != std::string::npos);
}

TEST_CASE("books that can't open report why")
{
    EpubSession session;
    CHECK(session.Open(Fixture("drm_adobe.epub"), &kMeasurer, TinyPage()) == epub::OpenError::kDrm);
    CHECK_FALSE(session.is_open());
    CHECK_FALSE(session.NextPage());
    CHECK(session.page().lines.empty());
    CHECK(session.Open(Fixture("missing.epub"), &kMeasurer, TinyPage()) == epub::OpenError::kIoError);
}

namespace {

class ScalableMeasurer final : public book_layout::TextMeasurer {
public:
    int Width(std::string_view text, uint8_t, book_layout::TextRole) const override
    {
        return static_cast<int>(text.size()) * char_width;
    }
    int LineHeight(book_layout::TextRole) const override { return char_width * 2; }
    int char_width = 10;
};

bool Before(const ReadingPosition& a, const ReadingPosition& b)
{
    return a.block != b.block ? a.block < b.block : a.offset < b.offset;
}

}  // namespace

TEST_CASE("changing the text size keeps the reader on the same passage")
{
    ScalableMeasurer measurer;
    EpubSession session;
    REQUIRE(session.Open(Fixture("epub2_ncx.epub"), &measurer, TinyPage()) == epub::OpenError::kNone);
    REQUIRE(session.NextPage());
    REQUIRE(session.NextPage());
    const ReadingPosition was = session.position();
    const size_t medium_pages = session.page_count();

    // Bigger text: more pages, and the page shown starts at or before where we were...
    measurer.char_width = 13;
    REQUIRE(session.Relayout(TinyPage()));
    CHECK(session.page_count() > medium_pages);
    CHECK(session.spine_index() == was.spine);
    CHECK_FALSE(Before(was, session.position()));
    // ...and the next page starts after it, so the passage is on screen.
    if (session.page_index() + 1 < session.page_count()) {
        REQUIRE(session.NextPage());
        CHECK(Before(was, session.position()));
    }

    // Back to the old size: same pages as before.
    measurer.char_width = 10;
    REQUIRE(session.Relayout(TinyPage()));
    CHECK(session.page_count() == medium_pages);
}

TEST_CASE("relayout before opening does nothing")
{
    EpubSession session;
    CHECK_FALSE(session.Relayout(TinyPage()));
}

TEST_CASE("go to percent lands where the progress says it is")
{
    EpubSession session;
    REQUIRE(session.Open(Fixture("epub2_ncx.epub"), &kMeasurer, TinyPage()) == epub::OpenError::kNone);

    REQUIRE(session.GoToPercent(0));
    CHECK(session.spine_index() == 0);
    CHECK(session.page_index() == 0);

    REQUIRE(session.GoToPercent(100));
    CHECK(session.percent() == 100);
    CHECK_FALSE(session.NextPage());  // the very end

    // In between, the jump and the readout agree to within one page's worth.
    int previous = -1;
    for (int target = 5; target <= 95; target += 10) {
        REQUIRE(session.GoToPercent(target));
        const int landed = session.percent();
        CHECK(landed >= target);
        CHECK(landed >= previous);
        previous = landed;
        // One page back is before the target (unless this is the first page of the book).
        if (session.PreviousPage()) {
            CHECK(session.percent() <= target);
        }
    }
}

TEST_CASE("progress weighs chapters by size and only counts the reading order")
{
    EpubSession session;
    REQUIRE(session.Open(Fixture("epub2_ncx.epub"), &kMeasurer, TinyPage()) == epub::OpenError::kNone);
    int last = -1;
    do {
        CHECK(session.percent() >= last);
        last = session.percent();
    } while (session.NextPage());
    CHECK(last == 100);

    EpubSession closed;
    CHECK_FALSE(closed.GoToPercent(50));
}

TEST_CASE("a bookmarked place can be found again, even after the text size changes")
{
    ScalableMeasurer measurer;
    EpubSession session;
    REQUIRE(session.Open(Fixture("epub2_ncx.epub"), &measurer, TinyPage()) == epub::OpenError::kNone);
    REQUIRE(session.NextPage());
    REQUIRE(session.NextPage());
    const ReadingPosition mark = session.position();
    CHECK(session.IsOnCurrentPage(mark));

    REQUIRE(session.GoToPercent(100));
    CHECK_FALSE(session.IsOnCurrentPage(mark));

    measurer.char_width = 13;  // bigger text, different pages
    REQUIRE(session.Relayout(TinyPage()));
    REQUIRE(session.GoTo(mark));
    CHECK(session.IsOnCurrentPage(mark));
    CHECK(session.spine_index() == mark.spine);

    CHECK_FALSE(session.GoTo(ReadingPosition{9999, 0, 0}));
    EpubSession closed;
    CHECK_FALSE(closed.GoTo(mark));
    CHECK_FALSE(closed.IsOnCurrentPage(mark));
}

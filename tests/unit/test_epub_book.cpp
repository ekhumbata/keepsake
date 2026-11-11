// EpubBook + XHTML parsing against the generated test shelf (tests/fixtures/make_epubs.py).
#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "epub_engine/epub_book.h"
#include "epub_engine/xhtml_parser.h"

using epub::BlockType;
using epub::EpubBook;
using epub::OpenError;

namespace {

std::string Fixture(const char* name)
{
    return std::string(KEEPSAKE_FIXTURE_DIR) + "/" + name;
}

std::unique_ptr<EpubBook> OpenFixture(const char* name, OpenError* error)
{
    auto source = std::make_unique<epub::FileByteSource>(Fixture(name));
    REQUIRE(source->is_open());
    return EpubBook::Open(std::move(source), error);
}

}  // namespace

TEST_CASE("EPUB 2: metadata, spine and NCX table of contents")
{
    for (const char* name : {"epub2_ncx.epub", "epub2_stored.epub", "no_container.epub",
                             "font_obfuscated.epub"}) {
        CAPTURE(name);
        OpenError error = OpenError::kNone;
        auto book = OpenFixture(name, &error);
        REQUIRE(book != nullptr);
        CHECK(error == OpenError::kNone);

        CHECK(book->metadata().title == "A Rainy Day Sampler");
        CHECK(book->metadata().author == "Test Author");  // the first creator
        CHECK(book->metadata().language == "en");
        CHECK(book->metadata().cover_path == "OEBPS/images/cover.png");

        REQUIRE(book->spine().size() == 2);
        CHECK(book->spine()[0].path == "OEBPS/text/one.xhtml");
        CHECK(book->spine()[1].path == "OEBPS/text/two.xhtml");

        const auto& toc = book->toc();
        REQUIRE(toc.size() == 3);
        CHECK(toc[0].title == "Chapter One");
        CHECK(toc[0].depth == 0);
        CHECK(toc[0].spine_index == 0);
        CHECK(toc[0].fragment == "start");
        CHECK(toc[1].title == "A Second Part");
        CHECK(toc[1].depth == 1);
        CHECK(toc[1].fragment == "part2");
        CHECK(toc[2].title == "Chapter Two");
        CHECK(toc[2].spine_index == 1);

        std::string cover;
        CHECK(book->ReadFile(book->metadata().cover_path, 1 << 16, &cover));
        CHECK(cover.substr(1, 3) == "PNG");
    }
}

TEST_CASE("EPUB 3: nav document, cover-image, percent-encoded hrefs, non-linear items")
{
    OpenError error = OpenError::kNone;
    auto book = OpenFixture("epub3_nav.epub", &error);
    REQUIRE(book != nullptr);

    CHECK(book->metadata().title == "Sampler & Friends");
    CHECK(book->metadata().author == "Caf\xC3\xA9 Writer");
    CHECK(book->metadata().language == "en-GB");
    CHECK(book->metadata().cover_path == "book/img/cover.png");

    REQUIRE(book->spine().size() == 3);
    CHECK(book->spine()[0].path == "book/chapter 1.xhtml");
    CHECK(book->spine()[0].linear);
    CHECK_FALSE(book->spine()[1].linear);

    // Only the epub:type="toc" nav counts, not the landmarks nav before it.
    const auto& toc = book->toc();
    REQUIRE(toc.size() == 3);
    CHECK(toc[0].title == "Chapter One");
    CHECK(toc[0].spine_index == 0);
    CHECK(toc[1].title == "A Second Part");
    CHECK(toc[1].depth == 1);
    CHECK(toc[1].fragment == "part2");
    CHECK(toc[2].title == "Chapter Two");
    CHECK(toc[2].spine_index == 2);
    CHECK(toc[2].fragment == "end");

    epub::Chapter chapter;
    REQUIRE(book->LoadChapter(0, &chapter));
    CHECK(chapter.blocks.front().type == BlockType::kHeading);
    // The image href resolves relative to "book/".
    bool found_image = false;
    for (const auto& block : chapter.blocks) {
        if (block.type == BlockType::kImage) {
            CHECK(block.image_href == "book/img/lamp.png");
            found_image = true;
        }
    }
    CHECK(found_image);
}

TEST_CASE("chapter one becomes the expected blocks")
{
    OpenError error = OpenError::kNone;
    auto book = OpenFixture("epub2_ncx.epub", &error);
    REQUIRE(book != nullptr);
    epub::Chapter chapter;
    REQUIRE(book->LoadChapter(0, &chapter));
    const auto& b = chapter.blocks;
    // h1, p, p (with <br>s), li, li, blockquote, hr, h2, p, image. The <p> wrapping the
    // image has no text of its own, so it produces no block.
    REQUIRE(b.size() == 10);

    CHECK(b[0].type == BlockType::kHeading);
    CHECK(b[0].level == 1);
    CHECK(epub::BlockText(b[0]) == "Chapter One");

    // Whitespace collapsed; bold and italic runs kept apart.
    CHECK(b[1].type == BlockType::kParagraph);
    CHECK(epub::BlockText(b[1]) ==
          "The kettle began to sing just as the rain arrived, and Mira decided the day was "
          "officially a reading day.");
    REQUIRE(b[1].runs.size() == 5);
    CHECK(b[1].runs[1].text == "Mira");
    CHECK(b[1].runs[1].style == epub::kStyleBold);
    CHECK(b[1].runs[3].text == "officially");
    CHECK(b[1].runs[3].style == epub::kStyleItalic);

    // <br/> becomes a line-break run.
    CHECK(epub::BlockText(b[2]) == "She wrote three things on a card:\ntea\nblanket\nbook");

    CHECK(b[3].type == BlockType::kListItem);
    CHECK(b[3].level == 1);
    CHECK(epub::BlockText(b[3]) == "Find the lamp");
    CHECK(b[4].type == BlockType::kListItem);
    CHECK(b[4].runs.back().style == epub::kStyleItalic);

    CHECK(b[5].type == BlockType::kBlockquote);
    CHECK(epub::BlockText(b[5]) == "A quiet afternoon is a small holiday.");

    CHECK(b[6].type == BlockType::kRule);

    CHECK(b[7].type == BlockType::kHeading);
    CHECK(b[7].level == 2);

    // Entities decoded to UTF-8 (U+2019, U+00A0, U+2014, U+201C/D).
    CHECK(epub::BlockText(b[8]) ==
          "It\xE2\x80\x99s late\xC2\xA0now \xE2\x80\x94 the \xE2\x80\x9Clast\xE2\x80\x9D page & "
          "then sleep.");

    CHECK(b[9].type == BlockType::kImage);
    CHECK(b[9].image_href == "OEBPS/images/lamp.png");
    CHECK(epub::BlockText(b[9]) == "A small lamp");

    // <script> and <head>/<title> text never shows up.
    for (const auto& block : b) {
        CHECK(epub::BlockText(block).find("ignored") == std::string::npos);
        CHECK(epub::BlockText(block) != "One");
    }

    CHECK(chapter.AnchorBlock("start") == 0);
    CHECK(chapter.AnchorBlock("part2") == 7);
    CHECK_FALSE(chapter.truncated);
}

TEST_CASE("<pre> keeps its lines")
{
    OpenError error = OpenError::kNone;
    auto book = OpenFixture("epub2_ncx.epub", &error);
    REQUIRE(book != nullptr);
    epub::Chapter chapter;
    REQUIRE(book->LoadChapter(1, &chapter));
    bool found = false;
    for (const auto& block : chapter.blocks) {
        if (block.type == BlockType::kPreformatted) {
            CHECK(epub::BlockText(block) == "line one\n  line two");
            found = true;
        }
    }
    CHECK(found);
    CHECK(chapter.AnchorBlock("end") == chapter.blocks.size() - 1);
    CHECK_FALSE(book->LoadChapter(2, &chapter));  // out of range
}

TEST_CASE("books that can't be opened say why")
{
    const struct {
        const char* name;
        OpenError expected;
    } cases[] = {
        {"drm_adobe.epub", OpenError::kDrm},
        {"drm_encryption_only.epub", OpenError::kDrm},
        {"truncated.epub", OpenError::kNotEpub},  // the end record is in the missing half
        {"not_a_zip.epub", OpenError::kNotEpub},
        {"no_package.epub", OpenError::kNotEpub},
    };
    for (const auto& c : cases) {
        CAPTURE(c.name);
        OpenError error = OpenError::kNone;
        auto book = OpenFixture(c.name, &error);
        CHECK(book == nullptr);
        CHECK(error == c.expected);
    }
}

TEST_CASE("a damaged chapter fails to load without taking the book down")
{
    OpenError error = OpenError::kNone;
    auto book = OpenFixture("bad_crc.epub", &error);
    REQUIRE(book != nullptr);
    epub::Chapter chapter;
    CHECK(book->LoadChapter(0, &chapter));
    CHECK_FALSE(book->LoadChapter(1, &chapter));  // CRC mismatch
}

TEST_CASE("XHTML edge cases")
{
    SUBCASE("unclosed tags and stray end tags still give text")
    {
        const auto chapter = epub::ParseXhtml("<p>one <b>two<p>three</i></span>four", "a.xhtml");
        REQUIRE(chapter.blocks.size() == 2);
        CHECK(epub::BlockText(chapter.blocks[0]) == "one two");
        CHECK(epub::BlockText(chapter.blocks[1]) == "threefour");
    }
    SUBCASE("nested lists report their depth")
    {
        const auto chapter =
            epub::ParseXhtml("<ul><li>a<ul><li>b</li></ul></li><li>c</li></ul>", "a.xhtml");
        REQUIRE(chapter.blocks.size() == 3);
        CHECK(chapter.blocks[0].level == 1);
        CHECK(chapter.blocks[1].level == 2);
        CHECK(chapter.blocks[2].level == 1);
    }
    SUBCASE("inline CSS italic/bold")
    {
        const auto chapter = epub::ParseXhtml(
            "<p><span style=\"font-style: italic\">x</span><span style='font-weight:700'>y</span></p>",
            "a.xhtml");
        REQUIRE(chapter.blocks.size() == 1);
        REQUIRE(chapter.blocks[0].runs.size() == 2);
        CHECK(chapter.blocks[0].runs[0].style == epub::kStyleItalic);
        CHECK(chapter.blocks[0].runs[1].style == epub::kStyleBold);
    }
    SUBCASE("block limit truncates")
    {
        std::string many;
        for (int i = 0; i < 50; ++i) {
            many += "<p>x</p>";
        }
        epub::XhtmlLimits limits;
        limits.max_blocks = 10;
        const auto chapter = epub::ParseXhtml(many, "a.xhtml", limits);
        CHECK(chapter.blocks.size() == 10);
        CHECK(chapter.truncated);
    }
    SUBCASE("deep nesting doesn't blow up")
    {
        std::string deep;
        for (int i = 0; i < 5000; ++i) {
            deep += "<div><span>";
        }
        deep += "bottom";
        const auto chapter = epub::ParseXhtml(deep, "a.xhtml");
        REQUIRE(chapter.blocks.size() == 1);
        CHECK(epub::BlockText(chapter.blocks[0]) == "bottom");
    }
    SUBCASE("table cells are kept apart")
    {
        const auto chapter =
            epub::ParseXhtml("<table><tr><td>a</td><td>b</td></tr><tr><td>c</td></tr></table>",
                             "a.xhtml");
        REQUIRE(chapter.blocks.size() == 2);
        CHECK(epub::BlockText(chapter.blocks[0]) == "a b");
        CHECK(epub::BlockText(chapter.blocks[1]) == "c");
    }
}

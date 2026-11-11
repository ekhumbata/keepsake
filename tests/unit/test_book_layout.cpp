// book_layout: blocks -> lines -> pages, with a fixed-width fake font so expectations are
// exact. All sample text is original.
#include <doctest/doctest.h>

#include <sstream>
#include <string>
#include <vector>

#include "book_layout/book_layout.h"
#include "epub_engine/xhtml_parser.h"

using book_layout::LayoutOptions;
using book_layout::LineKind;
using book_layout::Page;
using book_layout::Position;

namespace {

// Every byte is 10 px wide (bold 11 px); lines are 20 px (headings 30 px).
class FixedMeasurer final : public book_layout::TextMeasurer {
public:
    int Width(std::string_view text, uint8_t style, book_layout::TextRole) const override
    {
        return static_cast<int>(text.size()) * ((style & epub::kStyleBold) ? 11 : 10);
    }
    int LineHeight(book_layout::TextRole role) const override
    {
        return role == book_layout::TextRole::kBody || role == book_layout::TextRole::kPreformatted
                   ? 20
                   : 30;
    }
};

std::vector<std::string> Words(const std::string& text)
{
    std::vector<std::string> words;
    std::istringstream in(text);
    std::string w;
    while (in >> w) {
        words.push_back(w);
    }
    return words;
}

std::string LineText(const book_layout::Line& line)
{
    std::string text;
    for (const auto& span : line.spans) {
        text += span.text;
    }
    return text;
}

LayoutOptions SmallPage()
{
    LayoutOptions options;
    options.width = 200;   // 20 characters
    options.height = 100;  // five body lines
    options.paragraph_gap = 0;
    options.heading_gap = 0;
    options.list_indent = 20;
    options.quote_indent = 30;
    return options;
}

epub::Chapter LongChapter()
{
    std::string xhtml = "<h1>Getting Cozy</h1>";
    for (int i = 0; i < 30; ++i) {
        xhtml += "<p>Paragraph " + std::to_string(i) +
                 " notes that the lamp was warm, the tea was <b>very</b> hot, and the cat had "
                 "<i>opinions</i> about the blanket.</p>";
    }
    return epub::ParseXhtml(xhtml, "a.xhtml");
}

}  // namespace

TEST_CASE("an empty chapter is one empty page")
{
    const auto pages = book_layout::PaginateChapter({}, FixedMeasurer(), SmallPage());
    REQUIRE(pages.size() == 1);
    CHECK(pages[0].lines.empty());
    CHECK(book_layout::PageForPosition(pages, {5, 5}) == 0);
}

TEST_CASE("wrapping is greedy and respects the width")
{
    const auto chapter = epub::ParseXhtml("<p>one two three four five six seven</p>", "a.xhtml");
    const auto pages = book_layout::PaginateChapter(chapter, FixedMeasurer(), SmallPage());
    REQUIRE(pages.size() == 1);
    const auto& lines = pages[0].lines;
    REQUIRE(lines.size() == 2);
    CHECK(LineText(lines[0]) == "one two three four");  // 18 chars; adding " five" makes 23
    CHECK(LineText(lines[1]) == "five six seven");
    CHECK(lines[0].offset == 0);
    CHECK(lines[1].offset == 19);  // byte offset of "five" in the block text
    CHECK(lines[1].y == 20);
}

TEST_CASE("paging through a long chapter keeps every word, in order, inside the page")
{
    const epub::Chapter chapter = LongChapter();
    const LayoutOptions options = SmallPage();
    const auto pages = book_layout::PaginateChapter(chapter, FixedMeasurer(), options);
    CHECK(pages.size() > 10);

    std::vector<std::string> laid_out;
    Position previous{0, 0};
    bool first = true;
    for (const Page& page : pages) {
        REQUIRE_FALSE(page.lines.empty());
        for (const auto& line : page.lines) {
            CHECK(line.y >= 0);
            CHECK(line.y + line.height <= options.height);
            int right = line.x_indent;
            for (const auto& span : line.spans) {
                CHECK(span.x >= line.x_indent);
                right = span.x + FixedMeasurer().Width(span.text, span.style, line.role);
            }
            CHECK(right <= options.width);
            const Position here{line.block, line.offset};
            if (!first) {
                CHECK(previous < here);  // positions strictly increase
            }
            previous = here;
            first = false;
            for (const auto& w : Words(LineText(line))) {
                laid_out.push_back(w);
            }
        }
    }
    std::vector<std::string> expected;
    for (const auto& block : chapter.blocks) {
        for (const auto& w : Words(epub::BlockText(block))) {
            expected.push_back(w);
        }
    }
    CHECK(laid_out == expected);
}

TEST_CASE("a saved position lands on the same text after the font changes")
{
    const epub::Chapter chapter = LongChapter();
    LayoutOptions small = SmallPage();
    LayoutOptions large = SmallPage();
    large.width = 300;
    large.height = 160;
    const auto pages_small = book_layout::PaginateChapter(chapter, FixedMeasurer(), small);
    const auto pages_large = book_layout::PaginateChapter(chapter, FixedMeasurer(), large);

    for (size_t i = 0; i < pages_small.size(); ++i) {
        CHECK(book_layout::PageForPosition(pages_small, book_layout::PageStart(pages_small[i])) == i);
    }
    // Reading position from page 7 of the small layout: the large layout's page must contain it.
    const Position saved = book_layout::PageStart(pages_small[7]);
    const size_t page = book_layout::PageForPosition(pages_large, saved);
    CHECK_FALSE(saved < book_layout::PageStart(pages_large[page]));
    if (page + 1 < pages_large.size()) {
        CHECK(saved < book_layout::PageStart(pages_large[page + 1]));
    }
}

TEST_CASE("styles become separate spans at the right x")
{
    const auto chapter = epub::ParseXhtml("<p>a <b>bold</b> word</p>", "a.xhtml");
    const auto pages = book_layout::PaginateChapter(chapter, FixedMeasurer(), SmallPage());
    const auto& spans = pages[0].lines[0].spans;
    REQUIRE(spans.size() == 3);
    CHECK(spans[0].text == "a ");
    CHECK(spans[0].x == 0);
    CHECK(spans[1].text == "bold");
    CHECK(spans[1].style == epub::kStyleBold);
    CHECK(spans[1].x == 20);
    CHECK(spans[2].text == " word");
    CHECK(spans[2].x == 20 + 44);
}

TEST_CASE("an overlong word is broken between characters")
{
    const std::string long_word(45, 'x');
    const auto chapter = epub::ParseXhtml("<p>hi " + long_word + " end</p>", "a.xhtml");
    const auto pages = book_layout::PaginateChapter(chapter, FixedMeasurer(), SmallPage());
    const auto& lines = pages[0].lines;
    REQUIRE(lines.size() == 4);
    CHECK(LineText(lines[0]) == "hi");
    CHECK(LineText(lines[1]) == std::string(20, 'x'));
    CHECK(LineText(lines[2]) == std::string(20, 'x'));
    CHECK(LineText(lines[3]) == std::string(5, 'x') + " end");
    CHECK(lines[2].offset == 3 + 20);
}

TEST_CASE("multi-byte characters are never split")
{
    // 25 two-byte characters (U+00E9) in one word: 50 bytes at 10 px per byte.
    std::string word;
    for (int i = 0; i < 25; ++i) {
        word += "\xC3\xA9";
    }
    const auto chapter = epub::ParseXhtml("<p>" + word + "</p>", "a.xhtml");
    const auto pages = book_layout::PaginateChapter(chapter, FixedMeasurer(), SmallPage());
    for (const auto& page : pages) {
        for (const auto& line : page.lines) {
            const std::string text = LineText(line);
            CHECK(text.size() % 2 == 0);
            CHECK(static_cast<unsigned char>(text[0]) == 0xC3);
        }
    }
}

TEST_CASE("lists get bullets and indents; quotes get indents; <br> forces a break")
{
    const auto chapter = epub::ParseXhtml(
        "<ul><li>first item that wraps onto a second line</li></ul>"
        "<blockquote><p>quoted</p></blockquote><p>a<br/>b</p>",
        "a.xhtml");
    const auto pages = book_layout::PaginateChapter(chapter, FixedMeasurer(), SmallPage());
    std::vector<book_layout::Line> lines;
    for (const auto& page : pages) {
        lines.insert(lines.end(), page.lines.begin(), page.lines.end());
    }
    // The item wraps at 18 characters: "first item that" / "wraps onto a" / "second line".
    REQUIRE(lines.size() == 6);
    CHECK(lines[0].kind == LineKind::kBullet);
    CHECK(lines[0].x_indent == 20);
    CHECK(LineText(lines[0]) == "first item that");
    CHECK(lines[1].kind == LineKind::kText);
    CHECK(lines[1].x_indent == 20);
    CHECK(LineText(lines[2]) == "second line");
    CHECK(LineText(lines[3]) == "quoted");
    CHECK(lines[3].x_indent == 30);
    CHECK(LineText(lines[lines.size() - 2]) == "a");
    CHECK(LineText(lines.back()) == "b");
}

TEST_CASE("gaps separate blocks but never start a page; headings use their role")
{
    LayoutOptions options = SmallPage();
    options.paragraph_gap = 10;
    options.heading_gap = 5;
    const auto chapter = epub::ParseXhtml("<h2>Title</h2><p>one</p><p>two</p>", "a.xhtml");
    const auto pages = book_layout::PaginateChapter(chapter, FixedMeasurer(), options);
    const auto& lines = pages[0].lines;
    REQUIRE(lines.size() == 3);
    CHECK(lines[0].y == 0);  // no gap at the top of a page
    CHECK(lines[0].role == book_layout::TextRole::kHeading2);
    CHECK(lines[0].height == 30);
    CHECK(lines[1].y == 30 + 10);
    CHECK(lines[2].y == 30 + 10 + 20 + 10);
}

TEST_CASE("rules, images and <pre> blank lines")
{
    const auto chapter = epub::ParseXhtml(
        "<p>x</p><hr/><img src='i.png' alt='A lamp'/><pre>a\n\nb</pre>", "t/a.xhtml");
    LayoutOptions options = SmallPage();
    options.height = 400;
    const auto pages = book_layout::PaginateChapter(chapter, FixedMeasurer(), options);
    const auto& lines = pages[0].lines;
    REQUIRE(lines.size() == 6);
    CHECK(lines[1].kind == LineKind::kRule);
    CHECK(lines[1].height == options.rule_height);
    CHECK(lines[2].kind == LineKind::kImage);
    CHECK(LineText(lines[2]) == "A lamp");
    CHECK(LineText(lines[3]) == "a");
    CHECK(LineText(lines[4]).empty());
    CHECK(LineText(lines[5]) == "b");
}

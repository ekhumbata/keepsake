// PaginateBookText: the TXT reader's greedy word-wrap. The reader feeds it chunks read off SD
// and uses consumed_bytes as the next page's file offset, so the invariants that matter are:
// pages never lose or reorder words, consumed_bytes is a real offset into the input, and the
// line budget is respected.
#include <doctest/doctest.h>

#include <sstream>
#include <string>
#include <vector>

#include "epaper_ui/book_reader_page.h"
#include "epaper_ui/font_renderer.h"

namespace {

constexpr int kPortraitWidth = 480;
constexpr int kPortraitHeight = 800;

std::vector<std::string> SplitWords(const std::string& text)
{
    std::vector<std::string> words;
    std::istringstream stream(text);
    std::string word;
    while (stream >> word) {
        words.push_back(word);
    }
    return words;
}

// Original filler prose (written for these tests), repeated to make a multi-page "book".
std::string MakeBook(int paragraphs)
{
    std::string book;
    for (int i = 0; i < paragraphs; ++i) {
        book += "Paragraph " + std::to_string(i) +
                " begins here. The little lamp on the windowsill hummed while the kettle\n"
                "warmed, and the cat considered the rain with great seriousness before\n"
                "deciding, as always, that the blanket was the better plan for the evening.\n"
                "\n";
    }
    return book;
}

}  // namespace

TEST_CASE("empty text is exhausted immediately")
{
    const auto layout = epaper_ui::PaginateBookText(kPortraitWidth, kPortraitHeight, "");
    CHECK(layout.lines.empty());
    CHECK(layout.consumed_bytes == 0);
    CHECK(layout.text_exhausted);
}

TEST_CASE("a degenerate page size consumes nothing")
{
    const auto layout = epaper_ui::PaginateBookText(kPortraitWidth, 10, "Some words here.");
    CHECK(layout.lines.empty());
    CHECK(layout.text_exhausted);
}

TEST_CASE("short text fits on one page and is exhausted")
{
    const std::string text = "A short sentence that easily fits.";
    const auto layout = epaper_ui::PaginateBookText(kPortraitWidth, kPortraitHeight, text);
    REQUIRE(layout.lines.size() == 1);
    CHECK(layout.lines[0] == text);
    CHECK(layout.consumed_bytes == text.size());
    CHECK(layout.text_exhausted);
}

TEST_CASE("every line fits the content width unless it is a single overlong word")
{
    const std::string book = MakeBook(40);
    const auto layout = epaper_ui::PaginateBookText(kPortraitWidth, kPortraitHeight, book);
    REQUIRE_FALSE(layout.lines.empty());
    CHECK_FALSE(layout.text_exhausted);
    CHECK(layout.consumed_bytes < book.size());

    // The content area is narrower than the portrait width; nothing may exceed the panel.
    for (const std::string& line : layout.lines) {
        if (SplitWords(line).size() > 1) {
            CHECK(epaper_ui::MeasureText(design::TypographyRole::kBody, line) <=
                  kPortraitWidth);
        }
    }
}

TEST_CASE("paging through a book keeps every word, in order")
{
    const std::string book = MakeBook(25);
    std::vector<std::string> paged_words;
    size_t offset = 0;
    int pages = 0;
    while (offset < book.size()) {
        const auto layout =
            epaper_ui::PaginateBookText(kPortraitWidth, kPortraitHeight, book.substr(offset));
        REQUIRE(layout.consumed_bytes > 0);  // must always make progress
        REQUIRE(layout.consumed_bytes <= book.size() - offset);
        for (const std::string& line : layout.lines) {
            for (const std::string& word : SplitWords(line)) {
                paged_words.push_back(word);
            }
        }
        offset += layout.consumed_bytes;
        ++pages;
        REQUIRE(pages < 1000);
    }
    CHECK(pages > 1);
    CHECK(paged_words == SplitWords(book));
}

TEST_CASE("a blank line becomes a paragraph gap; single newlines reflow")
{
    const std::string text = "first line\nstill first paragraph\n\nsecond paragraph";
    const auto layout = epaper_ui::PaginateBookText(kPortraitWidth, kPortraitHeight, text);
    REQUIRE(layout.lines.size() == 3);
    CHECK(layout.lines[0] == "first line still first paragraph");
    CHECK(layout.lines[1].empty());
    CHECK(layout.lines[2] == "second paragraph");
    CHECK(layout.text_exhausted);
}

TEST_CASE("CRLF line endings behave like LF")
{
    const std::string text = "one\r\ntwo\r\n\r\nthree";
    const auto layout = epaper_ui::PaginateBookText(kPortraitWidth, kPortraitHeight, text);
    REQUIRE(layout.lines.size() == 3);
    CHECK(layout.lines[0] == "one two");
    CHECK(layout.lines[1].empty());
    CHECK(layout.lines[2] == "three");
}

TEST_CASE("a word wider than the line is kept whole on its own line")
{
    const std::string long_word(200, 'w');
    const std::string text = "before " + long_word + " after";
    const auto layout = epaper_ui::PaginateBookText(kPortraitWidth, kPortraitHeight, text);
    REQUIRE(layout.lines.size() == 3);
    CHECK(layout.lines[0] == "before");
    CHECK(layout.lines[1] == long_word);
    CHECK(layout.lines[2] == "after");
}

TEST_CASE("typographic punctuation is transliterated without changing byte offsets")
{
    // U+201C/U+201D curly double quotes, U+2014 em dash, U+2026 ellipsis.
    const std::string text = "\xE2\x80\x9CHello\xE2\x80\x9D \xE2\x80\x94 wait\xE2\x80\xA6";
    const auto layout = epaper_ui::PaginateBookText(kPortraitWidth, kPortraitHeight, text);
    REQUIRE(layout.lines.size() == 1);
    CHECK(layout.consumed_bytes == text.size());
    CHECK(layout.lines[0].find('\xE2') == std::string::npos);
    CHECK(layout.lines[0].find('"') != std::string::npos);
    CHECK(layout.lines[0].find("Hello") != std::string::npos);
    CHECK(layout.lines[0].find("--") != std::string::npos);
    CHECK(layout.lines[0].find("wait...") != std::string::npos);
}

TEST_CASE("a multi-byte character cut by a chunk boundary does not break pagination")
{
    // The coordinator reads fixed-size chunks, so a chunk can end mid-sequence. The cut
    // character isn't consumed: the next read starts at it and gets all of it.
    const std::string text = "ends with a cut \xE2\x80";
    const auto layout = epaper_ui::PaginateBookText(kPortraitWidth, kPortraitHeight, text);
    CHECK(layout.consumed_bytes == text.size() - 2);
    CHECK(layout.text_exhausted);
    REQUIRE(layout.lines.size() == 1);
    CHECK(layout.lines[0] == "ends with a cut");
}

TEST_CASE("curly quotes and runs of spaces come out as single spaces and whole words (#90)")
{
    const std::string text =
        "\xE2\x80\x9C" "Don\xE2\x80\x99t,\xE2\x80\x9D she said.   Tabs\tand\t\tspaces\r\n"
        "wrap \xC2\xA0here.";
    const auto layout = epaper_ui::PaginateBookText(kPortraitWidth, kPortraitHeight, text);
    std::string joined;
    for (const std::string& line : layout.lines) {
        CHECK(line.find("  ") == std::string::npos);
        CHECK((line.empty() || (line.front() != ' ' && line.back() != ' ')));
        joined += (joined.empty() ? "" : " ") + line;
    }
    CHECK(joined == "\"Don't,\" she said. Tabs and spaces wrap here.");
    CHECK(layout.consumed_bytes == text.size());
}

TEST_CASE("a page that ends in whitespace resumes at the next word")
{
    // Fill one short page, then check the next one starts exactly on a word.
    std::string text;
    for (int i = 0; i < 400; ++i) {
        text += "word\r\n";
    }
    const auto first = epaper_ui::PaginateBookText(kPortraitWidth, 300, text);
    REQUIRE_FALSE(first.text_exhausted);
    REQUIRE(first.consumed_bytes < text.size());
    const auto second =
        epaper_ui::PaginateBookText(kPortraitWidth, 300, text.substr(first.consumed_bytes));
    REQUIRE_FALSE(second.lines.empty());
    CHECK(second.lines[0].substr(0, 4) == "word");
}

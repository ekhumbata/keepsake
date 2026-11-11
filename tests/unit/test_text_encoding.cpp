// .txt encodings (#27): detection, decoding, and the reader's pagination on top of them.
#include <doctest/doctest.h>

#include <string>

#include "book_formats/book_formats.h"
#include "epaper_ui/book_reader_page.h"

using book_formats::TextEncoding;

namespace {

std::string Utf16(const std::string& ascii, bool little_endian, bool bom)
{
    std::string out;
    if (bom) {
        out += little_endian ? "\xFF\xFE" : "\xFE\xFF";
    }
    for (const char c : ascii) {
        if (little_endian) {
            out.push_back(c);
            out.push_back('\0');
        } else {
            out.push_back('\0');
            out.push_back(c);
        }
    }
    return out;
}

std::string Joined(const epaper_ui::BookReaderPageLayout& layout)
{
    std::string all;
    for (const std::string& line : layout.lines) {
        all += line + "|";
    }
    return all;
}

}  // namespace

TEST_CASE("text encodings are recognised")
{
    CHECK(book_formats::DetectTextEncoding("Plain old ASCII.") == TextEncoding::kUtf8);
    CHECK(book_formats::DetectTextEncoding("caf\xC3\xA9 au lait") == TextEncoding::kUtf8);
    CHECK(book_formats::DetectTextEncoding("\xEF\xBB\xBFwith a BOM") == TextEncoding::kUtf8);
    CHECK(book_formats::DetectTextEncoding("ends mid-character \xE2\x80") == TextEncoding::kUtf8);
    CHECK(book_formats::DetectTextEncoding("caf\xE9 au lait") == TextEncoding::kLatin1);
    CHECK(book_formats::DetectTextEncoding("\x93quoted\x94") == TextEncoding::kLatin1);
    CHECK(book_formats::DetectTextEncoding(Utf16("Hello there", true, true)) ==
          TextEncoding::kUtf16Le);
    CHECK(book_formats::DetectTextEncoding(Utf16("Hello there", false, true)) ==
          TextEncoding::kUtf16Be);
    CHECK(book_formats::DetectTextEncoding(Utf16("No byte order mark", true, false)) ==
          TextEncoding::kUtf16Le);
    CHECK(book_formats::DetectTextEncoding(Utf16("No byte order mark", false, false)) ==
          TextEncoding::kUtf16Be);
    CHECK(book_formats::DetectTextEncoding("") == TextEncoding::kUtf8);
}

TEST_CASE("characters decode in each encoding")
{
    uint32_t cp = 0;
    size_t length = 0;
    REQUIRE(book_formats::DecodeCharacter("\xE2\x80\x9C", 0, TextEncoding::kUtf8, &cp, &length));
    CHECK(cp == 0x201C);
    CHECK(length == 3);
    CHECK_FALSE(book_formats::DecodeCharacter("\xE2\x80", 0, TextEncoding::kUtf8, &cp, &length));
    REQUIRE(book_formats::DecodeCharacter("\xE2x", 0, TextEncoding::kUtf8, &cp, &length));
    CHECK(cp == 0xFFFD);  // malformed, not "wait for more"
    CHECK(length == 1);

    REQUIRE(book_formats::DecodeCharacter("\x93", 0, TextEncoding::kLatin1, &cp, &length));
    CHECK(cp == 0x201C);
    REQUIRE(book_formats::DecodeCharacter("\xE9", 0, TextEncoding::kLatin1, &cp, &length));
    CHECK(cp == 0xE9);

    REQUIRE(book_formats::DecodeCharacter(std::string("A\0", 2), 0, TextEncoding::kUtf16Le, &cp,
                                          &length));
    CHECK(cp == 'A');
    CHECK(length == 2);
    // A surrogate pair is one character; half of one at the end waits for more.
    const std::string pair("\x3D\xD8\x00\xDE", 4);  // U+1F600, little-endian
    REQUIRE(book_formats::DecodeCharacter(pair, 0, TextEncoding::kUtf16Le, &cp, &length));
    CHECK(cp == 0x1F600);
    CHECK(length == 4);
    CHECK_FALSE(book_formats::DecodeCharacter(pair.substr(0, 2), 0, TextEncoding::kUtf16Le, &cp,
                                              &length));
    CHECK_FALSE(book_formats::DecodeCharacter("A", 0, TextEncoding::kUtf16Be, &cp, &length));
}

TEST_CASE("Latin-1 and UTF-16 books read as words, not garbage")
{
    const auto latin1 = epaper_ui::PaginateBookText(480, 800, "A na\xEFve caf\xE9, \x93quoted\x94.",
                                                     TextEncoding::kLatin1);
    CHECK(Joined(latin1) == "A naive cafe, \"quoted\".|");
    CHECK(latin1.text_exhausted);
    CHECK(latin1.consumed_bytes == 23);

    const std::string utf16 = Utf16("Hello from a Windows notepad file.", true, true);
    const auto le = epaper_ui::PaginateBookText(480, 800, utf16, TextEncoding::kUtf16Le);
    CHECK(Joined(le) == "Hello from a Windows notepad file.|");
    CHECK(le.consumed_bytes == utf16.size());

    // Cut in the middle of a unit: stop before it, on an even byte.
    const auto cut = epaper_ui::PaginateBookText(480, 800, utf16.substr(0, 11),
                                                 TextEncoding::kUtf16Le);
    CHECK(cut.consumed_bytes == 10);
    CHECK(Joined(cut) == "Hell|");
}

TEST_CASE("accented UTF-8 letters no longer split words")
{
    const auto layout = epaper_ui::PaginateBookText(480, 800, "A na\xC3\xAFve caf\xC3\xA9.");
    CHECK(Joined(layout) == "A naive cafe.|");
}

// epaper_ui's bridge to book_layout: transliteration, measuring with the real fonts, and
// drawing a laid-out page into a guarded framebuffer. Set KEEPSAKE_PREVIEW_PBM=<path> to also
// write the first page as a portrait PBM image for a look by eye. Sample text is original.
#include <doctest/doctest.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "book_layout/book_layout.h"
#include "epaper_ui/book_page.h"
#include "epub_engine/xhtml_parser.h"

namespace {

constexpr int kRawWidth = 800;
constexpr int kRawHeight = 480;
constexpr int kPortraitWidth = 480;
constexpr int kPortraitHeight = 800;
constexpr int kMargin = 24;
constexpr size_t kGuard = 64;

const char* kSample =
    "<h1>Chapter One</h1>"
    "<p>The kettle began to sing just as the rain arrived, and <b>Mira</b> decided the day was "
    "<i>officially</i> a reading day. It\xE2\x80\x99s the kind of afternoon that asks for "
    "nothing \xE2\x80\x94 only a blanket, a lamp, and one more page.</p>"
    "<p>She wrote three things on a card:<br/>tea<br/>blanket<br/>book</p>"
    "<ul><li>Find the lamp</li><li>Move the cat gently, because the cat has opinions</li></ul>"
    "<hr/>"
    "<h2>A Second Part</h2>"
    "<p>Caf\xC3\xA9 au lait, na\xC3\xAFve plans, and a \xE2\x80\x9Cquick\xE2\x80\x9D nap\xE2\x80\xA6</p>"
    "<p><img src='lamp.png' alt='A small lamp'/></p>";

void WritePbm(const std::vector<uint8_t>& fb, const char* path)
{
    FILE* f = std::fopen(path, "wb");
    if (f == nullptr) {
        return;
    }
    std::fprintf(f, "P1\n%d %d\n", kPortraitWidth, kPortraitHeight);
    for (int y = 0; y < kPortraitHeight; ++y) {
        for (int x = 0; x < kPortraitWidth; ++x) {
            const int raw_x = y;
            const int raw_y = kRawHeight - 1 - x;
            const bool white =
                fb[kGuard + static_cast<size_t>(raw_y) * (kRawWidth / 8) + raw_x / 8] & (0x80 >> (raw_x % 8));
            std::fputc(white ? '0' : '1', f);
        }
        std::fputc('\n', f);
    }
    std::fclose(f);
}

}  // namespace

TEST_CASE("UTF-8 text becomes something the ASCII fonts can draw")
{
    CHECK(epaper_ui::ToDisplayText("plain") == "plain");
    CHECK(epaper_ui::ToDisplayText("It\xE2\x80\x99s") == "It's");
    CHECK(epaper_ui::ToDisplayText("\xE2\x80\x9Cq\xE2\x80\x9D") == "\"q\"");
    CHECK(epaper_ui::ToDisplayText("a\xE2\x80\x94" "b\xE2\x80\xA6") == "a--b...");
    CHECK(epaper_ui::ToDisplayText("Caf\xC3\xA9 na\xC3\xAFve \xC3\x86on Stra\xC3\x9F" "e") ==
          "Cafe naive AEon Strasse");
    CHECK(epaper_ui::ToDisplayText("x\xC2\xA0y\xC2\xADz") == "x yz");  // nbsp, soft hyphen
    CHECK(epaper_ui::ToDisplayText("\xE6\x97\xA5") == "?");            // one '?' per character
    CHECK(epaper_ui::ToDisplayText("\xE6\x97") == "??");               // truncated sequence
    CHECK(epaper_ui::ToDisplayText("\x80") == "?");
}

TEST_CASE("a laid-out chapter page draws inside the framebuffer")
{
    const epub::Chapter chapter = epub::ParseXhtml(kSample, "a.xhtml");
    book_layout::LayoutOptions options;
    options.width = kPortraitWidth - 2 * kMargin;
    options.height = 640;
    options.paragraph_gap = 10;
    const auto pages =
        book_layout::PaginateChapter(chapter, epaper_ui::EpaperTextMeasurer(), options);
    REQUIRE_FALSE(pages.empty());
    REQUIRE_FALSE(pages[0].lines.empty());

    std::vector<uint8_t> fb(kRawWidth * kRawHeight / 8 + 2 * kGuard, 0xFF);
    for (size_t i = 0; i < kGuard; ++i) {
        fb[i] = fb[fb.size() - 1 - i] = 0xA5;
    }
    for (const auto& page : pages) {
        epaper_ui::DrawBookLayoutPage(fb.data() + kGuard, kRawWidth, kRawHeight, kPortraitWidth,
                                      kPortraitHeight, kMargin, 80, page);
    }
    for (size_t i = 0; i < kGuard; ++i) {
        CHECK(fb[i] == 0xA5);
        CHECK(fb[fb.size() - 1 - i] == 0xA5);
    }
    size_t ink = 0;
    for (size_t i = kGuard; i < fb.size() - kGuard; ++i) {
        ink += fb[i] != 0xFF;
    }
    CHECK(ink > 100);

    if (const char* preview = std::getenv("KEEPSAKE_PREVIEW_PBM")) {
        std::vector<uint8_t> first(fb.size(), 0xFF);
        epaper_ui::DrawBookLayoutPage(first.data() + kGuard, kRawWidth, kRawHeight, kPortraitWidth,
                                      kPortraitHeight, kMargin, 80, pages[0]);
        WritePbm(first, preview);
    }
}

TEST_CASE("bold text measures wider than regular with the real fonts")
{
    const epaper_ui::EpaperTextMeasurer measurer;
    const int regular = measurer.Width("Reading day", epub::kStyleNone, book_layout::TextRole::kBody);
    const int bold = measurer.Width("Reading day", epub::kStyleBold, book_layout::TextRole::kBody);
    CHECK(regular > 0);
    CHECK(bold >= regular);
    CHECK(measurer.LineHeight(book_layout::TextRole::kHeading1) >
          measurer.LineHeight(book_layout::TextRole::kBody));
}

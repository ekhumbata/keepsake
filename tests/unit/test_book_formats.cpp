// book_formats: sniffing the real format and choosing the friendly message (#29), including
// the generated test shelf end to end (detect -> open -> message).
#include <doctest/doctest.h>

#include <cstdio>
#include <memory>
#include <string>

#include "book_formats/book_formats.h"

using book_formats::Format;
using book_formats::Problem;

namespace {

std::string ReadHead(const std::string& path)
{
    std::string head(book_formats::kSniffBytes, '\0');
    FILE* f = std::fopen(path.c_str(), "rb");
    REQUIRE(f != nullptr);
    head.resize(std::fread(head.data(), 1, head.size(), f));
    std::fclose(f);
    return head;
}

// What the library/reader will do with a file: sniff, then try the EPUB engine.
Problem Classify(const char* name)
{
    const std::string path = std::string(KEEPSAKE_FIXTURE_DIR) + "/" + name;
    const auto detection = book_formats::Detect(name, ReadHead(path));
    Problem problem = book_formats::ProblemForDetection(detection);
    if (problem != Problem::kNone || detection.format != Format::kEpub) {
        return problem;
    }
    epub::OpenError error = epub::OpenError::kNone;
    auto book = epub::EpubBook::Open(std::make_unique<epub::FileByteSource>(path), &error);
    return book_formats::ProblemForOpenError(error);
}

std::string MobiHeader(uint16_t encryption)
{
    std::string head(200, '\0');
    head.replace(60, 8, "BOOKMOBI");
    head[78] = 0;
    head[79] = 0;
    head[80] = 0;
    head[81] = 100;  // record 0 at offset 100
    head[100 + 12] = static_cast<char>(encryption >> 8);
    head[100 + 13] = static_cast<char>(encryption & 0xFF);
    return head;
}

}  // namespace

TEST_CASE("the test shelf gets the right message")
{
    CHECK(Classify("epub2_ncx.epub") == Problem::kNone);
    CHECK(Classify("epub3_nav.epub") == Problem::kNone);
    CHECK(Classify("font_obfuscated.epub") == Problem::kNone);
    CHECK(Classify("drm_adobe.epub") == Problem::kDrm);
    CHECK(Classify("drm_encryption_only.epub") == Problem::kDrm);
    CHECK(Classify("not_a_zip.epub") == Problem::kPdf);  // a PDF renamed to .epub
    CHECK(Classify("truncated.epub") == Problem::kDamaged);
    CHECK(Classify("no_package.epub") == Problem::kDamaged);
}

TEST_CASE("formats are sniffed from content, extension second")
{
    CHECK(book_formats::Detect("a.pdf", "%PDF-1.7\n...").format == Format::kPdf);
    CHECK(book_formats::Detect("a.txt", "%PDF-1.7\n...").format == Format::kPdf);
    CHECK(book_formats::Detect("notes.txt", "Once upon a time.\nThe end.\n").format == Format::kText);
    CHECK(book_formats::Detect("README", "plain words here\n").format == Format::kText);
    CHECK(book_formats::Detect("photo.jpg", std::string("\xFF\xD8\xFF\xE0\0\x10JFIF", 10)).format ==
          Format::kUnknown);
    CHECK(book_formats::Detect("binary.txt", std::string("ab\0cd", 5)).format == Format::kUnknown);
    CHECK(book_formats::Detect("empty.epub", "").format == Format::kEmpty);
    CHECK(book_formats::Detect("comic.cbz", "PK\x03\x04 rest").format == Format::kComicZip);
    CHECK(book_formats::Detect("archive.zip", "PK\x03\x04 rest").format == Format::kOtherZip);
    CHECK(book_formats::Detect("book.epub", "PK\x03\x04 rest").format == Format::kEpub);
    CHECK(book_formats::Detect("BOOK.EPUB", "not a zip").format == Format::kEpub);
}

TEST_CASE("Kindle files: MOBI encryption flag and KFX")
{
    const auto clear = book_formats::Detect("a.azw3", MobiHeader(0));
    CHECK(clear.format == Format::kMobi);
    CHECK_FALSE(clear.drm);
    CHECK(book_formats::ProblemForDetection(clear) == Problem::kKindle);

    const auto locked = book_formats::Detect("a.azw", MobiHeader(2));
    CHECK(locked.format == Format::kMobi);
    CHECK(locked.drm);
    CHECK(book_formats::ProblemForDetection(locked) == Problem::kDrm);

    const auto kfx = book_formats::Detect("a.kfx", std::string("\xEA" "DRMION\xEE rest"));
    CHECK(kfx.format == Format::kKindleKfx);
    CHECK(book_formats::ProblemForDetection(kfx) == Problem::kDrm);

    // A header too short to hold record 0 doesn't crash or claim DRM.
    std::string short_head = MobiHeader(2).substr(0, 90);
    CHECK_FALSE(book_formats::Detect("a.mobi", short_head).drm);
}

TEST_CASE("every problem has friendly ASCII copy")
{
    for (Problem p : {Problem::kDrm, Problem::kPdf, Problem::kKindle, Problem::kComic,
                      Problem::kDamaged, Problem::kNotABook, Problem::kEmpty,
                      Problem::kCouldNotRead}) {
        const auto message = book_formats::MessageFor(p);
        CHECK(std::string(message.title).size() > 5);
        CHECK(std::string(message.body).size() > 20);
        for (const char* text : {message.title, message.body}) {
            for (const char* c = text; *c != '\0'; ++c) {
                CHECK(static_cast<unsigned char>(*c) < 0x80);  // today's fonts are ASCII
            }
        }
    }
}

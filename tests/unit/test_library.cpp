// library: titles from file names, the index file format, shelf order and labels (#27).
#include <doctest/doctest.h>

#include "library/library.h"

using library::BookRecord;

TEST_CASE("titles are made from file names")
{
    CHECK(library::TitleFromPath("garden_notes.txt") == "Garden notes");
    CHECK(library::TitleFromPath("Austen/the-lighthouse__keepers cat.epub") ==
          "The lighthouse keepers cat");
    CHECK(library::TitleFromPath("notes.v2.txt") == "Notes v2");
    CHECK(library::TitleFromPath(".hidden") == "Hidden");  // dotfiles are skipped by the scan anyway
    CHECK(library::TitleFromPath("___.txt") == "___");
    CHECK(library::Extension("a/B.EPUB") == "epub");
    CHECK(library::Extension("dir.v1/readme").empty());
    CHECK(library::Extension("x.").empty());
}

TEST_CASE("the index round-trips, including awkward characters")
{
    std::vector<BookRecord> records = {
        {"Austen/Emma.epub", "Emma", "Jane Austen", 123456, 1700000000, 1731300000, 42},
        {"odd\tname\\x.txt", "Line\nbreak", "", 10, 5, 0, -1},
    };
    const std::string text = library::SerializeIndex(records);
    CHECK(text.rfind("keepsake-library 1\n", 0) == 0);
    CHECK(library::ParseIndex(text) == records);
}

TEST_CASE("a damaged or foreign index degrades to what can be read")
{
    CHECK(library::ParseIndex("").empty());
    CHECK(library::ParseIndex("keepsake-library 99\na\tb\tc\t1\t2\t3\t4\n").empty());
    const auto records = library::ParseIndex(
        "keepsake-library 1\r\n"
        "good.epub\tGood\t\t1\t2\t3\t150\r\n"
        "too\tfew\tfields\n"
        "bad.epub\tBad\t\tNaN\t2\t3\t4\n"
        "\tNoPath\t\t1\t2\t3\t4\n");
    REQUIRE(records.size() == 1);
    CHECK(records[0].path == "good.epub");
    CHECK(records[0].progress_percent == 100);  // clamped
}

TEST_CASE("the shelf puts the book being read first, then titles A to Z")
{
    std::vector<BookRecord> records = {
        {"c.txt", "cherry", "", 0, 0, 0, -1},
        {"a.epub", "Apple", "", 0, 0, 100, 10},
        {"b.epub", "banana", "", 0, 0, 0, -1},
        {"d.epub", "Date", "", 0, 0, 300, 80},
    };
    library::SortForShelf(&records);
    CHECK(records[0].path == "d.epub");
    CHECK(records[1].path == "a.epub");
    CHECK(records[2].path == "b.epub");
    CHECK(records[3].path == "c.txt");
}

TEST_CASE("shelf labels show progress")
{
    CHECK(library::ShelfLabel({"x.epub", "Emma", "", 0, 0, 0, -1}) == "Emma");
    CHECK(library::ShelfLabel({"x.epub", "Emma", "", 0, 0, 5, 0}) == "Emma");
    CHECK(library::ShelfLabel({"x.epub", "Emma", "", 0, 0, 5, 42}) == "Emma - 42%");
    CHECK(library::ShelfLabel({"x.epub", "Emma", "", 0, 0, 5, 100}) == "Emma - Done");
    CHECK(library::ShelfLabel({"my_book.txt", "", "", 0, 0, 0, -1}) == "My book");
}

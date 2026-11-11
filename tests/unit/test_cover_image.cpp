// Book covers (#27): decoding, fitting, dithering and the cache file.
#include <doctest/doctest.h>

#include <string>

#include "cover_image/cover_image.h"
#include "epub_engine/epub_book.h"

using cover_image::Result;
using cover_image::Thumbnail;

namespace {

std::string FromHex(const char* hex)
{
    std::string out;
    for (size_t i = 0; hex[i] != '\0' && hex[i + 1] != '\0'; i += 2) {
        out.push_back(static_cast<char>(std::stoi(std::string(hex + i, 2), nullptr, 16)));
    }
    return out;
}

// 24 x 36 JPEG made for this test: black top third, a mid-gray square lower down.
const std::string kJpeg = FromHex(
    "ffd8ffe000104a46494600010100000100010000ffdb0043000302020202020302020203030303040604040404040806"
    "06050609080a0a090809090a0c0f0c0a0b0e0b09090d110d0e0f101011100a0c12131210130f101010ffc0000b080024"
    "001801011100ffc4001f0000010501010101010100000000000000000102030405060708090a0bffc400b51000020103"
    "03020403050504040000017d01020300041105122131410613516107227114328191a1082342b1c11552d1f024336272"
    "82090a161718191a25262728292a3435363738393a434445464748494a535455565758595a636465666768696a737475"
    "767778797a838485868788898a92939495969798999aa2a3a4a5a6a7a8a9aab2b3b4b5b6b7b8b9bac2c3c4c5c6c7c8c9"
    "cad2d3d4d5d6d7d8d9dae1e2e3e4e5e6e7e8e9eaf1f2f3f4f5f6f7f8f9faffda0008010100003f00fcaaa28afd54ff00"
    "8718ff00d5d17fe593ff00ddf47fc38c7feae8bff2c9ff00eefa3fe1c63ff5745ff964ff00f77d7e8a7c7af14ebbe0bf"
    "84faef89bc337df63d4acfecbe44fe5249b37dcc48df2b82a72acc391debe41ff86a3f8ebff43cff00e532cfff008d57"
    "d7df017c53aef8d3e13e85e26f135f7db352bcfb579f3f9491efd9732a2fca8028c2aa8e076a3e3d785b5df1a7c27d77"
    "c33e19b1fb66a579f65f220f3523dfb2e6276f99c85185563c9ed5f20ffc32e7c75ffa11bff2a767ff00c76bebef80be"
    "16d77c17f09f42f0cf89ac7ec7a959fdabcf83cd49366fb995d7e64254e5594f07bd7a051457ffd9");

double BlackShare(const Thumbnail& t, int y0, int y1)
{
    int black = 0;
    int total = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = 0; x < t.width; ++x) {
            black += t.black(x, y) ? 1 : 0;
            ++total;
        }
    }
    return total ? static_cast<double>(black) / total : 0.0;
}

std::string Fixture(const char* name)
{
    return std::string(KEEPSAKE_FIXTURE_DIR) + "/" + name;
}

}  // namespace

TEST_CASE("a JPEG cover becomes a dithered thumbnail that fits the box")
{
    Thumbnail thumb;
    REQUIRE(cover_image::MakeThumbnail(kJpeg, 48, 48, &thumb) == Result::kOk);
    // 24x36 fits a 48x48 box at 32x48 (2x is the most it may grow).
    CHECK(thumb.width == 32);
    CHECK(thumb.height == 48);
    CHECK(thumb.bits.size() == static_cast<size_t>(thumb.stride()) * 48);
    CHECK(BlackShare(thumb, 0, 14) > 0.95);   // black band
    CHECK(BlackShare(thumb, 46, 48) < 0.05);  // white bottom edge
    // The gray square comes out as a dither: some black, some white.
    const double gray = BlackShare(thumb, 26, 36);
    CHECK(gray > 0.1);
    CHECK(gray < 0.7);
}

TEST_CASE("shape is kept and small images don't grow past 2x")
{
    Thumbnail thumb;
    REQUIRE(cover_image::MakeThumbnail(kJpeg, 1000, 1000, &thumb) == Result::kOk);
    CHECK(thumb.width == 48);
    CHECK(thumb.height == 72);
    REQUIRE(cover_image::MakeThumbnail(kJpeg, 12, 100, &thumb) == Result::kOk);
    CHECK(thumb.width == 12);
    CHECK(thumb.height == 18);
}

TEST_CASE("the test book's PNG cover can be read out of the EPUB and thumbnailed")
{
    epub::OpenError error = epub::OpenError::kNone;
    auto book = epub::EpubBook::Open(
        std::make_unique<epub::FileByteSource>(Fixture("epub2_ncx.epub")), &error);
    REQUIRE(book != nullptr);
    REQUIRE_FALSE(book->metadata().cover_path.empty());
    std::string png;
    REQUIRE(book->ReadFile(book->metadata().cover_path, 1 << 20, &png));

    Thumbnail thumb;
    REQUIRE(cover_image::MakeThumbnail(png, 40, 60, &thumb) == Result::kOk);
    CHECK(thumb.width == 40);
    CHECK(thumb.height == 60);
    CHECK(BlackShare(thumb, 0, 18) > 0.95);
}

TEST_CASE("bad input is refused, not crashed on")
{
    Thumbnail thumb;
    CHECK(cover_image::MakeThumbnail("", 10, 10, &thumb) == Result::kEmpty);
    CHECK(cover_image::MakeThumbnail("not an image at all", 10, 10, &thumb) ==
          Result::kUndecodable);
    // A cut-off JPEG decodes as far as it goes (the rest comes out gray), or not at all.
    if (cover_image::MakeThumbnail(kJpeg.substr(0, kJpeg.size() / 2), 10, 10, &thumb) ==
        Result::kOk) {
        CHECK(thumb.bits.size() == static_cast<size_t>(thumb.stride()) * thumb.height);
    }
    cover_image::Limits tiny;
    tiny.max_source_pixels = 100;
    CHECK(cover_image::MakeThumbnail(kJpeg, 10, 10, &thumb, tiny) == Result::kTooBig);
    tiny = {};
    tiny.max_input_bytes = 10;
    CHECK(cover_image::MakeThumbnail(kJpeg, 10, 10, &thumb, tiny) == Result::kTooBig);
    CHECK(cover_image::MakeThumbnail(kJpeg, 0, 10, &thumb) == Result::kTooBig);
    CHECK(thumb.width == 0);
}

TEST_CASE("dithering keeps flat white and black flat")
{
    const Thumbnail white = cover_image::Dither(std::vector<uint8_t>(9 * 3, 255), 9, 3);
    const Thumbnail black = cover_image::Dither(std::vector<uint8_t>(9 * 3, 0), 9, 3);
    CHECK(BlackShare(white, 0, 3) == 0.0);
    CHECK(BlackShare(black, 0, 3) == 1.0);
    CHECK(white.stride() == 2);
    CHECK(cover_image::Dither({1, 2}, 5, 5).width == 0);  // too few pixels
}

TEST_CASE("the cache file round-trips and rejects damage")
{
    Thumbnail thumb;
    REQUIRE(cover_image::MakeThumbnail(kJpeg, 30, 30, &thumb) == Result::kOk);
    const std::string file = cover_image::Serialize(thumb);
    Thumbnail back;
    REQUIRE(cover_image::Parse(file, &back));
    CHECK(back == thumb);

    CHECK_FALSE(cover_image::Parse(file.substr(0, file.size() - 1), &back));
    CHECK_FALSE(cover_image::Parse(file + "x", &back));
    std::string wrong_magic = file;
    wrong_magic[0] = 'X';
    CHECK_FALSE(cover_image::Parse(wrong_magic, &back));
    std::string wrong_version = file;
    wrong_version[4] = 9;
    CHECK_FALSE(cover_image::Parse(wrong_version, &back));
    CHECK_FALSE(cover_image::Parse("", &back));
    CHECK(back.width == 0);
}

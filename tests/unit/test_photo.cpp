// Photos (#136): EXIF, turning and fitting, levels, captions and order from file names, and
// the cache name. Only generated images: personal photos never go in the repo.
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "photo/photo.h"

using namespace photo;

namespace {

std::string FromHex(const char* hex)
{
    std::string out;
    for (size_t i = 0; hex[i] != '\0' && hex[i + 1] != '\0'; i += 2) {
        out.push_back(static_cast<char>(std::stoi(std::string(hex + i, 2), nullptr, 16)));
    }
    return out;
}

// 24 x 16 JPEG made with Pillow for this test: the left third black, stored sideways
// (EXIF orientation 6, DateTimeOriginal 2019:05:20 18:30:00). Upright it's 16 x 24 with the
// top third black.
const std::string kSideways = FromHex(
    "ffd8ffe000104a46494600010100000100010000ffe100544578696600004d4d002a0000000800020112000300000001"
    "0006000087690004000000010000002600000000000190030002000000140000003800000000323031393a30353a3230"
    "2031383a33303a303000ffdb0043000302020302020303030304030304050805050404050a070706080c0a0c0c0b0a0b"
    "0b0d0e12100d0e110e0b0b1016101113141515150c0f171816141812141514ffc0000b080010001801011100ffc4001f"
    "0000010501010101010100000000000000000102030405060708090a0bffc400b5100002010303020403050504040000"
    "017d01020300041105122131410613516107227114328191a1082342b1c11552d1f02433627282090a161718191a2526"
    "2728292a3435363738393a434445464748494a535455565758595a636465666768696a737475767778797a8384858687"
    "88898a92939495969798999aa2a3a4a5a6a7a8a9aab2b3b4b5b6b7b8b9bac2c3c4c5c6c7c8c9cad2d3d4d5d6d7d8d9da"
    "e1e2e3e4e5e6e7e8e9eaf1f2f3f4f5f6f7f8f9faffda0008010100003f00fcaaafeaa28afe55ebfaa8a2bfffd9");

Gray Pattern(int width, int height)
{
    Gray image;
    image.width = width;
    image.height = height;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            image.pixels.push_back(static_cast<uint8_t>(y * width + x));
        }
    }
    return image;
}

}  // namespace

TEST_CASE("EXIF orientation and date come out of a phone-style JPEG")
{
    const Exif exif = ReadExif(kSideways);
    CHECK(exif.orientation == 6);
    CHECK(exif.date_time == "2019:05:20 18:30:00");

    CHECK(ReadExif("").orientation == 1);
    CHECK(ReadExif("not a jpeg").orientation == 1);
    // Cut off anywhere, it never reads past the end and falls back to the defaults.
    for (size_t n = 0; n < kSideways.size(); n += 7) {
        const Exif cut = ReadExif(std::string_view(kSideways).substr(0, n));
        CHECK(cut.orientation >= 1);
        CHECK(cut.orientation <= 8);
    }
}

TEST_CASE("the eight orientations turn the right way")
{
    const Gray image = Pattern(3, 2);  // 0 1 2 / 3 4 5
    CHECK(Orient(image, 1).pixels == image.pixels);
    CHECK(Orient(image, 2).pixels == std::vector<uint8_t>{2, 1, 0, 5, 4, 3});
    CHECK(Orient(image, 3).pixels == std::vector<uint8_t>{5, 4, 3, 2, 1, 0});
    CHECK(Orient(image, 4).pixels == std::vector<uint8_t>{3, 4, 5, 0, 1, 2});
    const Gray six = Orient(image, 6);  // a quarter turn clockwise
    CHECK(six.width == 2);
    CHECK(six.height == 3);
    CHECK(six.pixels == std::vector<uint8_t>{3, 0, 4, 1, 5, 2});
    CHECK(Orient(image, 8).pixels == std::vector<uint8_t>{2, 5, 1, 4, 0, 3});
    CHECK(Orient(image, 5).pixels == std::vector<uint8_t>{0, 3, 1, 4, 2, 5});
    CHECK(Orient(image, 7).pixels == std::vector<uint8_t>{5, 2, 4, 1, 3, 0});
    CHECK(Orient(image, 9).pixels == image.pixels);
}

TEST_CASE("photos fit the screen keeping their shape")
{
    const Gray portrait = FitInside(Pattern(3000, 4000), kBoxWidth, kBoxHeight);
    CHECK(portrait.width == 480);
    CHECK(portrait.height == 640);
    const Gray landscape = FitInside(Pattern(4000, 3000), kBoxWidth, kBoxHeight);
    CHECK(landscape.width == 480);
    CHECK(landscape.height == 360);
    const Gray tall = FitInside(Pattern(100, 1000), kBoxWidth, kBoxHeight);
    CHECK(tall.height == 760);
    CHECK(tall.width == 76);
    const Gray tiny = FitInside(Pattern(10, 10), kBoxWidth, kBoxHeight);  // at most 2x
    CHECK(tiny.width == 20);
    CHECK(FitInside(Gray{}, 10, 10).width == 0);
}

TEST_CASE("levels stretch a dull photo to full range")
{
    Gray dull;
    dull.width = 100;
    dull.height = 1;
    for (int i = 0; i < 100; ++i) {
        dull.pixels.push_back(static_cast<uint8_t>(100 + i / 2));  // 100..149
    }
    AutoLevels(&dull);
    CHECK(dull.pixels.front() == 0);
    CHECK(dull.pixels.back() == 255);
    // Flat images are left alone.
    Gray flat;
    flat.width = 4;
    flat.height = 1;
    flat.pixels = {200, 201, 202, 203};
    AutoLevels(&flat);
    CHECK(flat.pixels[0] > 200);  // only the gentle mid-tone lift
}

TEST_CASE("a sideways phone photo is prepared upright")
{
    cover_image::Thumbnail out;
    REQUIRE(PrepareEncoded(kSideways, &out) == Result::kOk);
    CHECK(out.width == 32);   // 16 x 24, enlarged 2x at most
    CHECK(out.height == 48);
    int top_black = 0;
    int bottom_black = 0;
    for (int x = 0; x < out.width; ++x) {
        top_black += out.black(x, 4);
        bottom_black += out.black(x, out.height - 4);
    }
    CHECK(top_black > out.width * 3 / 4);
    CHECK(bottom_black < out.width / 4);
}

TEST_CASE("formats are recognised so the friend gets a plain answer")
{
    CHECK(FormatName(kSideways) == "JPEG");
    CHECK(FormatName(std::string("\x89PNG\r\n\x1a\n....", 12)) == "PNG");
    CHECK(FormatName("GIF89a") == "GIF");
    CHECK(FormatName(std::string("\0\0\0\x18" "ftypheic", 12)) == "HEIC");
    CHECK(FormatName(std::string("\0\0\0\x18" "ftypavif", 12)) == "AVIF");
    CHECK(FormatName(std::string("RIFF\x10\0\0\0WEBPVP8 ", 16)) == "WebP");
    CHECK(FormatName("hello") == "");
    cover_image::Thumbnail out;
    CHECK(PrepareEncoded(std::string("\0\0\0\x18" "ftypheic....", 16), &out) == Result::kUnsupported);
    CHECK(PrepareEncoded("garbage!", &out) == Result::kUndecodable);
    CHECK(PrepareEncoded("", &out) == Result::kEmpty);
    cover_image::Limits small;
    small.max_source_pixels = 100;
    CHECK(PrepareEncoded(kSideways, &out, small) == Result::kTooBig);
}

TEST_CASE("which files are photos")
{
    CHECK(IsPhotoName("beach.jpg"));
    CHECK(IsPhotoName("2019/Beach.JPEG"));
    CHECK(IsPhotoName("a.png"));
    CHECK(IsPhotoName("a.gif"));
    CHECK_FALSE(IsPhotoName("._beach.jpg"));
    CHECK_FALSE(IsPhotoName(".hidden.jpg"));
    CHECK_FALSE(IsPhotoName("beach.heic"));
    CHECK_FALSE(IsPhotoName("notes.txt"));
    CHECK_FALSE(IsPhotoName("jpg"));
}

TEST_CASE("captions come from file names, else the photo's date")
{
    CHECK(CaptionFor("2019-05-20 First date.jpg", "").Text() == "First date, May 20, 2019");
    CHECK(CaptionFor("2019_05_20_first_date.jpg", "").Text() == "first date, May 20, 2019");
    CHECK(CaptionFor("20190520 - Beach.jpg", "").Text() == "Beach, May 20, 2019");
    CHECK(CaptionFor("2019-05-20.jpg", "").Text() == "May 20, 2019");
    CHECK(CaptionFor("trips/Beach day.png", "").Text() == "Beach day");
    CHECK(CaptionFor("Beach day.png", "2021:07:04 12:00:00").Text() == "Beach day, July 4, 2021");
    CHECK(CaptionFor("IMG_1234.JPG", "2021:07:04 12:00:00").Text() == "July 4, 2021");
    CHECK(CaptionFor("IMG_1234.JPG", "").Text().empty());
    CHECK(CaptionFor("PXL_20230115_183012345.jpg", "").Text() == "January 15, 2023");
    CHECK(CaptionFor("DSC01234.jpg", "").Text().empty());
    CHECK(CaptionFor("2019-02-30 Oops.jpg", "").Text() == "2019-02-30 Oops");  // not a date
    CHECK(CaptionFor("Paris.jpg", "").Text() == "Paris");
    CHECK(CaptionFor("Photos of us.jpg", "").Text() == "Photos of us");
}

TEST_CASE("oldest first, undated last, then by name")
{
    std::vector<Entry> entries = {
        {"c.jpg", CaptionFor("c.jpg", "")},
        {"2021-01-01 b.jpg", CaptionFor("2021-01-01 b.jpg", "")},
        {"a.jpg", CaptionFor("a.jpg", "2019:05:20 10:00:00")},
        {"2019-05-20 z.jpg", CaptionFor("2019-05-20 z.jpg", "")},
        {"b.jpg", CaptionFor("b.jpg", "")},
    };
    SortOldestFirst(&entries);
    std::vector<std::string> order;
    for (const Entry& e : entries) {
        order.push_back(e.path);
    }
    CHECK(order == std::vector<std::string>{"2019-05-20 z.jpg", "a.jpg", "2021-01-01 b.jpg",
                                            "b.jpg", "c.jpg"});
}

TEST_CASE("the cache name follows the file")
{
    const std::string name = CacheName("beach.jpg", 1000, 1700000000);
    CHECK(name.size() == 20);
    CHECK(name.substr(16) == ".kpb");
    CHECK(CacheName("beach.jpg", 1000, 1700000000) == name);
    CHECK(CacheName("beach.jpg", 1001, 1700000000) != name);
    CHECK(CacheName("beach.jpg", 1000, 1700000001) != name);
    CHECK(CacheName("beach2.jpg", 1000, 1700000000) != name);
}

#ifndef PHOTO_PHOTO_H_
#define PHOTO_PHOTO_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "cover_image/cover_image.h"

// Photos (#136): pictures of the two of them on the SD card (`/photos`), shown one at a time,
// full screen, on the 1-bit panel. Each photo is prepared once (decoded, turned the right way
// up, fitted, levelled, dithered) and cached, so flipping through is just a read and a refresh.
// This part has no hardware in it; photo_service does the card and the device-only decoder.
namespace photo {

// The panel area a photo is fitted into: the whole portrait screen minus the caption line.
inline constexpr int kScreenWidth = 480;
inline constexpr int kScreenHeight = 800;
inline constexpr int kCaptionHeight = 40;
inline constexpr int kBoxWidth = kScreenWidth;
inline constexpr int kBoxHeight = kScreenHeight - kCaptionHeight;

// --- EXIF -------------------------------------------------------------------------------------

struct Exif {
    int orientation = 1;     // 1..8 as in the EXIF spec; 1 = as stored
    std::string date_time;   // DateTimeOriginal, "2019:05:20 18:30:00", or empty
};

// Reads Orientation and DateTimeOriginal from a JPEG's APP1 Exif block. Anything missing or
// malformed is just left at its default; never reads outside `jpeg`.
Exif ReadExif(std::string_view jpeg);

// --- images -----------------------------------------------------------------------------------

struct Gray {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;  // one byte per pixel, 0 black .. 255 white
};

// Turns an image the way EXIF orientation `orientation` says (1..8; others leave it as is).
Gray Orient(const Gray& image, int orientation);

// Fits inside box_width x box_height keeping the shape, never enlarging more than 2x.
Gray FitInside(const Gray& image, int box_width, int box_height);

// Stretches the tones so the darkest ~1% is black and the lightest ~1% white, then lifts the
// mid-tones a little: dithered photos otherwise come out muddy and dark on e-paper.
void AutoLevels(Gray* image);

enum class Result : uint8_t {
    kOk,
    kEmpty,
    kTooBig,        // over the decoder's limits; a smaller copy would work
    kUnsupported,   // a format we can't read (HEIC, WebP, ...); see FormatName
    kUndecodable,   // damaged
};

// What kind of file this looks like, by its first bytes: "JPEG", "PNG", "GIF", "HEIC", "WebP",
// "TIFF", "BMP", or "" when unknown.
std::string FormatName(std::string_view head);
bool CanDecode(std::string_view format_name);

// Orients, fits, levels and dithers an already-decoded photo.
cover_image::Thumbnail Prepare(const Gray& decoded, int orientation);

// The whole path for a file in memory (stb_image: JPEG, PNG, GIF up to the cover limits).
Result PrepareEncoded(std::string_view encoded, cover_image::Thumbnail* out,
                      const cover_image::Limits& limits = {});

// --- the photos folder ----------------------------------------------------------------------

// .jpg/.jpeg/.png/.gif, not hidden ("._x.jpg" from macOS is skipped too).
bool IsPhotoName(std::string_view filename);

struct Date {
    int year = 0;
    int month = 0;
    int day = 0;
    bool valid() const { return year > 0; }
};

struct Caption {
    std::string title;  // "First date", or empty
    Date date;          // from the name, else EXIF, else unset
    // "First date · May 20, 2019", "May 20, 2019", "First date", or "" (ASCII "-" instead of
    // the middle dot until the fonts have it).
    std::string Text() const;
};

// "2019-05-20 First date.jpg", "20190520_beach.jpg", "Beach day.png", "IMG_1234.JPG"...
// `exif_date` ("2019:05:20 18:30:00") fills in the date when the name has none. Camera names
// (IMG_1234, DSC01234, PXL_..., Photo 2019-...) give no title.
Caption CaptionFor(std::string_view filename, std::string_view exif_date);

struct Entry {
    std::string path;    // relative to /photos, e.g. "2019/beach.jpg"
    Caption caption;
};

// Oldest first by date (undated photos after dated ones), then by path.
void SortOldestFirst(std::vector<Entry>* entries);

// The cache file name for a photo: a hash of its path, size and modification time, so a
// replaced file is prepared again. 16 hex digits + ".kpb".
std::string CacheName(std::string_view path, uint64_t size, int64_t modified);

}  // namespace photo

#endif  // PHOTO_PHOTO_H_

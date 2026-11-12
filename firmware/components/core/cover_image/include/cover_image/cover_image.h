#ifndef COVER_IMAGE_COVER_IMAGE_H_
#define COVER_IMAGE_COVER_IMAGE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Book covers (#27): an EPUB's cover image (JPEG, PNG or GIF) as a small 1-bit thumbnail for
// the shelf, and the cache file that keeps it so each cover is only decoded once. Portable:
// the firmware reads the image out of the book and writes the cache; this does the rest.
namespace cover_image {

// 1 bit per pixel, rows top to bottom, each row padded to whole bytes, most significant bit
// first. A set bit is black ink.
struct Thumbnail {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> bits;

    int stride() const { return (width + 7) / 8; }
    bool black(int x, int y) const
    {
        return (bits[static_cast<size_t>(y) * stride() + static_cast<size_t>(x) / 8] >>
                (7 - x % 8)) & 1;
    }
    bool operator==(const Thumbnail& other) const = default;
};

struct Limits {
    size_t max_input_bytes = 4u * 1024u * 1024u;
    // Decoding holds the whole image at one byte per pixel: this caps that buffer.
    uint64_t max_source_pixels = 4u * 1024u * 1024u;
};

enum class Result {
    kOk,
    kEmpty,        // no bytes
    kTooBig,       // over the limits (bytes or pixels)
    kUndecodable,  // not an image we can read, or damaged
};

// Decodes `encoded`, scales it to fit inside box_width x box_height keeping its shape (never
// enlarging past 2x), and dithers it to black and white. The thumbnail is at least 1x1.
Result MakeThumbnail(std::string_view encoded, int box_width, int box_height, Thumbnail* out,
                     const Limits& limits = {});

// Decodes `encoded` (JPEG, PNG or GIF) to 8-bit grayscale, one byte per pixel, rows top to
// bottom. Same limits as MakeThumbnail. Photos (#136) build on this.
Result DecodeGray(std::string_view encoded, std::vector<uint8_t>* gray, int* width, int* height,
                  const Limits& limits = {});

// Area-average resampling of an 8-bit grayscale image to out_width x out_height (both > 0).
std::vector<uint8_t> Resample(const uint8_t* src, int width, int height, int out_width,
                              int out_height);

// Dithers an 8-bit grayscale image (0 black .. 255 white) to 1 bit (Atkinson: high contrast
// with clean whites, which suits small covers on e-paper).
Thumbnail Dither(const std::vector<uint8_t>& gray, int width, int height);

// The cache file: a short header (magic, version, size) and the bits. Parse rejects anything
// that doesn't add up, so a damaged cache file just means decoding the cover again.
std::string Serialize(const Thumbnail& thumbnail);
bool Parse(std::string_view data, Thumbnail* out);

}  // namespace cover_image

#endif  // COVER_IMAGE_COVER_IMAGE_H_

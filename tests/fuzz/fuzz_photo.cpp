// libFuzzer target: photos from the SD card (#136), whatever the friend drops in: EXIF,
// format sniffing, decoding and preparing, and file names.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include "photo/photo.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::string_view bytes(reinterpret_cast<const char*>(data), size);
    const photo::Exif exif = photo::ReadExif(bytes);
    if (exif.orientation < 1 || exif.orientation > 8) {
        std::abort();
    }
    (void)photo::FormatName(bytes);
    cover_image::Limits limits;
    limits.max_source_pixels = 1u << 20;  // keep each run small
    cover_image::Thumbnail out;
    if (photo::PrepareEncoded(bytes, &out, limits) == photo::Result::kOk) {
        if (out.width <= 0 || out.height <= 0 || out.width > photo::kBoxWidth * 2 ||
            out.height > photo::kBoxHeight * 2) {
            std::abort();
        }
    }
    const photo::Caption caption = photo::CaptionFor(bytes, bytes);
    (void)caption.Text();
    (void)photo::IsPhotoName(bytes);
    (void)photo::CacheName(bytes, size, static_cast<int64_t>(size));
    return 0;
}

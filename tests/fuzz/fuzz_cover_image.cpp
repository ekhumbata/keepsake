// libFuzzer target: cover images straight out of EPUBs (any bytes a book claims is its cover)
// and the cover cache files on the SD card.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#include "cover_image/cover_image.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1) {
        return 0;
    }
    const std::string_view bytes(reinterpret_cast<const char*>(data + 1), size - 1);
    // Small limits keep each run fast; the logic doesn't depend on the numbers.
    cover_image::Limits limits;
    limits.max_source_pixels = 512u * 512u;
    const int box = 8 + data[0] % 120;

    cover_image::Thumbnail thumb;
    if (cover_image::MakeThumbnail(bytes, box, box * 3 / 2, &thumb, limits) ==
        cover_image::Result::kOk) {
        if (thumb.width < 1 || thumb.height < 1 || thumb.width > box ||
            thumb.height > box * 3 / 2 ||
            thumb.bits.size() != static_cast<size_t>(thumb.stride()) * thumb.height) {
            std::abort();
        }
        cover_image::Thumbnail back;
        if (!cover_image::Parse(cover_image::Serialize(thumb), &back) || !(back == thumb)) {
            std::abort();
        }
    }
    cover_image::Thumbnail parsed;
    if (cover_image::Parse(bytes, &parsed) &&
        parsed.bits.size() != static_cast<size_t>(parsed.stride()) * parsed.height) {
        std::abort();
    }
    return 0;
}

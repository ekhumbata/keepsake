#include "tjpgd_decode.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <new>

#include "esp32s3/rom/tjpgd.h"
#include "esp_log.h"

namespace photo_service {
namespace {

constexpr const char* kTag = "PhotoJpeg";
constexpr size_t kPoolBytes = 4096;  // TJpgDec's work area (needs ~3.1 KB)

// The ROM header's exact callback types differ between IDF versions (UINT vs uint32_t), so
// take them from the functions themselves.
template <typename F>
struct Args;
template <typename R, typename A0, typename A1, typename A2, typename A3, typename A4>
struct Args<R (*)(A0, A1, A2, A3, A4)> {
    using second = A1;
};
template <typename R, typename A0, typename A1, typename A2>
struct Args<R (*)(A0, A1, A2)> {
    using second = A1;
};
template <typename F>
struct Ret;
template <typename R, typename... A>
struct Ret<R (*)(A...)> {
    using type = R;
};

using InFn = typename Args<decltype(&jd_prepare)>::second;
using OutFn = typename Args<decltype(&jd_decomp)>::second;
using InRet = typename Ret<InFn>::type;
using OutRet = typename Ret<OutFn>::type;

struct Context {
    FILE* file = nullptr;
    photo::Gray* gray = nullptr;
};

}  // namespace

bool DecodeJpegScaled(FILE* file, photo::Gray* out)
{
    if (file == nullptr || out == nullptr) {
        return false;
    }
    std::unique_ptr<uint8_t[]> pool(new (std::nothrow) uint8_t[kPoolBytes]);
    if (!pool) {
        return false;
    }
    Context context;
    context.file = file;
    context.gray = out;
    JDEC decoder = {};
    InFn in = [](JDEC* jd, auto* buffer, auto length) -> InRet {
        FILE* f = static_cast<Context*>(jd->device)->file;
        if (buffer == nullptr) {  // skip
            return std::fseek(f, static_cast<long>(length), SEEK_CUR) == 0 ? static_cast<InRet>(length)
                                                                           : 0;
        }
        return static_cast<InRet>(std::fread(buffer, 1, length, f));
    };
    std::rewind(file);
    const JRESULT prepared = jd_prepare(&decoder, in, pool.get(), kPoolBytes, &context);
    if (prepared != JDR_OK) {
        ESP_LOGI(kTag, "TJpgDec can't read this one (%d), e.g. progressive", static_cast<int>(prepared));
        return false;
    }
    const int width = decoder.width;
    const int height = decoder.height;
    // The smallest scale that still covers the photo area (portrait or landscape).
    int scale = 0;
    while (scale < 3) {
        const int w = width >> (scale + 1);
        const int h = height >> (scale + 1);
        const bool covers = std::max(w, h) >= photo::kBoxHeight && std::min(w, h) >= photo::kBoxWidth / 2;
        if (!covers) {
            break;
        }
        ++scale;
    }
    out->width = (width + (1 << scale) - 1) >> scale;
    out->height = (height + (1 << scale) - 1) >> scale;
    out->pixels.assign(static_cast<size_t>(out->width) * static_cast<size_t>(out->height), 255);

    OutFn write = [](JDEC* jd, void* bitmap, JRECT* rect) -> OutRet {
        photo::Gray* gray = static_cast<Context*>(jd->device)->gray;
        const auto* rgb = static_cast<const uint8_t*>(bitmap);
        for (int y = rect->top; y <= rect->bottom; ++y) {
            for (int x = rect->left; x <= rect->right; ++x, rgb += 3) {
                if (x < gray->width && y < gray->height) {
                    gray->pixels[static_cast<size_t>(y) * static_cast<size_t>(gray->width) +
                                 static_cast<size_t>(x)] =
                        static_cast<uint8_t>((rgb[0] * 77 + rgb[1] * 150 + rgb[2] * 29) >> 8);
                }
            }
        }
        return 1;  // carry on
    };
    const JRESULT decoded = jd_decomp(&decoder, write, static_cast<uint8_t>(scale));
    if (decoded != JDR_OK) {
        ESP_LOGW(kTag, "TJpgDec decode failed (%d)", static_cast<int>(decoded));
        return false;
    }
    return true;
}

}  // namespace photo_service

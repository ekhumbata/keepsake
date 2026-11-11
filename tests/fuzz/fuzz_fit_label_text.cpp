// libFuzzer target: label fitting and multi-line wrapping on arbitrary bytes (book file
// names, Wi-Fi SSIDs and transcripts all reach these from outside the device).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "epaper_ui/font_renderer.h"
#include "render_utils.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 2) {
        return 0;
    }
    const int max_width = data[0] * 2;  // 0..510 px, covers the zero and tiny-width edges
    const auto role = static_cast<design::TypographyRole>(data[1] % 20);
    const std::string text(reinterpret_cast<const char*>(data + 2), size - 2);

    const std::string fitted = epaper_ui::FitLabelText(role, text, max_width);
    if (fitted.size() > text.size() + 3) {  // at most an added ellipsis
        std::abort();
    }
    (void)epaper_ui::MeasureText(role, text);
    return 0;
}

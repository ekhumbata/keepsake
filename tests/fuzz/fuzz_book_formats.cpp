// libFuzzer target: format sniffing on arbitrary file heads (every file on the SD card).
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "book_formats/book_formats.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1) {
        return 0;
    }
    static constexpr const char* kNames[] = {"a.epub", "a.txt", "a.azw3", "a.kfx", "a.cbz", "a"};
    const std::string_view head(reinterpret_cast<const char*>(data + 1), size - 1);
    const auto detection = book_formats::Detect(kNames[data[0] % 6], head);
    (void)book_formats::MessageFor(book_formats::ProblemForDetection(detection));
    return 0;
}

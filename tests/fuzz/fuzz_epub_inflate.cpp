// libFuzzer target: raw DEFLATE decoding of arbitrary bytes (every compressed zip entry in a
// book goes through this).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "epub_engine/inflate.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    constexpr size_t kMaxOutput = 1u << 20;
    std::string out;
    const epub::InflateStatus status = epub::InflateRaw(data, size, kMaxOutput, &out);
    if (out.size() > kMaxOutput) {
        std::abort();
    }
    (void)status;
    return 0;
}

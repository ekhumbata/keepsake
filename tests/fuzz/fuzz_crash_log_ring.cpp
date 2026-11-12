// libFuzzer target: the crash log ring attached to whatever RAM held at boot (#41). After a
// power-up the block is garbage; Attach must never trust or overrun it.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "crash_report/crash_report.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < crash_report::LogRing::kHeaderBytes + 1) {
        return 0;
    }
    std::vector<uint8_t> memory(data, data + size);
    crash_report::LogRing ring;
    ring.Attach(memory.data(), memory.size());
    const std::string before = ring.Contents();
    if (before.size() > ring.capacity()) {
        std::abort();
    }
    ring.Append(std::string(reinterpret_cast<const char*>(data), size % 97));
    if (ring.Contents().size() > ring.capacity()) {
        std::abort();
    }
    crash_report::LogRing again;
    if (!again.Attach(memory.data(), memory.size()) || again.Contents() != ring.Contents()) {
        std::abort();  // a ring we wrote is always trusted and reads back the same
    }
    return 0;
}

// libFuzzer target: the release manifest the device downloads before an update (#40).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include "firmware_update/firmware_update.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    firmware_update::Release release;
    std::string error;
    const bool ok = firmware_update::ParseManifest(text, &release, &error);
    if (!ok && error.empty()) {
        std::abort();  // a refusal always says why
    }
    if (ok) {
        if (release.url.rfind("https://", 0) != 0 || release.size == 0 ||
            !firmware_update::IsLowerHex(release.sha256, 64) ||
            release.notes.size() > firmware_update::kMaxNotesBytes) {
            std::abort();
        }
        firmware_update::Version again;
        if (!firmware_update::ParseVersion(release.version_text, &again) ||
            firmware_update::Compare(again, release.version) != 0) {
            std::abort();
        }
        (void)firmware_update::Decide({1, 0, 0, false}, release, "", 0x7E0000);
    }
    firmware_update::Version version;
    if (firmware_update::ParseVersion(text, &version)) {
        if (firmware_update::Compare(version, version) != 0) {
            std::abort();
        }
    }
    return 0;
}

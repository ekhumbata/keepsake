// libFuzzer target: the provisioning file (hand-written JSON on the SD card, #42).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#include "provisioning/mini_json.h"
#include "provisioning/provision.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    mini_json::Value root;
    std::string error;
    if (!mini_json::Parse(text, &root, &error) && error.empty()) {
        std::abort();  // a refusal always says why
    }
    provisioning::Provision provision;
    const auto result = provisioning::ParseProvision(text, &provision);
    if (result.ok != result.problems.empty()) {
        std::abort();
    }
    if (!result.ok && !provision.empty()) {
        std::abort();  // nothing is imported from a refused file
    }
    if (provision.wifi.size() > provisioning::kMaxWifiNetworks) {
        std::abort();
    }
    (void)provisioning::DescribeForLog(provision);
    return 0;
}

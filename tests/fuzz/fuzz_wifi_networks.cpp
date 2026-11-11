// libFuzzer target: the saved Wi-Fi networks as read back from NVS (#43), then edited.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#include "wifi_networks/wifi_networks.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    wifi_networks::NetworkList list = wifi_networks::NetworkList::Parse(text);
    if (list.networks().size() > wifi_networks::kMaxNetworks) {
        std::abort();
    }
    if (!(wifi_networks::NetworkList::Parse(list.Serialize()) == list)) {
        std::abort();  // what's stored reads back the same
    }
    // Use the first bytes as names to add and mark, like a person adding networks.
    const std::string_view name = text.substr(0, size % 40);
    list.Add(name, text.substr(0, size % 70));
    list.MarkUsed(name);
    if (list.networks().size() > wifi_networks::kMaxNetworks) {
        std::abort();
    }
    const std::vector<wifi_networks::SeenNetwork> scan = {{std::string(name), -50}};
    const auto candidates = list.Candidates(&scan);
    if (candidates.size() > list.networks().size()) {
        std::abort();
    }
    return 0;
}

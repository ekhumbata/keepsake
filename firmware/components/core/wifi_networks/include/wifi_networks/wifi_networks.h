#ifndef WIFI_NETWORKS_WIFI_NETWORKS_H_
#define WIFI_NETWORKS_WIFI_NETWORKS_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Saved Wi-Fi networks (#43): the friend's home, Eric's home for testing, a phone hotspot.
// Keeps up to kMaxNetworks, remembers which was used most recently, and decides the order to
// try them in. Portable: wifi_service stores Serialize()'s text in NVS and does the joining.
namespace wifi_networks {

inline constexpr size_t kMaxNetworks = 5;

struct Network {
    std::string ssid;
    std::string password;  // empty for an open network
    uint32_t last_used = 0;  // higher = used more recently (a counter, not a time)

    bool operator==(const Network& other) const = default;
};

struct SeenNetwork {
    std::string ssid;
    int rssi = -127;  // dBm, from a scan
};

class NetworkList {
public:
    const std::vector<Network>& networks() const { return networks_; }
    bool empty() const { return networks_.empty(); }
    const Network* Find(std::string_view ssid) const;

    // Saves a network as the most recently used (a known SSID gets its new password). When the
    // list is full, the least recently used network makes room. False for an empty SSID or one
    // longer than 32 bytes.
    bool Add(std::string_view ssid, std::string_view password);
    bool Remove(std::string_view ssid);
    // After a successful connection: tried first next time.
    void MarkUsed(std::string_view ssid);

    // The order to try networks in. With a scan, the saved networks it saw, strongest first
    // (ties: most recently used); if it saw none, all of them by recency. Without a scan, all
    // of them, most recently used first.
    std::vector<Network> Candidates(const std::vector<SeenNetwork>* scan = nullptr) const;

    // Storage: a version header, then one network per line (escaped, tab-separated). Parse
    // skips malformed lines and keeps at most kMaxNetworks; an unknown version gives an empty
    // list.
    std::string Serialize() const;
    static NetworkList Parse(std::string_view text);

    bool operator==(const NetworkList& other) const = default;

private:
    // A stamp newer than every saved one (renumbering them first if the counter would wrap).
    uint32_t NextStamp();

    std::vector<Network> networks_;  // most recently used first
};

}  // namespace wifi_networks

#endif  // WIFI_NETWORKS_WIFI_NETWORKS_H_

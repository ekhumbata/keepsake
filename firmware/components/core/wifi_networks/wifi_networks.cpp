#include "wifi_networks/wifi_networks.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace wifi_networks {
namespace {

constexpr std::string_view kHeader = "keepsake-wifi 1";
constexpr size_t kMaxSsidBytes = 32;
constexpr size_t kMaxPasswordBytes = 64;

std::string Escape(std::string_view text)
{
    std::string out;
    for (const char c : text) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::string Unescape(std::string_view text)
{
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\\' || i + 1 >= text.size()) {
            out.push_back(text[i]);
            continue;
        }
        const char next = text[++i];
        out.push_back(next == 't' ? '\t' : next == 'n' ? '\n' : next == 'r' ? '\r' : next);
    }
    return out;
}

bool MoreRecent(const Network& a, const Network& b)
{
    return a.last_used > b.last_used;
}

}  // namespace

const Network* NetworkList::Find(std::string_view ssid) const
{
    for (const Network& network : networks_) {
        if (network.ssid == ssid) {
            return &network;
        }
    }
    return nullptr;
}

uint32_t NetworkList::NextStamp()
{
    uint32_t newest = 0;
    for (const Network& network : networks_) {
        newest = std::max(newest, network.last_used);
    }
    if (newest == UINT32_MAX) {
        // Renumber oldest = 1 .. newest = n, keeping the order (networks_ is newest first).
        std::stable_sort(networks_.begin(), networks_.end(), MoreRecent);
        uint32_t stamp = static_cast<uint32_t>(networks_.size());
        for (Network& network : networks_) {
            network.last_used = stamp--;
        }
        newest = static_cast<uint32_t>(networks_.size());
    }
    return newest + 1;
}

bool NetworkList::Add(std::string_view ssid, std::string_view password)
{
    if (ssid.empty() || ssid.size() > kMaxSsidBytes || password.size() > kMaxPasswordBytes) {
        return false;
    }
    const uint32_t stamp = NextStamp();
    Remove(ssid);
    if (networks_.size() >= kMaxNetworks) {
        networks_.pop_back();  // least recently used (the list is kept newest first)
    }
    networks_.insert(networks_.begin(), Network{std::string(ssid), std::string(password), stamp});
    return true;
}

bool NetworkList::Remove(std::string_view ssid)
{
    const auto it = std::find_if(networks_.begin(), networks_.end(),
                                 [&](const Network& n) { return n.ssid == ssid; });
    if (it == networks_.end()) {
        return false;
    }
    networks_.erase(it);
    return true;
}

void NetworkList::MarkUsed(std::string_view ssid)
{
    const auto it = std::find_if(networks_.begin(), networks_.end(),
                                 [&](const Network& n) { return n.ssid == ssid; });
    if (it == networks_.end()) {
        return;
    }
    Network network = *it;
    network.last_used = NextStamp();
    networks_.erase(it);
    networks_.insert(networks_.begin(), std::move(network));
}

std::vector<Network> NetworkList::Candidates(const std::vector<SeenNetwork>* scan) const
{
    std::vector<Network> by_recency = networks_;
    std::stable_sort(by_recency.begin(), by_recency.end(), MoreRecent);
    if (scan == nullptr) {
        return by_recency;
    }
    struct Seen {
        Network network;
        int rssi;
    };
    std::vector<Seen> seen;
    for (const Network& network : by_recency) {
        int best = -128;
        bool found = false;
        for (const SeenNetwork& result : *scan) {
            if (result.ssid == network.ssid) {
                best = std::max(best, result.rssi);
                found = true;
            }
        }
        if (found) {
            seen.push_back({network, best});
        }
    }
    if (seen.empty()) {
        return by_recency;  // none in range (or a scan that missed them): try them all anyway
    }
    std::stable_sort(seen.begin(), seen.end(),
                     [](const Seen& a, const Seen& b) { return a.rssi > b.rssi; });
    std::vector<Network> out;
    for (Seen& s : seen) {
        out.push_back(std::move(s.network));
    }
    return out;
}

std::string NetworkList::Serialize() const
{
    std::string out(kHeader);
    out.push_back('\n');
    for (const Network& network : networks_) {
        out += std::to_string(network.last_used) + '\t' + Escape(network.ssid) + '\t' +
               Escape(network.password) + '\n';
    }
    return out;
}

NetworkList NetworkList::Parse(std::string_view text)
{
    NetworkList list;
    size_t start = 0;
    bool first = true;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        std::string_view line = text.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (first) {
            first = false;
            if (line != kHeader) {
                return {};
            }
            continue;
        }
        const size_t tab1 = line.find('\t');
        const size_t tab2 = tab1 == std::string_view::npos ? tab1 : line.find('\t', tab1 + 1);
        if (tab2 == std::string_view::npos || line.find('\t', tab2 + 1) != std::string_view::npos) {
            continue;
        }
        const std::string stamp_text(line.substr(0, tab1));
        char* stop = nullptr;
        const unsigned long stamp = std::strtoul(stamp_text.c_str(), &stop, 10);
        if (stamp_text.empty() || stamp_text.size() > 10 || *stop != '\0' || stamp > UINT32_MAX) {
            continue;
        }
        const std::string ssid = Unescape(line.substr(tab1 + 1, tab2 - tab1 - 1));
        const std::string password = Unescape(line.substr(tab2 + 1));
        if (ssid.empty() || ssid.size() > kMaxSsidBytes || password.size() > kMaxPasswordBytes ||
            list.Find(ssid) != nullptr || list.networks_.size() >= kMaxNetworks) {
            continue;
        }
        list.networks_.push_back(Network{ssid, password, static_cast<uint32_t>(stamp)});
    }
    std::stable_sort(list.networks_.begin(), list.networks_.end(), MoreRecent);
    return list;
}

}  // namespace wifi_networks

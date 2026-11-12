#ifndef PROVISIONING_PROVISION_H_
#define PROVISIONING_PROVISION_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// The provisioning file (#42): /.keepsake/provision.json, written by Eric on a computer and
// copied to the SD card. It pre-loads the friend's setup so there are no setup steps; the
// device imports it once, then wipes it. See provision.example.json at the repo root.
//
// Every field is optional. Nothing here is ever logged: DescribeForLog says what's present
// without the values.
namespace provisioning {

inline constexpr size_t kMaxWifiNetworks = 5;

struct WifiNetwork {
    std::string ssid;
    std::string password;  // empty for an open network

    bool operator==(const WifiNetwork& other) const = default;
};

struct Provision {
    std::vector<WifiNetwork> wifi;   // in order of preference
    std::string timezone;            // IANA name, e.g. "Europe/London"
    std::string weather_location;    // a place name for the weather (#37)
    std::string friend_name;         // for the greeting (#85)
    std::string birthday;            // "MM-DD" (the year, if given, is dropped)
    std::string anniversary;         // "MM-DD" or "YYYY-MM-DD" (with the year: "5th anniversary")
    std::string first_date;          // "MM-DD" or "YYYY-MM-DD"
    // Holidays to celebrate, from "holidays": ["christmas", "new_year", "valentines",
    // "halloween"]. All of them when the file doesn't say.
    bool holidays_given = false;
    std::vector<std::string> holidays;
    struct SpecialDay {
        std::string date;     // "MM-DD" or "YYYY-MM-DD"
        std::string title;    // the Home title and card heading for the day
        std::string message;  // the card's line
        bool operator==(const SpecialDay& other) const = default;
    };
    std::vector<SpecialDay> special_days;  // at most 12
    std::string welcome_message;     // shown once on the welcome card (#86)
    std::string gemini_key;          // optional (#49)

    bool operator==(const Provision& other) const = default;
    bool empty() const { return *this == Provision{}; }
};

struct ParseResult {
    bool ok = false;
    // Plain-language reasons the file was refused (nothing is imported then), each naming the
    // field, e.g. "wifi[1].password: must be 8 to 63 characters (or empty for an open network)".
    std::vector<std::string> problems;
    // Things that didn't stop the import, e.g. an unknown field (probably a typo).
    std::vector<std::string> warnings;
};

// Reads and checks the file. On success *out holds the values (trimmed, birthday as "MM-DD");
// on failure *out is left empty and every problem found is listed.
ParseResult ParseProvision(std::string_view json, Provision* out);

// "wifi: 2 networks, timezone, friend name, gemini key" -- which fields are set, never values.
std::string DescribeForLog(const Provision& provision);

}  // namespace provisioning

#endif  // PROVISIONING_PROVISION_H_

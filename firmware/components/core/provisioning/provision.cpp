#include "provisioning/provision.h"

#include "provisioning/mini_json.h"

namespace provisioning {
namespace {

constexpr size_t kMaxSsidBytes = 32;
constexpr size_t kMinPasswordBytes = 8;
constexpr size_t kMaxPasswordBytes = 63;
constexpr size_t kMaxTimezoneBytes = 64;
constexpr size_t kMaxLocationBytes = 64;
constexpr size_t kMaxNameBytes = 32;
constexpr size_t kMaxWelcomeBytes = 400;
constexpr size_t kMaxKeyBytes = 128;

std::string Trim(const std::string& text)
{
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }
    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

bool IsDigits(std::string_view text)
{
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return !text.empty();
}

class Checker {
public:
    explicit Checker(ParseResult* result) : result_(result) {}

    void Problem(const std::string& field, const std::string& what)
    {
        result_->problems.push_back(field + ": " + what);
    }

    // A text field: must be a string; returns it trimmed (or "" when absent).
    std::string Text(const mini_json::Value& root, const char* key, size_t max_bytes)
    {
        const mini_json::Value* value = root.Find(key);
        if (value == nullptr || value->type == mini_json::Value::Type::kNull) {
            return {};
        }
        if (value->type != mini_json::Value::Type::kString) {
            Problem(key, "must be text in \"quotes\"");
            return {};
        }
        std::string text = Trim(value->string);
        if (text.size() > max_bytes) {
            Problem(key, "is too long (at most " + std::to_string(max_bytes) + " characters)");
            return {};
        }
        return text;
    }

private:
    ParseResult* result_;
};

void ReadWifi(const mini_json::Value& root, Checker* check, Provision* out)
{
    const mini_json::Value* wifi = root.Find("wifi");
    if (wifi == nullptr || wifi->type == mini_json::Value::Type::kNull) {
        return;
    }
    if (wifi->type != mini_json::Value::Type::kArray) {
        check->Problem("wifi", "must be a list: [ { \"ssid\": ..., \"password\": ... } ]");
        return;
    }
    if (wifi->array.size() > kMaxWifiNetworks) {
        check->Problem("wifi", "at most " + std::to_string(kMaxWifiNetworks) + " networks");
        return;
    }
    for (size_t i = 0; i < wifi->array.size(); ++i) {
        const mini_json::Value& entry = wifi->array[i];
        const std::string field = "wifi[" + std::to_string(i) + "]";
        if (entry.type != mini_json::Value::Type::kObject) {
            check->Problem(field, "must be { \"ssid\": ..., \"password\": ... }");
            continue;
        }
        const mini_json::Value* ssid = entry.Find("ssid");
        const mini_json::Value* password = entry.Find("password");
        WifiNetwork network;
        // SSIDs and passwords are taken exactly as written (spaces can be part of them).
        if (ssid == nullptr || ssid->type != mini_json::Value::Type::kString || ssid->string.empty()) {
            check->Problem(field + ".ssid", "the network name is missing");
            continue;
        }
        if (ssid->string.size() > kMaxSsidBytes) {
            check->Problem(field + ".ssid", "network names are at most 32 characters");
            continue;
        }
        network.ssid = ssid->string;
        if (password != nullptr && password->type != mini_json::Value::Type::kNull) {
            if (password->type != mini_json::Value::Type::kString) {
                check->Problem(field + ".password", "must be text in \"quotes\"");
                continue;
            }
            network.password = password->string;
        }
        const size_t length = network.password.size();
        const bool hex_key = length == 64 && network.password.find_first_not_of(
                                                 "0123456789abcdefABCDEF") == std::string::npos;
        if (length != 0 && !hex_key && (length < kMinPasswordBytes || length > kMaxPasswordBytes)) {
            check->Problem(field + ".password",
                           "must be 8 to 63 characters (or empty for an open network)");
            continue;
        }
        for (const WifiNetwork& earlier : out->wifi) {
            if (earlier.ssid == network.ssid) {
                check->Problem(field + ".ssid", "\"" + network.ssid + "\" is listed twice");
            }
        }
        out->wifi.push_back(std::move(network));
    }
}

std::string ReadBirthday(const mini_json::Value& root, Checker* check)
{
    const std::string text = check->Text(root, "birthday", 10);
    if (text.empty()) {
        return {};
    }
    // "MM-DD" or "YYYY-MM-DD".
    std::string month_day = text;
    if (text.size() == 10 && text[4] == '-' && IsDigits(text.substr(0, 4))) {
        month_day = text.substr(5);
    }
    const bool shaped = month_day.size() == 5 && month_day[2] == '-' &&
                        IsDigits(month_day.substr(0, 2)) && IsDigits(month_day.substr(3, 2));
    if (shaped) {
        static constexpr int kDaysIn[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        const int month = std::stoi(month_day.substr(0, 2));
        const int day = std::stoi(month_day.substr(3, 2));
        if (month >= 1 && month <= 12 && day >= 1 && day <= kDaysIn[month - 1]) {
            return month_day;
        }
    }
    check->Problem("birthday", "write it as \"MM-DD\" or \"YYYY-MM-DD\", e.g. \"07-14\"");
    return {};
}

std::string ReadTimezone(const mini_json::Value& root, Checker* check)
{
    const std::string text = check->Text(root, "timezone", kMaxTimezoneBytes);
    if (text.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789/_+-") !=
        std::string::npos) {
        check->Problem("timezone", "use a name like \"Europe/London\" or \"America/Toronto\"");
        return {};
    }
    return text;
}

std::string ReadGeminiKey(const mini_json::Value& root, Checker* check)
{
    const std::string key = check->Text(root, "gemini_key", kMaxKeyBytes);
    for (const char c : key) {
        if (c <= ' ' || c > '~') {
            check->Problem("gemini_key", "has spaces or unusual characters in it (copy it again?)");
            return {};
        }
    }
    return key;
}

}  // namespace

ParseResult ParseProvision(std::string_view json, Provision* out)
{
    *out = Provision{};
    ParseResult result;
    mini_json::Value root;
    std::string error;
    if (!mini_json::Parse(json, &root, &error)) {
        result.problems.push_back("the file isn't valid JSON (" + error + ")");
        return result;
    }
    if (root.type != mini_json::Value::Type::kObject) {
        result.problems.push_back("the file must be one { ... } object");
        return result;
    }

    static constexpr const char* kKnown[] = {"version",      "wifi",          "timezone",
                                             "weather_location", "friend_name", "birthday",
                                             "welcome_message",  "gemini_key",  "_comment"};
    for (const auto& member : root.object) {
        bool known = false;
        for (const char* name : kKnown) {
            known = known || member.key == name;
        }
        if (!known) {
            result.warnings.push_back("\"" + member.key + "\" isn't a setting Keepsake knows "
                                      "(a typo?); it was ignored");
        }
    }

    Checker check(&result);
    if (const mini_json::Value* version = root.Find("version")) {
        if (version->type != mini_json::Value::Type::kNumber || version->number != 1.0) {
            check.Problem("version", "this Keepsake reads version 1 files");
        }
    }
    Provision parsed;
    ReadWifi(root, &check, &parsed);
    parsed.timezone = ReadTimezone(root, &check);
    parsed.weather_location = check.Text(root, "weather_location", kMaxLocationBytes);
    parsed.friend_name = check.Text(root, "friend_name", kMaxNameBytes);
    parsed.birthday = ReadBirthday(root, &check);
    parsed.welcome_message = check.Text(root, "welcome_message", kMaxWelcomeBytes);
    parsed.gemini_key = ReadGeminiKey(root, &check);

    if (result.problems.empty() && parsed.empty()) {
        result.problems.push_back("there's nothing to set up in it");
    }
    if (!result.problems.empty()) {
        return result;
    }
    result.ok = true;
    *out = std::move(parsed);
    return result;
}

std::string DescribeForLog(const Provision& provision)
{
    std::string out;
    const auto add = [&out](const std::string& part) {
        out += (out.empty() ? "" : ", ") + part;
    };
    if (!provision.wifi.empty()) {
        add("wifi: " + std::to_string(provision.wifi.size()) +
            (provision.wifi.size() == 1 ? " network" : " networks"));
    }
    if (!provision.timezone.empty()) add("timezone");
    if (!provision.weather_location.empty()) add("weather location");
    if (!provision.friend_name.empty()) add("friend name");
    if (!provision.birthday.empty()) add("birthday");
    if (!provision.welcome_message.empty()) add("welcome message");
    if (!provision.gemini_key.empty()) add("gemini key");
    return out.empty() ? "nothing" : out;
}

}  // namespace provisioning

#include "firmware_update/firmware_update.h"

#include <cmath>

#include "provisioning/mini_json.h"

namespace firmware_update {
namespace {

constexpr int kMaxVersionPart = 9999;
constexpr int64_t kClockValidAfter = 1704067200;  // 2024-01-01T00:00:00Z

bool ParseNumber(std::string_view text, size_t* pos, int* out)
{
    const size_t start = *pos;
    int value = 0;
    while (*pos < text.size() && text[*pos] >= '0' && text[*pos] <= '9') {
        value = value * 10 + (text[*pos] - '0');
        if (value > kMaxVersionPart) {
            return false;
        }
        ++*pos;
    }
    const size_t digits = *pos - start;
    if (digits == 0 || (digits > 1 && text[start] == '0')) {
        return false;  // no leading zeros, as in SemVer
    }
    *out = value;
    return true;
}

bool IsPrintableAscii(std::string_view text)
{
    for (const char c : text) {
        if (c < 0x20 || c > 0x7e) {
            return false;
        }
    }
    return true;
}

bool HasPrefix(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

}  // namespace

bool ParseVersion(std::string_view text, Version* out)
{
    if (!text.empty() && (text[0] == 'v' || text[0] == 'V')) {
        text.remove_prefix(1);
    }
    Version version = {};
    size_t pos = 0;
    if (!ParseNumber(text, &pos, &version.major) || pos >= text.size() || text[pos++] != '.' ||
        !ParseNumber(text, &pos, &version.minor) || pos >= text.size() || text[pos++] != '.' ||
        !ParseNumber(text, &pos, &version.patch)) {
        return false;
    }
    if (pos < text.size()) {
        const char separator = text[pos];
        const std::string_view rest = text.substr(pos + 1);
        if ((separator != '-' && separator != '+') || rest.empty()) {
            return false;
        }
        for (const char c : rest) {
            const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                            (c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == '+';
            if (!ok) {
                return false;
            }
        }
        // "1.2.3+build-7" is build metadata only, still a release.
        version.prerelease = separator == '-';
    }
    if (out != nullptr) {
        *out = version;
    }
    return true;
}

int Compare(const Version& a, const Version& b)
{
    if (a.major != b.major) {
        return a.major < b.major ? -1 : 1;
    }
    if (a.minor != b.minor) {
        return a.minor < b.minor ? -1 : 1;
    }
    if (a.patch != b.patch) {
        return a.patch < b.patch ? -1 : 1;
    }
    if (a.prerelease != b.prerelease) {
        return a.prerelease ? -1 : 1;
    }
    return 0;
}

bool IsLowerHex(std::string_view text, size_t digits)
{
    if (text.size() != digits) {
        return false;
    }
    for (const char c : text) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

bool ParseManifest(std::string_view json, Release* out, std::string* error)
{
    auto fail = [error](const char* message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };
    if (json.size() > kMaxManifestBytes) {
        return fail("manifest too big");
    }
    mini_json::Value root;
    std::string parse_error;
    mini_json::Limits limits = {};
    limits.max_bytes = kMaxManifestBytes;
    limits.max_depth = 4;
    if (!mini_json::Parse(json, &root, &parse_error, limits)) {
        if (error != nullptr) {
            *error = "manifest isn't JSON (" + parse_error + ")";
        }
        return false;
    }
    if (root.type != mini_json::Value::Type::kObject) {
        return fail("manifest isn't an object");
    }

    Release release;
    const mini_json::Value* version = root.Find("version");
    if (version == nullptr || version->type != mini_json::Value::Type::kString ||
        !ParseVersion(version->string, &release.version)) {
        return fail("bad or missing \"version\"");
    }
    release.version_text = version->string;
    if (!release.version_text.empty() &&
        (release.version_text[0] == 'v' || release.version_text[0] == 'V')) {
        release.version_text.erase(0, 1);
    }

    const mini_json::Value* url = root.Find("url");
    if (url == nullptr || url->type != mini_json::Value::Type::kString ||
        !HasPrefix(url->string, "https://") || url->string.size() <= 8 ||
        url->string.size() > kMaxUrlBytes || !IsPrintableAscii(url->string) ||
        url->string.find(' ') != std::string::npos) {
        return fail("bad or missing \"url\" (must be https)");
    }
    release.url = url->string;

    const mini_json::Value* size = root.Find("size");
    if (size == nullptr || size->type != mini_json::Value::Type::kNumber ||
        !(size->number >= 1.0) || size->number > 4294967295.0 ||
        std::floor(size->number) != size->number) {
        return fail("bad or missing \"size\"");
    }
    release.size = static_cast<uint32_t>(size->number);

    const mini_json::Value* sha256 = root.Find("sha256");
    if (sha256 == nullptr || sha256->type != mini_json::Value::Type::kString ||
        !IsLowerHex(sha256->string, 64)) {
        return fail("bad or missing \"sha256\" (64 lowercase hex digits)");
    }
    release.sha256 = sha256->string;

    if (const mini_json::Value* notes = root.Find("notes"); notes != nullptr) {
        if (notes->type != mini_json::Value::Type::kString) {
            return fail("\"notes\" must be text");
        }
        if (notes->string.size() > kMaxNotesBytes) {
            return fail("\"notes\" too long");
        }
        for (const char c : notes->string) {
            if (c == '\0' || c == '\n' || c == '\r') {
                return fail("\"notes\" must be one line");
            }
        }
        release.notes = notes->string;
    }

    if (out != nullptr) {
        *out = std::move(release);
    }
    return true;
}

Decision Decide(const Version& running, const Release& release, std::string_view bad_version,
                uint32_t slot_bytes)
{
    if (Compare(release.version, running) <= 0) {
        return Decision::kUpToDate;
    }
    if (release.version.prerelease) {
        return Decision::kPrerelease;
    }
    Version bad = {};
    if (!bad_version.empty() && ParseVersion(bad_version, &bad) &&
        Compare(bad, release.version) == 0) {
        return Decision::kKnownBad;
    }
    if (slot_bytes != 0 && release.size > slot_bytes) {
        return Decision::kTooBig;
    }
    return Decision::kInstall;
}

bool CheckDue(int64_t last_check, int64_t now, int64_t interval_seconds)
{
    if (last_check <= 0) {
        return true;
    }
    if (now < kClockValidAfter) {
        return false;  // checked before, and the clock can't say how long ago
    }
    if (now < last_check) {
        return true;
    }
    return now - last_check >= interval_seconds;
}

bool PowerAllows(bool usb_power, int battery_percent, bool asked_by_person)
{
    if (usb_power || battery_percent < 0) {
        return true;
    }
    return battery_percent >=
           (asked_by_person ? kAskedMinBatteryPercent : kBackgroundMinBatteryPercent);
}

uint8_t RequiredChecks(bool storage_was_ok)
{
    uint8_t required = kHealthHomeReached | kHealthDisplayOk;
    if (storage_was_ok) {
        required |= kHealthStorageOk;
    }
    return required;
}

Verdict Judge(uint8_t passed, uint8_t required, int64_t elapsed_ms, int64_t settle_ms,
              int64_t timeout_ms)
{
    if ((passed & required) == required && elapsed_ms >= settle_ms) {
        return Verdict::kHealthy;
    }
    return elapsed_ms >= timeout_ms ? Verdict::kFailed : Verdict::kWaiting;
}

std::string Describe(const Status& status)
{
    switch (status.phase) {
        case Phase::kChecking:
            return "Checking for updates...";
        case Phase::kUpToDate:
            return "You're up to date.";
        case Phase::kDownloading: {
            const int percent = status.percent < 0 ? 0 : (status.percent > 100 ? 100 : status.percent);
            return "Downloading " + status.version + "... " + std::to_string(percent) + "%";
        }
        case Phase::kReady:
            return "Version " + status.version + " is ready. Restart to finish.";
        case Phase::kRolledBack:
            return "Update " + status.version +
                   " didn't start properly, so Keepsake went back. Nothing was lost.";
        case Phase::kUpdated:
            return "Updated to " + status.version + ".";
        case Phase::kFailed:
            switch (status.problem) {
                case Problem::kNoWifi:
                    return "Couldn't check: no Wi-Fi.";
                case Problem::kLowBattery:
                    return "Plug in to update: the battery is low.";
                case Problem::kServer:
                    return "Couldn't reach the update server. It'll try again later.";
                case Problem::kDownload:
                    return "The download didn't finish. It'll try again later.";
                case Problem::kFlash:
                    return "The update couldn't be saved. It'll try again later.";
                case Problem::kNone:
                default:
                    return "Something went wrong. It'll try again later.";
            }
        case Phase::kIdle:
        default:
            return "Updates install by themselves over Wi-Fi.";
    }
}

std::string_view ButtonLabel(const Status& status)
{
    switch (status.phase) {
        case Phase::kChecking:
            return "Checking...";
        case Phase::kDownloading:
            return "Downloading...";
        case Phase::kReady:
            return "Restart to update";
        default:
            return "Check for updates";
    }
}

}  // namespace firmware_update

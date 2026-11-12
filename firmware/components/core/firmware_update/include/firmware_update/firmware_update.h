#ifndef FIRMWARE_UPDATE_FIRMWARE_UPDATE_H_
#define FIRMWARE_UPDATE_FIRMWARE_UPDATE_H_

#include <cstdint>
#include <string>
#include <string_view>

// Updates over the air from GitHub Releases (#40), the parts that don't touch hardware.
//
// Each release publishes a small manifest next to the image (tools/release/make_manifest.py):
//
//   {"version": "1.22.0", "url": "https://.../keepsake-1.22.0.bin", "size": 1234567,
//    "sha256": "<64 hex digits>", "notes": "Optional: one friendly line about what's new."}
//
// The device fetches it at most once a day (or when asked in Settings -> About), downloads the
// image into the other app slot if it is newer, and restarts into it. The new firmware is only
// kept once it passes a health check (Home reached, display works, the SD card still mounts);
// otherwise the bootloader goes back to the previous one, and that version is never offered
// again.
namespace firmware_update {

struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
    bool prerelease = false;  // "1.2.3-rc1": never installed automatically
};

// "1.22.0", "v1.22.0", "1.22.0-rc1", "1.22.0+build" (build metadata ignored). Each number is
// 0..9999. False for anything else.
bool ParseVersion(std::string_view text, Version* out);
// <0, 0, >0 like strcmp; a prerelease sorts before the same release.
int Compare(const Version& a, const Version& b);

struct Release {
    std::string version_text;
    Version version;
    std::string url;     // https only
    uint32_t size = 0;   // bytes, > 0
    std::string sha256;  // lowercase hex, 64 digits
    std::string notes;   // may be empty; at most kMaxNotesBytes
};

inline constexpr size_t kMaxManifestBytes = 4096;
inline constexpr size_t kMaxNotesBytes = 240;
inline constexpr size_t kMaxUrlBytes = 512;

// Parses and validates the manifest. On failure sets *error to a short reason for the log.
bool ParseManifest(std::string_view json, Release* out, std::string* error);

enum class Decision : uint8_t {
    kUpToDate,      // same or older than what's running
    kInstall,       // newer: download it
    kPrerelease,    // newer but a prerelease
    kKnownBad,      // this version rolled back before
    kTooBig,        // doesn't fit the app slot
};

Decision Decide(const Version& running, const Release& release, std::string_view bad_version,
                uint32_t slot_bytes);

// Whether a background check is due. `last_check` and `now` are Unix seconds (0 = never). A
// clock that isn't set yet (before 2024) only allows the first check; a clock that went back
// counts as due.
bool CheckDue(int64_t last_check, int64_t now, int64_t interval_seconds);

// Whether there's enough power to download and flash. Unknown battery (percent < 0) is
// allowed. A person asking for the update gets a lower bar than the daily background check.
bool PowerAllows(bool usb_power, int battery_percent, bool asked_by_person);
inline constexpr int kBackgroundMinBatteryPercent = 30;
inline constexpr int kAskedMinBatteryPercent = 15;

// --- health check after an update ---------------------------------------------------------

enum HealthCheck : uint8_t {
    kHealthHomeReached = 1u << 0,
    kHealthDisplayOk = 1u << 1,
    kHealthStorageOk = 1u << 2,
};

// What the new firmware has to show. The SD card counts only if it was mounted before the
// update, so taking the card out never makes a good update roll back.
uint8_t RequiredChecks(bool storage_was_ok);

enum class Verdict : uint8_t { kWaiting, kHealthy, kFailed };

// Healthy once every required check has passed and the firmware has kept running for
// `settle_ms` (so a crash just after Home still rolls back); failed if that hasn't happened by
// `timeout_ms`.
Verdict Judge(uint8_t passed, uint8_t required, int64_t elapsed_ms, int64_t settle_ms,
              int64_t timeout_ms);

// --- status, as shown in Settings -> About ---------------------------------------------------

enum class Phase : uint8_t {
    kIdle,          // nothing to say yet
    kChecking,
    kUpToDate,
    kDownloading,
    kReady,         // downloaded; restarts into it
    kFailed,        // the last check or download failed (see `problem`)
    kRolledBack,    // an update didn't start properly; back on the previous version
    kUpdated,       // this boot is a new version that passed the health check
};

enum class Problem : uint8_t {
    kNone,
    kNoWifi,
    kLowBattery,
    kServer,        // couldn't reach or read the release
    kDownload,      // the download broke off or didn't match
    kFlash,         // writing or checking the image failed
};

struct Status {
    Phase phase = Phase::kIdle;
    Problem problem = Problem::kNone;
    std::string version;       // the release in question (kDownloading/kReady/kRolledBack/kUpdated)
    int percent = 0;           // kDownloading
};

// One short sentence, e.g. "Downloading 1.22.0... 40%" or "You're up to date."
std::string Describe(const Status& status);
// The About page's update button: "Check for updates", "Restart to update", or what it's busy
// with ("Checking...", "Downloading...": pressing it then does nothing).
std::string_view ButtonLabel(const Status& status);  // a string literal

bool IsLowerHex(std::string_view text, size_t digits);

}  // namespace firmware_update

#endif  // FIRMWARE_UPDATE_FIRMWARE_UPDATE_H_

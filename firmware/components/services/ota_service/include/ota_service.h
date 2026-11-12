#ifndef OTA_SERVICE_H_
#define OTA_SERVICE_H_

#include <cstdint>
#include <functional>
#include <string>

#include "esp_err.h"
#include "firmware_update/firmware_update.h"

// Updates over the air from GitHub Releases (#40).
//
// - Init() runs early at boot. After an update it starts the health check: the app reports
//   what works with ReportHealth(), and the new firmware is kept once everything required has
//   passed and it has kept running for a while. Otherwise the device restarts into the previous
//   firmware, and that version is never downloaded again.
// - Start() (once the network jobs exist) checks at most once a day as a background job; a
//   person can also ask with CheckNow().
// - A downloaded update waits in the other app slot. It's applied by RestartIntoUpdate(), by
//   any restart, or quietly the next time the device goes to sleep (RestartIfUpdateReady()).
namespace ota_service {

esp_err_t Init();
void Start();

// Called (from any task) whenever GetStatus() changes. Keep it short: schedule a repaint.
void SetStatusListener(std::function<void()> listener);
firmware_update::Status GetStatus();

// firmware_update::HealthCheck bits. Cheap; call them every boot, it only matters after an
// update.
void ReportHealth(uint8_t checks);
bool VerifyingUpdate();

// A person asked (Settings -> About): check now, and download if there's something newer.
void CheckNow();
// Applies a downloaded update now. Does nothing (returns false) if none is ready.
bool RestartIntoUpdate();
// Applies a downloaded update without the start-up sound, for a moment nobody's using the
// device (just before it goes to sleep). Returns false (and does nothing) if none is ready.
bool RestartIfUpdateReady();

// This boot was a quiet restart into an update: skip the start-up sound.
bool QuietBoot();
// Once per update: what to say about it (empty otherwise). `title` like "Keepsake updated",
// `body` the release's notes or a default line.
bool TakeUpdatedNotice(std::string* title, std::string* body);

// For the board test console.
struct Info {
    std::string running_slot;
    std::string slot_state;   // "valid", "pending-verify", ...
    std::string bad_version;  // a version that rolled back, never offered again
    int64_t last_check = 0;   // Unix seconds, 0 = never
    uint8_t health_passed = 0;
};
Info GetInfo();
// Rehearsal (docs/board-tests.md): the next update fails its health check on purpose and
// rolls back.
void FailNextHealthCheck();
// After that rehearsal: offer the version that rolled back again.
void ClearBadVersion();

}  // namespace ota_service

#endif  // OTA_SERVICE_H_

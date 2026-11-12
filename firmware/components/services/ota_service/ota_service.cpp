#include "ota_service.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>

#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "mbedtls/sha256.h"
#include "net_jobs_service.h"
#include "nvs.h"
#include "power_service.h"
#include "sdkconfig.h"

namespace ota_service {
namespace {

using firmware_update::Phase;
using firmware_update::Problem;
using firmware_update::Status;

constexpr const char* kTag = "OtaService";
constexpr const char* kNvsNamespace = "ota";
constexpr const char* kKeyLastCheck = "last_check";   // i64, Unix seconds
constexpr const char* kKeyBadVersion = "bad_ver";     // str: rolled back, never again
constexpr const char* kKeyNewVersion = "new_ver";     // str: the update we restarted into
constexpr const char* kKeyNotes = "notes";            // str: its notes, for the card
constexpr const char* kKeyStorageOk = "storage_ok";   // u8: SD mounted before the update
constexpr const char* kKeyQuiet = "quiet";            // u8: next boot skips the start-up sound
constexpr const char* kKeyFailNext = "fail_next";     // u8: rehearsal, fail the health check
constexpr const char* kKeyNotice = "notice";          // u8: the "updated" card is still to show

constexpr int64_t kSettleMs = 20 * 1000;
constexpr int64_t kHealthTimeoutMs =
    static_cast<int64_t>(CONFIG_KEEPSAKE_OTA_HEALTH_TIMEOUT_SECONDS) * 1000;
constexpr int kMaxRedirects = 5;
constexpr size_t kChunkBytes = 4096;
constexpr int kHttpTimeoutMs = 15000;
constexpr int64_t kNoStartTimeoutUs = 90LL * 1000 * 1000;  // asked, but the job never ran

std::mutex s_mutex;
Status s_status = {};
std::function<void()> s_listener;
std::string s_bad_version;
std::string s_running_version;
int64_t s_last_check = 0;
bool s_quiet_boot = false;
std::atomic<bool> s_verifying{false};
std::atomic<bool> s_notice_pending{false};  // mirrors kKeyNotice, so Home can ask cheaply
bool s_force_fail = false;
uint8_t s_required = 0;
std::atomic<uint8_t> s_passed{0};
int64_t s_verify_started_us = 0;
std::atomic<bool> s_job_queued{false};
std::atomic<bool> s_job_started{false};
esp_timer_handle_t s_health_timer = nullptr;
esp_timer_handle_t s_check_timer = nullptr;
esp_timer_handle_t s_no_start_timer = nullptr;

// --- NVS --------------------------------------------------------------------------------------

std::string ReadString(nvs_handle_t handle, const char* key)
{
    size_t length = 0;
    if (nvs_get_str(handle, key, nullptr, &length) != ESP_OK || length == 0 || length > 512) {
        return {};
    }
    std::string value(length, '\0');
    if (nvs_get_str(handle, key, value.data(), &length) != ESP_OK) {
        return {};
    }
    value.resize(length - 1);
    return value;
}

uint8_t ReadU8(nvs_handle_t handle, const char* key, uint8_t fallback)
{
    uint8_t value = fallback;
    nvs_get_u8(handle, key, &value);
    return value;
}

template <typename Fn>
void WithNvs(Fn&& fn)
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGW(kTag, "can't open NVS");
        return;
    }
    fn(handle);
    nvs_commit(handle);
    nvs_close(handle);
}

// --- status ---------------------------------------------------------------------------------

void SetStatus(const Status& status)
{
    std::function<void()> listener;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_status = status;
        listener = s_listener;
    }
    if (listener) {
        listener();
    }
}

void SetPhase(Phase phase, Problem problem = Problem::kNone, const std::string& version = {},
              int percent = 0)
{
    Status status;
    status.phase = phase;
    status.problem = problem;
    status.version = version;
    status.percent = percent;
    SetStatus(status);
}

// --- health check -----------------------------------------------------------------------------

void MarkHealthy()
{
    const esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    ESP_LOGI(kTag, "update %s passed its health check: kept (%s)", s_running_version.c_str(),
             esp_err_to_name(err));
    WithNvs([](nvs_handle_t handle) {
        nvs_erase_key(handle, kKeyNewVersion);
        nvs_set_u8(handle, kKeyNotice, 1);
    });
    s_notice_pending = true;
    SetPhase(Phase::kUpdated, Problem::kNone, s_running_version);
}

void RollBack(const char* why)
{
    ESP_LOGE(kTag, "update %s failed its health check (%s, passed 0x%02x of 0x%02x): rolling back",
             s_running_version.c_str(), why, s_passed.load(), s_required);
    const std::string version = s_running_version;
    WithNvs([&version](nvs_handle_t handle) {
        nvs_set_str(handle, kKeyBadVersion, version.c_str());
        nvs_set_u8(handle, kKeyQuiet, 1);
    });
    esp_ota_mark_app_invalid_rollback_and_reboot();
    // Only returns if there's nothing to go back to.
    ESP_LOGE(kTag, "no previous firmware to roll back to; keeping this one");
    esp_ota_mark_app_valid_cancel_rollback();
}

void EvaluateHealth()
{
    if (!s_verifying) {
        return;
    }
    const int64_t elapsed_ms = (esp_timer_get_time() - s_verify_started_us) / 1000;
    const uint8_t passed = s_force_fail ? 0 : s_passed.load();
    switch (firmware_update::Judge(passed, s_required, elapsed_ms, kSettleMs, kHealthTimeoutMs)) {
        case firmware_update::Verdict::kHealthy:
            s_verifying = false;
            esp_timer_stop(s_health_timer);
            MarkHealthy();
            break;
        case firmware_update::Verdict::kFailed:
            s_verifying = false;
            esp_timer_stop(s_health_timer);
            RollBack(s_force_fail ? "rehearsal" : "timed out");
            break;
        case firmware_update::Verdict::kWaiting:
        default:
            break;
    }
}

// --- HTTP -------------------------------------------------------------------------------------

struct HttpStream {
    esp_http_client_handle_t client = nullptr;
    int64_t content_length = -1;
    ~HttpStream()
    {
        if (client != nullptr) {
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
        }
    }
};

// Opens `url` for reading, following redirects (GitHub's release links redirect to a CDN).
bool Open(const std::string& url, HttpStream* stream)
{
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_GET;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = kHttpTimeoutMs;
    config.buffer_size = 4096;  // GitHub's redirect headers are long
    config.buffer_size_tx = 2048;
    config.user_agent = "Keepsake";
    config.disable_auto_redirect = true;
    stream->client = esp_http_client_init(&config);
    if (stream->client == nullptr) {
        return false;
    }
    for (int redirects = 0; redirects <= kMaxRedirects; ++redirects) {
        const esp_err_t err = esp_http_client_open(stream->client, 0);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "open failed: %s", esp_err_to_name(err));
            return false;
        }
        stream->content_length = esp_http_client_fetch_headers(stream->client);
        const int status = esp_http_client_get_status_code(stream->client);
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            esp_http_client_flush_response(stream->client, nullptr);
            if (esp_http_client_set_redirection(stream->client) != ESP_OK) {
                return false;
            }
            esp_http_client_close(stream->client);
            continue;
        }
        if (status != 200) {
            ESP_LOGW(kTag, "HTTP %d", status);
            return false;
        }
        return true;
    }
    ESP_LOGW(kTag, "too many redirects");
    return false;
}

bool FetchManifest(std::string* body)
{
    HttpStream stream;
    if (!Open(CONFIG_KEEPSAKE_OTA_MANIFEST_URL, &stream)) {
        return false;
    }
    char buffer[512];
    while (body->size() <= firmware_update::kMaxManifestBytes) {
        const int got = esp_http_client_read(stream.client, buffer, sizeof(buffer));
        if (got < 0) {
            return false;
        }
        if (got == 0) {
            break;
        }
        body->append(buffer, static_cast<size_t>(got));
    }
    return !body->empty() && body->size() <= firmware_update::kMaxManifestBytes;
}

// --- the download -------------------------------------------------------------------------------

constexpr size_t kAppDescOffset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);

// Streaming SHA-256 of what was downloaded, checked against the manifest.
class Sha256 {
public:
    Sha256()
    {
        mbedtls_sha256_init(&context_);
        ok_ = mbedtls_sha256_starts(&context_, 0) == 0;
    }
    ~Sha256() { mbedtls_sha256_free(&context_); }
    void Update(const uint8_t* data, size_t size)
    {
        ok_ = ok_ && mbedtls_sha256_update(&context_, data, size) == 0;
    }
    // Empty if hashing failed.
    std::string HexDigest()
    {
        uint8_t digest[32];
        if (!ok_ || mbedtls_sha256_finish(&context_, digest) != 0) {
            return {};
        }
        char hex[65];
        for (int i = 0; i < 32; ++i) {
            std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
        }
        return std::string(hex, 64);
    }

private:
    mbedtls_sha256_context context_;
    bool ok_ = false;
};

Problem Download(const firmware_update::Release& release)
{
    const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
    if (target == nullptr) {
        return Problem::kFlash;
    }
    HttpStream stream;
    if (!Open(release.url, &stream)) {
        return Problem::kServer;
    }
    if (stream.content_length > 0 && stream.content_length != static_cast<int64_t>(release.size)) {
        ESP_LOGW(kTag, "size %lld, manifest says %lu", static_cast<long long>(stream.content_length),
                 static_cast<unsigned long>(release.size));
        return Problem::kDownload;
    }
    esp_ota_handle_t ota = 0;
    if (esp_ota_begin(target, OTA_WITH_SEQUENTIAL_WRITES, &ota) != ESP_OK) {
        return Problem::kFlash;
    }
    std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[kChunkBytes]);
    if (!buffer) {
        esp_ota_abort(ota);
        return Problem::kFlash;
    }
    Sha256 sha;
    const esp_app_desc_t* running = esp_app_get_description();
    uint32_t received = 0;
    int last_percent = -1;
    bool header_checked = false;
    Problem problem = Problem::kNone;
    while (received < release.size) {
        const size_t want = std::min<size_t>(kChunkBytes, release.size - received);
        const int got = esp_http_client_read(stream.client, reinterpret_cast<char*>(buffer.get()),
                                             static_cast<int>(want));
        if (got <= 0) {
            problem = Problem::kDownload;
            break;
        }
        // The first chunk holds the app description: make sure it's ours and the right
        // version before writing anything else.
        if (!header_checked && received == 0) {
            if (static_cast<size_t>(got) < kAppDescOffset + sizeof(esp_app_desc_t)) {
                problem = Problem::kDownload;
                break;
            }
            esp_app_desc_t incoming = {};
            std::memcpy(&incoming, buffer.get() + kAppDescOffset, sizeof(incoming));
            if (incoming.magic_word != ESP_APP_DESC_MAGIC_WORD ||
                std::strncmp(incoming.project_name, running->project_name,
                             sizeof(incoming.project_name)) != 0) {
                ESP_LOGE(kTag, "not a Keepsake image");
                problem = Problem::kDownload;
                break;
            }
            firmware_update::Version version = {};
            if (!firmware_update::ParseVersion(incoming.version, &version) ||
                firmware_update::Compare(version, release.version) != 0) {
                ESP_LOGE(kTag, "image says %.32s, manifest says %s", incoming.version,
                         release.version_text.c_str());
                problem = Problem::kDownload;
                break;
            }
            header_checked = true;
        }
        sha.Update(buffer.get(), static_cast<size_t>(got));
        if (esp_ota_write(ota, buffer.get(), static_cast<size_t>(got)) != ESP_OK) {
            problem = Problem::kFlash;
            break;
        }
        received += static_cast<uint32_t>(got);
        const int percent = static_cast<int>((static_cast<uint64_t>(received) * 100) / release.size);
        if (percent / 10 != last_percent / 10) {
            last_percent = percent;
            SetPhase(Phase::kDownloading, Problem::kNone, release.version_text, percent);
        }
    }
    if (problem == Problem::kNone && sha.HexDigest() != release.sha256) {
        ESP_LOGE(kTag, "SHA-256 doesn't match the manifest");
        problem = Problem::kDownload;
    }
    if (problem != Problem::kNone) {
        esp_ota_abort(ota);
        return problem;
    }
    if (esp_ota_end(ota) != ESP_OK) {  // checks the image
        return Problem::kFlash;
    }
    if (esp_ota_set_boot_partition(target) != ESP_OK) {
        return Problem::kFlash;
    }
    ESP_LOGI(kTag, "update %s written to %s; it starts on the next restart",
             release.version_text.c_str(), target->label);
    return Problem::kNone;
}

bool OnUsbPower(int* battery_percent)
{
    *battery_percent = -1;
    power_service::Status status = {};
    if (power_service::ReadStatus(&status) != ESP_OK) {
        return false;
    }
    if (status.battery.available) {
        *battery_percent = status.battery.state_of_charge_percent;
    }
    // power_input_valid is always true on this board (it means "the sample is valid");
    // usb_detected is VBUS itself.
    return status.usb_detected;
}

// The job itself, on the network jobs task.
bool CheckJob(bool asked)
{
    s_job_started = true;
    s_job_queued = false;
    if (s_verifying) {
        return true;  // the other slot holds the firmware to go back to: leave it alone
    }
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_status.phase == Phase::kReady) {
            return true;  // already downloaded, waiting for a restart
        }
    }
    SetPhase(Phase::kChecking);

    std::string body;
    const bool fetched = FetchManifest(&body);
    const int64_t now = static_cast<int64_t>(std::time(nullptr));
    s_last_check = now > 0 ? now : 1;
    WithNvs([](nvs_handle_t handle) { nvs_set_i64(handle, kKeyLastCheck, s_last_check); });
    if (!fetched) {
        SetPhase(Phase::kFailed, Problem::kServer);
        return true;
    }
    firmware_update::Release release;
    std::string error;
    if (!firmware_update::ParseManifest(body, &release, &error)) {
        ESP_LOGW(kTag, "manifest refused: %s", error.c_str());
        SetPhase(Phase::kFailed, Problem::kServer);
        return true;
    }
    firmware_update::Version running = {};
    firmware_update::ParseVersion(s_running_version, &running);
    const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
    const auto decision = firmware_update::Decide(running, release, s_bad_version,
                                                  target != nullptr ? target->size : 0);
    ESP_LOGI(kTag, "latest release %s, running %s: decision %d", release.version_text.c_str(),
             s_running_version.c_str(), static_cast<int>(decision));
    if (decision != firmware_update::Decision::kInstall) {
        SetPhase(Phase::kUpToDate);
        return true;
    }
    int battery_percent = -1;
    const bool usb = OnUsbPower(&battery_percent);
    if (!firmware_update::PowerAllows(usb, battery_percent, asked)) {
        ESP_LOGI(kTag, "update %s waits for more battery (%d%%)", release.version_text.c_str(),
                 battery_percent);
        SetPhase(Phase::kFailed, Problem::kLowBattery);
        return true;
    }

    SetPhase(Phase::kDownloading, Problem::kNone, release.version_text, 0);
    const Problem problem = Download(release);
    if (problem != Problem::kNone) {
        SetPhase(Phase::kFailed, problem);
        return true;
    }
    // Any restart from now on starts the update, so note now what its health check needs.
    const uint8_t storage_ok = (s_passed.load() & firmware_update::kHealthStorageOk) != 0 ? 1 : 0;
    WithNvs([&release, storage_ok](nvs_handle_t handle) {
        nvs_set_str(handle, kKeyNewVersion, release.version_text.c_str());
        nvs_set_str(handle, kKeyNotes, release.notes.c_str());
        nvs_set_u8(handle, kKeyStorageOk, storage_ok);
    });
    SetPhase(Phase::kReady, Problem::kNone, release.version_text);
    return true;
}

void SubmitCheck(bool asked)
{
    if (s_job_queued.exchange(true) && !asked) {
        return;  // one is already waiting
    }
    net_jobs_service::Submit(asked ? net_jobs::Urgency::kUser : net_jobs::Urgency::kBackground,
                             "update check", [asked] { return CheckJob(asked); });
}

void OnCheckTimer(void*)
{
    constexpr int64_t kInterval =
        static_cast<int64_t>(CONFIG_KEEPSAKE_OTA_CHECK_INTERVAL_HOURS) * 3600;
    if (s_verifying || s_job_queued) {
        return;
    }
    if (firmware_update::CheckDue(s_last_check, static_cast<int64_t>(std::time(nullptr)),
                                  kInterval)) {
        SubmitCheck(false);
    }
}

void OnNoStartTimer(void*)
{
    if (!s_job_started) {
        Status status = GetStatus();
        if (status.phase == Phase::kChecking) {
            SetPhase(Phase::kFailed, Problem::kNoWifi);
        }
    }
}

void PrepareRestart(bool quiet)
{
    // The new firmware's health check only expects the SD card if it works now, so a card
    // taken out never makes a good update roll back.
    const uint8_t storage_ok = (s_passed.load() & firmware_update::kHealthStorageOk) != 0 ? 1 : 0;
    WithNvs([quiet, storage_ok](nvs_handle_t handle) {
        nvs_set_u8(handle, kKeyStorageOk, storage_ok);
        if (quiet) {
            nvs_set_u8(handle, kKeyQuiet, 1);
        }
    });
}

const char* StateName(esp_ota_img_states_t state)
{
    switch (state) {
        case ESP_OTA_IMG_NEW: return "new";
        case ESP_OTA_IMG_PENDING_VERIFY: return "pending-verify";
        case ESP_OTA_IMG_VALID: return "valid";
        case ESP_OTA_IMG_INVALID: return "invalid";
        case ESP_OTA_IMG_ABORTED: return "aborted";
        case ESP_OTA_IMG_UNDEFINED:
        default: return "undefined";
    }
}

}  // namespace

esp_err_t Init()
{
    s_running_version = esp_app_get_description()->version;
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    const bool have_state =
        running != nullptr && esp_ota_get_state_partition(running, &state) == ESP_OK;

    std::string new_version;
    bool fail_next = false;
    uint8_t storage_ok = 1;
    WithNvs([&](nvs_handle_t handle) {
        nvs_get_i64(handle, kKeyLastCheck, &s_last_check);
        s_bad_version = ReadString(handle, kKeyBadVersion);
        new_version = ReadString(handle, kKeyNewVersion);
        storage_ok = ReadU8(handle, kKeyStorageOk, 1);
        s_quiet_boot = ReadU8(handle, kKeyQuiet, 0) != 0;
        fail_next = ReadU8(handle, kKeyFailNext, 0) != 0;
        s_notice_pending = ReadU8(handle, kKeyNotice, 0) != 0;
        nvs_erase_key(handle, kKeyQuiet);
    });

    if (have_state && state == ESP_OTA_IMG_PENDING_VERIFY) {
        // First boot of an update: it has to prove itself.
        s_verifying = true;
        s_required = firmware_update::RequiredChecks(storage_ok != 0);
        s_verify_started_us = esp_timer_get_time();
        if (fail_next) {
            s_force_fail = true;
            WithNvs([](nvs_handle_t handle) { nvs_erase_key(handle, kKeyFailNext); });
            ESP_LOGW(kTag, "rehearsal: this update will fail its health check on purpose");
        }
        ESP_LOGI(kTag, "verifying update %s (needs 0x%02x)", s_running_version.c_str(),
                 s_required);
        const esp_timer_create_args_t args = {
            .callback = [](void*) { EvaluateHealth(); },
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "ota_health",
            .skip_unhandled_events = true,
        };
        if (esp_timer_create(&args, &s_health_timer) == ESP_OK) {
            esp_timer_start_periodic(s_health_timer, 1000 * 1000);
        }
    } else if (!new_version.empty()) {
        firmware_update::Version wanted = {};
        firmware_update::Version now = {};
        const bool comparable = firmware_update::ParseVersion(new_version, &wanted) &&
                                firmware_update::ParseVersion(s_running_version, &now);
        if (!comparable || firmware_update::Compare(wanted, now) != 0) {
            // We restarted into an update and came back here: it rolled back (failed health
            // check, or a crash before it passed). Never offer that version again.
            ESP_LOGW(kTag, "update %s rolled back; staying on %s", new_version.c_str(),
                     s_running_version.c_str());
            s_bad_version = new_version;
            WithNvs([&new_version](nvs_handle_t handle) {
                nvs_set_str(handle, kKeyBadVersion, new_version.c_str());
                nvs_erase_key(handle, kKeyNewVersion);
                nvs_erase_key(handle, kKeyNotes);
            });
            SetPhase(Phase::kRolledBack, Problem::kNone, new_version);
        } else {
            // Running the update, already valid (rollback disabled, or verified before a
            // reset): nothing to check.
            WithNvs([](nvs_handle_t handle) { nvs_erase_key(handle, kKeyNewVersion); });
        }
    }
    ESP_LOGI(kTag, "running %s from %s (%s)", s_running_version.c_str(),
             running != nullptr ? running->label : "?", have_state ? StateName(state) : "no state");
    return ESP_OK;
}

void Start()
{
#if CONFIG_KEEPSAKE_OTA_ENABLED
    const esp_timer_create_args_t args = {
        .callback = &OnCheckTimer,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ota_check",
        .skip_unhandled_events = true,
    };
    if (esp_timer_create(&args, &s_check_timer) == ESP_OK) {
        // Looks every hour whether a day has passed; the first look is a few minutes after
        // boot, when start-up work is done.
        esp_timer_start_periodic(s_check_timer, 3600LL * 1000 * 1000);
    }
    const esp_timer_create_args_t first = {
        .callback = &OnCheckTimer,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ota_first",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t first_timer = nullptr;
    if (esp_timer_create(&first, &first_timer) == ESP_OK) {
        esp_timer_start_once(first_timer, 3LL * 60 * 1000 * 1000);
    }
#endif
}

void SetStatusListener(std::function<void()> listener)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_listener = std::move(listener);
}

Status GetStatus()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_status;
}

void ReportHealth(uint8_t checks)
{
    s_passed.fetch_or(checks);
}

bool VerifyingUpdate()
{
    return s_verifying;
}

void CheckNow()
{
#if CONFIG_KEEPSAKE_OTA_ENABLED
    if (s_verifying) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_status.phase == Phase::kChecking || s_status.phase == Phase::kDownloading ||
            s_status.phase == Phase::kReady) {
            return;
        }
    }
    SetPhase(Phase::kChecking);
    s_job_started = false;
    SubmitCheck(true);
    if (s_no_start_timer == nullptr) {
        const esp_timer_create_args_t args = {
            .callback = &OnNoStartTimer,
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "ota_nostart",
            .skip_unhandled_events = true,
        };
        esp_timer_create(&args, &s_no_start_timer);
    }
    if (s_no_start_timer != nullptr) {
        esp_timer_stop(s_no_start_timer);
        esp_timer_start_once(s_no_start_timer, kNoStartTimeoutUs);
    }
#endif
}

bool RestartIntoUpdate()
{
    if (GetStatus().phase != Phase::kReady) {
        return false;
    }
    ESP_LOGI(kTag, "restarting into the update");
    PrepareRestart(false);
    esp_restart();
    return true;
}

bool RestartIfUpdateReady()
{
    if (GetStatus().phase != Phase::kReady) {
        return false;
    }
    ESP_LOGI(kTag, "nobody's using it: restarting quietly into the update");
    PrepareRestart(true);
    esp_restart();
    return true;
}

bool QuietBoot()
{
    return s_quiet_boot;
}

bool TakeUpdatedNotice(std::string* title, std::string* body)
{
    if (!s_notice_pending.exchange(false)) {
        return false;
    }
    bool show = false;
    std::string notes;
    WithNvs([&](nvs_handle_t handle) {
        show = ReadU8(handle, kKeyNotice, 0) != 0;
        if (show) {
            notes = ReadString(handle, kKeyNotes);
            nvs_erase_key(handle, kKeyNotice);
            nvs_erase_key(handle, kKeyNotes);
        }
    });
    if (!show) {
        return false;
    }
    if (title != nullptr) {
        *title = "Keepsake updated";
    }
    if (body != nullptr) {
        *body = notes.empty() ? "Version " + s_running_version + " is on now." : notes;
    }
    return true;
}

Info GetInfo()
{
    Info info;
    const esp_partition_t* running = esp_ota_get_running_partition();
    info.running_slot = running != nullptr ? running->label : "?";
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    info.slot_state = running != nullptr && esp_ota_get_state_partition(running, &state) == ESP_OK
                          ? StateName(state)
                          : "undefined";
    info.bad_version = s_bad_version;
    info.last_check = s_last_check;
    info.health_passed = s_passed.load();
    return info;
}

void FailNextHealthCheck()
{
    WithNvs([](nvs_handle_t handle) { nvs_set_u8(handle, kKeyFailNext, 1); });
}

void ClearBadVersion()
{
    s_bad_version.clear();
    WithNvs([](nvs_handle_t handle) { nvs_erase_key(handle, kKeyBadVersion); });
    SetPhase(Phase::kIdle);
}

}  // namespace ota_service

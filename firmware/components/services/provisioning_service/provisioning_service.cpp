#include "provisioning_service.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "app_state_service.h"
#include "esp_log.h"
#include "gemini_service.h"
#include "nvs.h"
#include "provisioning/provision.h"
#include "storage_service.h"
#include "timezone_service.h"
#include "wifi_service.h"

namespace provisioning_service {
namespace {

constexpr const char* kTag = "Provisioning";
constexpr const char* kFolder = ".keepsake";
constexpr const char* kFileName = "provision.json";
constexpr const char* kProblemsFileName = "provision-problems.txt";
constexpr size_t kMaxFileBytes = 64u * 1024u;
// Networks after the first, kept for saved-networks support (#43) to pick up.
constexpr const char* kExtraNetworksNamespace = "provision";

std::string Join(const std::string& left, const char* right)
{
    return left.empty() || left.back() == '/' ? left + right : left + "/" + right;
}

std::string FolderPath()
{
    return Join(storage_service::MountPoint(), kFolder);
}

bool ReadFile(const std::string& path, std::string* out, bool* too_big)
{
    FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    out->clear();
    char buffer[512];
    size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (out->size() + read > kMaxFileBytes) {
            *too_big = true;
            break;
        }
        out->append(buffer, read);
    }
    std::fclose(file);
    return true;
}

// Overwrites the file's bytes before deleting it, so the passwords aren't left in free space
// on the card (as far as FAT allows: it rewrites the same clusters).
bool WipeAndDelete(const std::string& path, size_t length)
{
    FILE* file = std::fopen(path.c_str(), "r+b");
    if (file != nullptr) {
        static const char kZeros[256] = {};
        size_t left = length;
        while (left > 0) {
            const size_t chunk = left < sizeof(kZeros) ? left : sizeof(kZeros);
            if (std::fwrite(kZeros, 1, chunk, file) != chunk) {
                break;
            }
            left -= chunk;
        }
        std::fflush(file);
        fsync(fileno(file));
        std::fclose(file);
    }
    if (unlink(path.c_str()) != 0) {
        ESP_LOGE(kTag, "Couldn't delete the provisioning file: errno=%d", errno);
        return false;
    }
    return true;
}

void WriteProblems(const std::string& path, const provisioning::ParseResult& result)
{
    FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        return;
    }
    std::fputs("Keepsake couldn't use provision.json, so nothing was imported.\n"
               "Fix these and restart Keepsake (provision.example.json shows the format):\n\n",
               file);
    for (const std::string& problem : result.problems) {
        std::fprintf(file, "- %s\n", problem.c_str());
    }
    if (!result.warnings.empty()) {
        std::fputs("\nAlso noticed (these don't stop the import):\n", file);
        for (const std::string& warning : result.warnings) {
            std::fprintf(file, "- %s\n", warning.c_str());
        }
    }
    std::fclose(file);
}

void SaveExtraNetworks(const provisioning::Provision& provision)
{
    nvs_handle_t handle = 0;
    if (nvs_open(kExtraNetworksNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    (void)nvs_erase_all(handle);
    const size_t extras = provision.wifi.size() > 1 ? provision.wifi.size() - 1 : 0;
    nvs_set_u8(handle, "net_count", static_cast<uint8_t>(extras));
    for (size_t i = 0; i < extras; ++i) {
        const provisioning::WifiNetwork& network = provision.wifi[i + 1];
        const std::string n = std::to_string(i);
        nvs_set_str(handle, ("ssid" + n).c_str(), network.ssid.c_str());
        nvs_set_str(handle, ("pass" + n).c_str(), network.password.c_str());
    }
    nvs_commit(handle);
    nvs_close(handle);
}

// Applies each part; a part that fails is logged and reported, the rest still go in.
bool Apply(const provisioning::Provision& provision)
{
    bool ok = true;
    if (!provision.wifi.empty()) {
        const provisioning::WifiNetwork& first = provision.wifi.front();
        if (!wifi_service::SaveNetwork(first.ssid, first.password)) {
            ESP_LOGE(kTag, "Saving the Wi-Fi network failed");
            ok = false;
        }
        SaveExtraNetworks(provision);
    }
    if (!provision.timezone.empty()) {
        timezone_service::SettingsPatch patch;
        patch.has_timezone_name = true;
        patch.timezone_name = provision.timezone;
        patch.has_enabled = true;
        patch.enabled = true;
        const timezone_service::Result result = timezone_service::ApplySettingsPatch(patch);
        if (!result.success) {
            ESP_LOGE(kTag, "Timezone not applied: %s", result.message.c_str());
            ok = false;
        }
    }
    if (!provision.gemini_key.empty()) {
        gemini_service::SettingsPatch patch;
        patch.has_api_key = true;
        patch.api_key = provision.gemini_key;
        const gemini_service::Result result = gemini_service::ApplySettingsPatch(patch);
        if (!result.success) {
            ESP_LOGE(kTag, "Gemini key not applied: %s", result.message.c_str());
            ok = false;
        }
    }
    app_state_service::PersonalSettings personal;
    personal.friend_name = provision.friend_name;
    personal.birthday = provision.birthday;
    personal.welcome_message = provision.welcome_message;
    personal.weather_location = provision.weather_location;
    if (!app_state_service::SavePersonal(personal)) {
        ok = false;
    } else if (!provision.welcome_message.empty()) {
        app_state_service::MarkWelcomeNoteShown(false);  // a new note: show it once (#85)
    }
    return ok;
}

}  // namespace

Outcome ImportFromSdCard()
{
    Outcome outcome;
    if (!storage_service::IsMounted()) {
        return outcome;
    }
    const std::string folder = FolderPath();
    const std::string path = Join(folder, kFileName);
    const std::string problems_path = Join(folder, kProblemsFileName);

    std::string text;
    bool too_big = false;
    if (!ReadFile(path, &text, &too_big)) {
        return outcome;  // no file: the usual case
    }
    ESP_LOGI(kTag, "Found a provisioning file (%u bytes)", static_cast<unsigned>(text.size()));

    provisioning::Provision provision;
    provisioning::ParseResult result;
    if (too_big) {
        result.problems.push_back("the file is too big (at most 64 KB)");
    } else {
        result = provisioning::ParseProvision(text, &provision);
    }
    for (const std::string& warning : result.warnings) {
        ESP_LOGW(kTag, "%s", warning.c_str());
    }
    if (!result.ok) {
        for (const std::string& problem : result.problems) {
            ESP_LOGW(kTag, "Refused: %s", problem.c_str());
        }
        WriteProblems(problems_path, result);
        outcome.status = Status::kRefused;
        outcome.message = "The setup file has a problem";
        return outcome;
    }

    ESP_LOGI(kTag, "Importing: %s", provisioning::DescribeForLog(provision).c_str());
    const bool applied = Apply(provision);
    const bool wiped = WipeAndDelete(path, text.size());
    (void)unlink(problems_path.c_str());  // an old one, from an earlier attempt
    if (!applied || !wiped) {
        outcome.status = Status::kError;
        outcome.message = applied ? "Set up, but couldn't remove the setup file"
                                  : "Setup was only partly applied";
        return outcome;
    }
    outcome.status = Status::kImported;
    outcome.message = "All set up!";
    return outcome;
}

}  // namespace provisioning_service

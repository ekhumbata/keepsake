#include "app_state_service.h"

#include "esp_log.h"
#include "nvs.h"

namespace app_state_service {
namespace {

constexpr const char* kTag = "AppStateService";
constexpr const char* kNvsNamespace = "app_state";
constexpr const char* kOnboardingKey = "onboarded";
constexpr const char* kReaderTextSizeKey = "text_size";
constexpr uint8_t kDefaultReaderTextSize = 1;
constexpr const char* kFriendNameKey = "friend_name";
constexpr const char* kBirthdayKey = "birthday";
constexpr const char* kWelcomeKey = "welcome";
constexpr const char* kWeatherLocationKey = "weather_loc";
constexpr size_t kMaxPersonalBytes = 512;

std::string ReadString(nvs_handle_t handle, const char* key)
{
    size_t length = 0;
    if (nvs_get_str(handle, key, nullptr, &length) != ESP_OK || length == 0 ||
        length > kMaxPersonalBytes) {
        return {};
    }
    std::string value(length, '\0');
    if (nvs_get_str(handle, key, value.data(), &length) != ESP_OK) {
        return {};
    }
    value.resize(length > 0 ? length - 1 : 0);  // drop the terminating NUL
    return value;
}

}  // namespace

bool OnboardingViewed()
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    uint8_t viewed = 0;
    const esp_err_t err = nvs_get_u8(handle, kOnboardingKey, &viewed);
    nvs_close(handle);
    return err == ESP_OK && viewed != 0;
}

void MarkOnboardingViewed()
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGW(kTag, "Onboarding flag: nvs_open failed");
        return;
    }
    if (nvs_set_u8(handle, kOnboardingKey, 1) == ESP_OK) {
        (void)nvs_commit(handle);
    }
    nvs_close(handle);
}

uint8_t ReaderTextSize()
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return kDefaultReaderTextSize;
    }
    uint8_t size = kDefaultReaderTextSize;
    if (nvs_get_u8(handle, kReaderTextSizeKey, &size) != ESP_OK || size > 2) {
        size = kDefaultReaderTextSize;
    }
    nvs_close(handle);
    return size;
}

void SetReaderTextSize(uint8_t size)
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGW(kTag, "Reader text size: nvs_open failed");
        return;
    }
    if (nvs_set_u8(handle, kReaderTextSizeKey, size) == ESP_OK) {
        (void)nvs_commit(handle);
    }
    nvs_close(handle);
}

PersonalSettings Personal()
{
    PersonalSettings settings;
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return settings;
    }
    settings.friend_name = ReadString(handle, kFriendNameKey);
    settings.birthday = ReadString(handle, kBirthdayKey);
    settings.welcome_message = ReadString(handle, kWelcomeKey);
    settings.weather_location = ReadString(handle, kWeatherLocationKey);
    nvs_close(handle);
    return settings;
}

bool SavePersonal(const PersonalSettings& settings)
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGW(kTag, "Personal settings: nvs_open failed");
        return false;
    }
    esp_err_t err = ESP_OK;
    const auto save = [&](const char* key, const std::string& value) {
        if (err == ESP_OK && !value.empty()) {
            err = nvs_set_str(handle, key, value.c_str());
        }
    };
    save(kFriendNameKey, settings.friend_name);
    save(kBirthdayKey, settings.birthday);
    save(kWelcomeKey, settings.welcome_message);
    save(kWeatherLocationKey, settings.weather_location);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Personal settings: save failed (%s)", esp_err_to_name(err));
    }
    return err == ESP_OK;
}

}  // namespace app_state_service

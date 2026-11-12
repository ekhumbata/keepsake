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
constexpr const char* kNoteShownKey = "note_shown";
constexpr const char* kAnniversaryKey = "anniversary";
constexpr const char* kFirstDateKey = "first_date";
constexpr const char* kHolidaysKey = "holidays";
constexpr const char* kSpecialDaysKey = "special_days";
constexpr const char* kDayCardKey = "day_card";
constexpr size_t kMaxPersonalBytes = 3000;

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
    settings.anniversary = ReadString(handle, kAnniversaryKey);
    settings.first_date = ReadString(handle, kFirstDateKey);
    settings.special_days = ReadString(handle, kSpecialDaysKey);
    uint8_t holidays = 0;
    if (nvs_get_u8(handle, kHolidaysKey, &holidays) == ESP_OK) {
        settings.holidays = holidays;
    }
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
    save(kAnniversaryKey, settings.anniversary);
    save(kFirstDateKey, settings.first_date);
    save(kSpecialDaysKey, settings.special_days);
    if (err == ESP_OK && settings.holidays >= 0) {
        err = nvs_set_u8(handle, kHolidaysKey, static_cast<uint8_t>(settings.holidays));
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Personal settings: save failed (%s)", esp_err_to_name(err));
    }
    return err == ESP_OK;
}

bool WelcomeNoteShown()
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    uint8_t shown = 0;
    const esp_err_t err = nvs_get_u8(handle, kNoteShownKey, &shown);
    nvs_close(handle);
    return err == ESP_OK && shown != 0;
}

void MarkWelcomeNoteShown(bool shown)
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    if (nvs_set_u8(handle, kNoteShownKey, shown ? 1 : 0) == ESP_OK) {
        (void)nvs_commit(handle);
    }
    nvs_close(handle);
}

std::string LastDayCard()
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return {};
    }
    std::string value = ReadString(handle, kDayCardKey);
    nvs_close(handle);
    return value;
}

void SetLastDayCard(const std::string& value)
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    if (nvs_set_str(handle, kDayCardKey, value.c_str()) == ESP_OK) {
        (void)nvs_commit(handle);
    }
    nvs_close(handle);
}

}  // namespace app_state_service

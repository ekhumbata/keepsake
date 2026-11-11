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

}  // namespace app_state_service

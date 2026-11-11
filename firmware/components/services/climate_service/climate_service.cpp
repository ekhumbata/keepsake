#include "climate_service.h"

#include <mutex>

#include "esp_log.h"
#include "esp_timer.h"
#include "shtc3.h"
#include "waveshare_board.h"

namespace climate_service {
namespace {

constexpr const char* kTag = "ClimateService";
constexpr int64_t kMinReadIntervalUs = kMinReadIntervalSeconds * 1000 * 1000;

std::mutex s_mutex;
i2c_master_dev_handle_t s_device = nullptr;
bool s_have_reading = false;
Reading s_last = {};

esp_err_t EnsureDeviceLocked()
{
    if (s_device != nullptr) {
        return ESP_OK;
    }
    i2c_master_bus_handle_t bus = nullptr;
    esp_err_t err = waveshare_board::EnsureSensorI2cBus(&bus);
    if (err != ESP_OK) {
        return err;
    }
    i2c_master_dev_handle_t device = nullptr;
    err = waveshare_board::AddShtc3Device(bus, &device);
    if (err != ESP_OK) {
        return err;
    }
    uint16_t id = 0;
    err = shtc3::Probe(device, &id);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "SHTC3 probe failed: %s (id=0x%04x)", esp_err_to_name(err), id);
        i2c_master_bus_rm_device(device);
        return err;
    }
    ESP_LOGI(kTag, "SHTC3 found (id=0x%04x)", id);
    s_device = device;
    return ESP_OK;
}

}  // namespace

esp_err_t Read(Reading* out)
{
    if (out == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    std::lock_guard<std::mutex> lock(s_mutex);
    const int64_t now = esp_timer_get_time();
    if (s_have_reading && now - s_last.taken_at_us < kMinReadIntervalUs) {
        *out = s_last;
        return ESP_OK;
    }
    esp_err_t err = EnsureDeviceLocked();
    if (err != ESP_OK) {
        return err;
    }
    shtc3::Measurement measurement;
    err = shtc3::Measure(s_device, &measurement);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "SHTC3 measurement failed: %s", esp_err_to_name(err));
        return err;
    }
    s_last.temperature_c = measurement.temperature_c;
    s_last.relative_humidity = measurement.relative_humidity;
    s_last.taken_at_us = now;
    s_have_reading = true;
    *out = s_last;
    return ESP_OK;
}

bool LastReading(Reading* out)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_have_reading || out == nullptr) {
        return false;
    }
    *out = s_last;
    return true;
}

}  // namespace climate_service

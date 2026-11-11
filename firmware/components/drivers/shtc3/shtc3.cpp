#include "shtc3.h"

#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace shtc3 {
namespace {

// Commands from the SHTC3 datasheet (16-bit, MSB first).
constexpr uint16_t kCommandWakeup = 0x3517;
constexpr uint16_t kCommandSleep = 0xB098;
constexpr uint16_t kCommandReadId = 0xEFC8;
// Normal mode, clock stretching disabled, temperature first.
constexpr uint16_t kCommandMeasure = 0x7866;

constexpr int kTimeoutMs = 50;
constexpr uint32_t kWakeupDelayUs = 300;     // tSTART is 240 us max
constexpr uint32_t kMeasureDelayMs = 13;     // normal mode is 12.1 ms max
constexpr int kNotReadyRetries = 3;

// The datasheet's checksum example, and the conversion end points.
static_assert(Crc8(0xBE, 0xEF) == 0x92, "SHTC3 CRC-8 must match the datasheet example");
static_assert(TemperatureFromRaw(0) == -45.0f, "SHTC3 temperature formula");
static_assert(HumidityFromRaw(0) == 0.0f, "SHTC3 humidity formula");

esp_err_t SendCommand(i2c_master_dev_handle_t device, uint16_t command)
{
    const uint8_t bytes[2] = {static_cast<uint8_t>(command >> 8), static_cast<uint8_t>(command & 0xFF)};
    return i2c_master_transmit(device, bytes, sizeof(bytes), kTimeoutMs);
}

esp_err_t Wakeup(i2c_master_dev_handle_t device)
{
    const esp_err_t err = SendCommand(device, kCommandWakeup);
    if (err == ESP_OK) {
        esp_rom_delay_us(kWakeupDelayUs);
    }
    return err;
}

// Checks the CRC of a word at `bytes` (msb, lsb, crc) and returns it.
bool ReadWord(const uint8_t* bytes, uint16_t* out)
{
    if (Crc8(bytes[0], bytes[1]) != bytes[2]) {
        return false;
    }
    *out = static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
    return true;
}

}  // namespace

esp_err_t Sleep(i2c_master_dev_handle_t device)
{
    if (device == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return SendCommand(device, kCommandSleep);
}

esp_err_t Probe(i2c_master_dev_handle_t device, uint16_t* out_id)
{
    if (device == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = Wakeup(device);
    if (err != ESP_OK) {
        return err;
    }
    uint8_t bytes[3] = {};
    err = SendCommand(device, kCommandReadId);
    if (err == ESP_OK) {
        err = i2c_master_receive(device, bytes, sizeof(bytes), kTimeoutMs);
    }
    (void)Sleep(device);
    if (err != ESP_OK) {
        return err;
    }
    uint16_t id = 0;
    if (!ReadWord(bytes, &id)) {
        return ESP_ERR_INVALID_CRC;
    }
    if (out_id != nullptr) {
        *out_id = id;
    }
    // SHTC3 ID: bit 11 set and bits 5..0 = 0b000111.
    return ((id & 0x0800) != 0 && (id & 0x003F) == 0x0007) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t Measure(i2c_master_dev_handle_t device, Measurement* out)
{
    if (device == nullptr || out == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = Wakeup(device);
    if (err != ESP_OK) {
        return err;
    }
    err = SendCommand(device, kCommandMeasure);
    uint8_t bytes[6] = {};
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(kMeasureDelayMs) + 1);
        // Without clock stretching the sensor NACKs its address until the result is ready.
        for (int attempt = 0; attempt <= kNotReadyRetries; ++attempt) {
            err = i2c_master_receive(device, bytes, sizeof(bytes), kTimeoutMs);
            if (err == ESP_OK) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(5) + 1);
        }
    }
    (void)Sleep(device);
    if (err != ESP_OK) {
        return err;
    }
    uint16_t raw_temperature = 0;
    uint16_t raw_humidity = 0;
    if (!ReadWord(bytes, &raw_temperature) || !ReadWord(bytes + 3, &raw_humidity)) {
        return ESP_ERR_INVALID_CRC;
    }
    out->temperature_c = TemperatureFromRaw(raw_temperature);
    out->relative_humidity = HumidityFromRaw(raw_humidity);
    return ESP_OK;
}

}  // namespace shtc3

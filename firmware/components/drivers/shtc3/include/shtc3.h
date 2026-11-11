#ifndef SHTC3_H_
#define SHTC3_H_

#include <cstddef>
#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"

// Sensirion SHTC3 temperature/humidity sensor (I2C address 0x70). Board-agnostic: takes a
// device handle the caller added to its bus. Every call leaves the sensor in sleep mode
// (< 1 uA), so it draws nothing between reads.
namespace shtc3 {

constexpr uint8_t kI2cAddress = 0x70;

struct Measurement {
    float temperature_c = 0.0f;
    float relative_humidity = 0.0f;  // percent, 0..100
};

// CRC-8 of a 16-bit word as the sensor sends it (polynomial 0x31, init 0xFF).
constexpr uint8_t Crc8(uint8_t msb, uint8_t lsb)
{
    uint8_t crc = 0xFF;
    const uint8_t bytes[2] = {msb, lsb};
    for (uint8_t byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = static_cast<uint8_t>((crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1);
        }
    }
    return crc;
}

constexpr float TemperatureFromRaw(uint16_t raw)
{
    return -45.0f + 175.0f * static_cast<float>(raw) / 65536.0f;
}

constexpr float HumidityFromRaw(uint16_t raw)
{
    return 100.0f * static_cast<float>(raw) / 65536.0f;
}

// Wakes the sensor, reads its ID register and puts it back to sleep. ESP_ERR_NOT_FOUND if
// the ID doesn't look like an SHTC3.
esp_err_t Probe(i2c_master_dev_handle_t device, uint16_t* out_id = nullptr);

// Wakes the sensor, takes one normal-mode measurement (about 12 ms) and puts it back to
// sleep. ESP_ERR_INVALID_CRC if the data fails its checksum.
esp_err_t Measure(i2c_master_dev_handle_t device, Measurement* out);

// Puts the sensor to sleep (it also sleeps after Probe/Measure).
esp_err_t Sleep(i2c_master_dev_handle_t device);

}  // namespace shtc3

#endif  // SHTC3_H_

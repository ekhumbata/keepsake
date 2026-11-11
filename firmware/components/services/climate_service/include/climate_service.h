#ifndef CLIMATE_SERVICE_H_
#define CLIMATE_SERVICE_H_

#include <cstdint>

#include "esp_err.h"

// Indoor temperature and humidity from the board's SHTC3, for the ambient screen (#38).
// Initializes itself on first use; the sensor sleeps between reads, and a reading is reused
// for kMinReadIntervalSeconds so a screen refresh never costs an extra measurement.
namespace climate_service {

constexpr int64_t kMinReadIntervalSeconds = 60;

struct Reading {
    float temperature_c = 0.0f;
    float relative_humidity = 0.0f;  // percent
    int64_t taken_at_us = 0;         // esp_timer_get_time() when measured
};

// Fills *out with a reading no older than kMinReadIntervalSeconds (measuring if needed).
// On a failed measurement, returns the error and leaves *out untouched; a previous good
// reading is still available through LastReading().
esp_err_t Read(Reading* out);

// The last good reading, if there is one (false before the first success).
bool LastReading(Reading* out);

}  // namespace climate_service

#endif  // CLIMATE_SERVICE_H_

// libFuzzer target: Open-Meteo answers and the weather cache (#37), which arrive over the network
// or from NVS.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include "weather/weather.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    weather::Place place;
    std::string error;
    if (weather::ParseGeocode(text, &place, &error)) {
        if (std::fabs(place.latitude) > 90 || std::fabs(place.longitude) > 180) {
            std::abort();
        }
        (void)weather::ForecastUrl(place);
    } else if (error.empty()) {
        std::abort();
    }
    weather::Report report;
    if (weather::ParseForecast(text, &report, &error)) {
        report.fetched_at = 1800000000;
        weather::Report back;
        if (!weather::Deserialize(weather::Serialize(report), &back) || back.code != report.code) {
            std::abort();
        }
        (void)weather::Summary(report, 1800000000, true);
    }
    (void)weather::Deserialize(text, &report);
    (void)weather::ParseCoordinates(text, &place);
    (void)weather::GeocodeUrl(text);
    return 0;
}

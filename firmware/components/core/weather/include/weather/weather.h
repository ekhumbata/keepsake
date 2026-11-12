#ifndef WEATHER_WEATHER_H_
#define WEATHER_WEATHER_H_

#include <cstdint>
#include <string>
#include <string_view>

// Weather at a glance, with no API key (#37): Open-Meteo's free geocoding and forecast APIs.
// The place comes from the provisioning file ("weather_location": a town name, or
// "51.5,-0.13"). This part builds the URLs and reads the answers; weather_service fetches them
// hourly as a background network job and keeps the last good one in NVS.
namespace weather {

struct Place {
    double latitude = 0.0;
    double longitude = 0.0;
    std::string name;  // as Open-Meteo calls it ("London"), or the coordinates as given
};

// "51.5072, -0.1276" -> coordinates (no lookup needed). False for anything else.
bool ParseCoordinates(std::string_view text, Place* out);

std::string GeocodeUrl(std::string_view place_name);
std::string ForecastUrl(const Place& place);

// The first result of a geocoding search. False if there's none or it's malformed.
bool ParseGeocode(std::string_view json, Place* out, std::string* error);

enum class Kind : uint8_t { kClear, kPartlyCloudy, kCloudy, kFog, kDrizzle, kRain, kSnow, kStorm };

struct Report {
    double temperature_c = 0.0;  // now
    double high_c = 0.0;         // today
    double low_c = 0.0;
    int code = 0;                // WMO weather code
    bool is_day = true;
    int64_t fetched_at = 0;      // Unix seconds
};

bool ParseForecast(std::string_view json, Report* out, std::string* error);

Kind KindOf(int wmo_code);
// "Sunny" (or "Clear" at night), "Partly cloudy", "Light rain", ...
std::string Describe(int wmo_code, bool is_day);
// "12°" (rounded), in °C or °F.
std::string Temperature(double celsius, bool fahrenheit);

// How old a report may get before it's no longer shown at all.
inline constexpr int64_t kHideAfterSeconds = 6 * 3600;
// How often to fetch.
inline constexpr int64_t kRefreshSeconds = 3600;

// Whether a report is recent enough to show at all (under kHideAfterSeconds old).
// AgeText: "" while fresh (under an hour), then "updated 2 h ago".
std::string AgeText(int64_t fetched_at, int64_t now);
bool Showable(int64_t fetched_at, int64_t now);
bool FetchDue(int64_t fetched_at, int64_t now);

// One line for Home: "12°, partly cloudy" (+ " · updated 2 h ago" when old). Empty when there's
// nothing worth showing.
std::string Summary(const Report& report, int64_t now, bool fahrenheit);

// The NVS cache: a short text form of a report (and the place it's for).
std::string Serialize(const Report& report);
bool Deserialize(std::string_view text, Report* out);

}  // namespace weather

#endif  // WEATHER_WEATHER_H_

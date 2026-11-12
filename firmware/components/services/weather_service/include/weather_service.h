#ifndef WEATHER_SERVICE_H_
#define WEATHER_SERVICE_H_

#include <functional>
#include <string>

#include "esp_err.h"
#include "weather/weather.h"

// Weather at a glance (#37). Once an hour (when the radio is up anyway, or as its own short
// background job with Wi-Fi on demand) it asks Open-Meteo for the place the provisioning file
// named, keeps the last good report in NVS, and says when it changed. Offline, the last report
// is shown with its age, and hidden once it's ~6 h old (weather::Showable).
namespace weather_service {

// The place to fetch for: a town name or "lat, lon" (app_state_service's weather_location).
using LocationProvider = std::function<std::string()>;

esp_err_t Init();  // loads the cached report
void SetLocationProvider(LocationProvider provider);
// Called (from the network task) after a new report arrives. Keep it short.
void SetListener(std::function<void()> listener);
void Start();      // the hourly check

// The last good report, however old (check weather::Showable). False if there's none.
bool Latest(weather::Report* out);
bool Fahrenheit();
// The line for Home ("8C, light rain"), or "" when there's nothing worth showing.
std::string SummaryNow();

}  // namespace weather_service

#endif  // WEATHER_SERVICE_H_

#include "weather_service.h"

#include <atomic>
#include <ctime>
#include <mutex>
#include <string>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "net_jobs_service.h"
#include "nvs.h"
#include "sdkconfig.h"

namespace weather_service {
namespace {

constexpr const char* kTag = "WeatherService";
constexpr const char* kNvsNamespace = "weather";
constexpr const char* kKeyReport = "report";
constexpr const char* kKeyPlaceQuery = "place_q";   // the location text the place was found for
constexpr const char* kKeyPlace = "place";          // "lat;lon"
constexpr size_t kMaxAnswerBytes = 16 * 1024;
constexpr int kHttpTimeoutMs = 10000;
constexpr int64_t kCheckEveryUs = 10LL * 60 * 1000 * 1000;  // looks every 10 min; fetches hourly
constexpr int64_t kClockValidAfter = 1704067200;              // 2024-01-01

std::mutex s_mutex;
weather::Report s_report = {};
bool s_have_report = false;
LocationProvider s_location;
std::function<void()> s_listener;
std::atomic<bool> s_job_queued{false};
esp_timer_handle_t s_timer = nullptr;

std::string ReadString(nvs_handle_t handle, const char* key)
{
    size_t length = 0;
    if (nvs_get_str(handle, key, nullptr, &length) != ESP_OK || length == 0 || length > 256) {
        return {};
    }
    std::string value(length, '\0');
    if (nvs_get_str(handle, key, value.data(), &length) != ESP_OK) {
        return {};
    }
    value.resize(length - 1);
    return value;
}

bool HttpGet(const std::string& url, std::string* body)
{
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_GET;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = kHttpTimeoutMs;
    config.user_agent = "Keepsake";
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        return false;
    }
    bool ok = false;
    if (esp_http_client_open(client, 0) == ESP_OK) {
        esp_http_client_fetch_headers(client);
        const int status = esp_http_client_get_status_code(client);
        if (status == 200) {
            char buffer[512];
            ok = true;
            while (true) {
                const int got = esp_http_client_read(client, buffer, sizeof(buffer));
                if (got < 0) {
                    ok = false;
                    break;
                }
                if (got == 0) {
                    break;
                }
                body->append(buffer, static_cast<size_t>(got));
                if (body->size() > kMaxAnswerBytes) {
                    ok = false;
                    break;
                }
            }
        } else {
            ESP_LOGW(kTag, "HTTP %d", status);
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ok && !body->empty();
}

// The place for `query`: coordinates as given, the cached lookup, or a fresh one.
// Returns false (and *permanent) when the name can't be found at all.
bool ResolvePlace(const std::string& query, weather::Place* place, bool* permanent)
{
    *permanent = false;
    if (weather::ParseCoordinates(query, place)) {
        return true;
    }
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) == ESP_OK) {
        const std::string cached_query = ReadString(handle, kKeyPlaceQuery);
        const std::string cached = ReadString(handle, kKeyPlace);
        nvs_close(handle);
        weather::Place found;
        std::string as_coordinates = cached;
        for (char& c : as_coordinates) {
            if (c == ';') c = ',';
        }
        if (cached_query == query && weather::ParseCoordinates(as_coordinates, &found)) {
            *place = found;
            return true;
        }
    }
    std::string body;
    if (!HttpGet(weather::GeocodeUrl(query), &body)) {
        return false;
    }
    std::string error;
    if (!weather::ParseGeocode(body, place, &error)) {
        ESP_LOGW(kTag, "can't find \"%s\": %s", query.c_str(), error.c_str());
        *permanent = true;
        return false;
    }
    char coordinates[48];
    std::snprintf(coordinates, sizeof(coordinates), "%.4f;%.4f", place->latitude, place->longitude);
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, kKeyPlaceQuery, query.c_str());
        nvs_set_str(handle, kKeyPlace, coordinates);
        nvs_commit(handle);
        nvs_close(handle);
    }
    ESP_LOGI(kTag, "weather place: %s (%s)", place->name.c_str(), coordinates);
    return true;
}

// The network job. True when done (or nothing to do); false to retry on a later connection.
bool FetchJob()
{
    s_job_queued = false;
    std::string query;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_location) {
            query = s_location();
        }
    }
    if (query.empty()) {
        return true;
    }
    weather::Place place;
    bool permanent = false;
    if (!ResolvePlace(query, &place, &permanent)) {
        return permanent;  // a name that doesn't exist won't start existing on a retry
    }
    std::string body;
    if (!HttpGet(weather::ForecastUrl(place), &body)) {
        ESP_LOGW(kTag, "forecast fetch failed; trying again on the next connection");
        return false;
    }
    weather::Report report;
    std::string error;
    if (!weather::ParseForecast(body, &report, &error)) {
        ESP_LOGW(kTag, "forecast refused: %s", error.c_str());
        return true;
    }
    report.fetched_at = static_cast<int64_t>(std::time(nullptr));
    std::function<void()> listener;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_report = report;
        s_have_report = true;
        listener = s_listener;
    }
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, kKeyReport, weather::Serialize(report).c_str());
        nvs_commit(handle);
        nvs_close(handle);
    }
    ESP_LOGI(kTag, "weather: %.1f C, code %d", report.temperature_c, report.code);
    if (listener) {
        listener();
    }
    return true;
}

void OnTimer(void*)
{
    const int64_t now = static_cast<int64_t>(std::time(nullptr));
    int64_t fetched_at = 0;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        fetched_at = s_have_report ? s_report.fetched_at : 0;
    }
    // Without a set clock a report can't be dated; wait for the time sync.
    if (now < kClockValidAfter || !weather::FetchDue(fetched_at, now)) {
        return;
    }
    if (!s_job_queued.exchange(true)) {
        net_jobs_service::Submit(net_jobs::Urgency::kBackground, "weather", FetchJob);
    }
}

}  // namespace

esp_err_t Init()
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return ESP_OK;  // nothing cached yet
    }
    const std::string cached = ReadString(handle, kKeyReport);
    nvs_close(handle);
    weather::Report report;
    if (weather::Deserialize(cached, &report)) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_report = report;
        s_have_report = true;
    }
    return ESP_OK;
}

void SetLocationProvider(LocationProvider provider)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_location = std::move(provider);
}

void SetListener(std::function<void()> listener)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_listener = std::move(listener);
}

void Start()
{
#if CONFIG_KEEPSAKE_WEATHER_ENABLED
    const esp_timer_create_args_t args = {
        .callback = &OnTimer,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "weather",
        .skip_unhandled_events = true,
    };
    if (esp_timer_create(&args, &s_timer) == ESP_OK) {
        esp_timer_start_periodic(s_timer, kCheckEveryUs);
    }
#endif
}

bool Latest(weather::Report* out)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_have_report) {
        return false;
    }
    if (out != nullptr) {
        *out = s_report;
    }
    return true;
}

bool Fahrenheit()
{
#if CONFIG_KEEPSAKE_WEATHER_FAHRENHEIT
    return true;
#else
    return false;
#endif
}

std::string SummaryNow()
{
    weather::Report report;
    if (!Latest(&report)) {
        return {};
    }
    return weather::Summary(report, static_cast<int64_t>(std::time(nullptr)), Fahrenheit());
}

}  // namespace weather_service

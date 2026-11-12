#include "weather/weather.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "provisioning/mini_json.h"

namespace weather {
namespace {

using mini_json::Value;

constexpr const char* kGeocodeBase = "https://geocoding-api.open-meteo.com/v1/search?count=1&language=en&format=json&name=";
constexpr const char* kForecastBase = "https://api.open-meteo.com/v1/forecast";

bool Fail(std::string* error, const char* message)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool Number(const Value* value, double* out)
{
    if (value == nullptr || value->type != Value::Type::kNumber || !std::isfinite(value->number)) {
        return false;
    }
    *out = value->number;
    return true;
}

// The first element of an array member, as a number.
bool FirstNumber(const Value* object, const char* key, double* out)
{
    const Value* array = object != nullptr ? object->Find(key) : nullptr;
    if (array == nullptr || array->type != Value::Type::kArray || array->array.empty()) {
        return false;
    }
    return Number(&array->array.front(), out);
}

bool ValidCoordinates(double latitude, double longitude)
{
    return latitude >= -90.0 && latitude <= 90.0 && longitude >= -180.0 && longitude <= 180.0;
}

bool ParseLeadingDouble(std::string_view text, size_t* pos, double* out)
{
    while (*pos < text.size() && text[*pos] == ' ') {
        ++*pos;
    }
    const size_t start = *pos;
    if (*pos < text.size() && (text[*pos] == '-' || text[*pos] == '+')) {
        ++*pos;
    }
    bool digits = false;
    bool dot = false;
    while (*pos < text.size()) {
        const char c = text[*pos];
        if (c >= '0' && c <= '9') {
            digits = true;
        } else if (c == '.' && !dot) {
            dot = true;
        } else {
            break;
        }
        ++*pos;
    }
    if (!digits) {
        return false;
    }
    *out = std::strtod(std::string(text.substr(start, *pos - start)).c_str(), nullptr);
    while (*pos < text.size() && text[*pos] == ' ') {
        ++*pos;
    }
    return true;
}

std::string UrlEncode(std::string_view text)
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    for (const char c : text) {
        const auto byte = static_cast<unsigned char>(c);
        const bool plain = (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
                           (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' ||
                           byte == '.' || byte == '~';
        if (plain) {
            out.push_back(c);
        } else {
            out.push_back('%');
            out.push_back(kHex[byte >> 4]);
            out.push_back(kHex[byte & 0x0F]);
        }
    }
    return out;
}

}  // namespace

bool ParseCoordinates(std::string_view text, Place* out)
{
    size_t pos = 0;
    double latitude = 0.0;
    double longitude = 0.0;
    if (!ParseLeadingDouble(text, &pos, &latitude) || pos >= text.size() || text[pos] != ',') {
        return false;
    }
    ++pos;
    if (!ParseLeadingDouble(text, &pos, &longitude) || pos != text.size() ||
        !ValidCoordinates(latitude, longitude)) {
        return false;
    }
    if (out != nullptr) {
        out->latitude = latitude;
        out->longitude = longitude;
        out->name = std::string(text);
    }
    return true;
}

std::string GeocodeUrl(std::string_view place_name)
{
    return std::string(kGeocodeBase) + UrlEncode(place_name);
}

std::string ForecastUrl(const Place& place)
{
    char buffer[320];
    std::snprintf(buffer, sizeof(buffer),
                  "%s?latitude=%.4f&longitude=%.4f"
                  "&current=temperature_2m,weather_code,is_day"
                  "&daily=weather_code,temperature_2m_max,temperature_2m_min"
                  "&timezone=auto&forecast_days=1",
                  kForecastBase, place.latitude, place.longitude);
    return buffer;
}

bool ParseGeocode(std::string_view json, Place* out, std::string* error)
{
    Value root;
    std::string parse_error;
    if (!mini_json::Parse(json, &root, &parse_error) || root.type != Value::Type::kObject) {
        return Fail(error, "geocoding answer isn't JSON");
    }
    const Value* results = root.Find("results");
    if (results == nullptr || results->type != Value::Type::kArray || results->array.empty()) {
        return Fail(error, "place not found");
    }
    const Value& first = results->array.front();
    Place place;
    if (first.type != Value::Type::kObject || !Number(first.Find("latitude"), &place.latitude) ||
        !Number(first.Find("longitude"), &place.longitude) ||
        !ValidCoordinates(place.latitude, place.longitude)) {
        return Fail(error, "geocoding result without coordinates");
    }
    if (const Value* name = first.Find("name"); name != nullptr && name->type == Value::Type::kString) {
        place.name = name->string.substr(0, 64);
    }
    if (out != nullptr) {
        *out = std::move(place);
    }
    return true;
}

bool ParseForecast(std::string_view json, Report* out, std::string* error)
{
    Value root;
    std::string parse_error;
    if (!mini_json::Parse(json, &root, &parse_error) || root.type != Value::Type::kObject) {
        return Fail(error, "forecast isn't JSON");
    }
    const Value* current = root.Find("current");
    const Value* daily = root.Find("daily");
    if (current == nullptr || current->type != Value::Type::kObject || daily == nullptr ||
        daily->type != Value::Type::kObject) {
        return Fail(error, "forecast without current/daily");
    }
    Report report;
    double code = 0.0;
    double is_day = 1.0;
    if (!Number(current->Find("temperature_2m"), &report.temperature_c) ||
        !Number(current->Find("weather_code"), &code)) {
        return Fail(error, "forecast without temperature or weather code");
    }
    Number(current->Find("is_day"), &is_day);  // optional
    if (!FirstNumber(daily, "temperature_2m_max", &report.high_c) ||
        !FirstNumber(daily, "temperature_2m_min", &report.low_c)) {
        return Fail(error, "forecast without today's high/low");
    }
    if (std::fabs(report.temperature_c) > 100.0 || std::fabs(report.high_c) > 100.0 ||
        std::fabs(report.low_c) > 100.0 || code < 0.0 || code > 99.0) {
        return Fail(error, "forecast values out of range");
    }
    report.code = static_cast<int>(code);
    report.is_day = is_day != 0.0;
    if (out != nullptr) {
        *out = report;
    }
    return true;
}

Kind KindOf(int code)
{
    if (code <= 1) return Kind::kClear;
    if (code == 2) return Kind::kPartlyCloudy;
    if (code == 3) return Kind::kCloudy;
    if (code == 45 || code == 48) return Kind::kFog;
    if (code >= 51 && code <= 57) return Kind::kDrizzle;
    if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return Kind::kRain;
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return Kind::kSnow;
    if (code >= 95) return Kind::kStorm;
    return Kind::kCloudy;
}

std::string Describe(int code, bool is_day)
{
    switch (code) {
        case 0: return is_day ? "Sunny" : "Clear";
        case 1: return is_day ? "Mostly sunny" : "Mostly clear";
        case 2: return "Partly cloudy";
        case 3: return "Cloudy";
        case 45: case 48: return "Foggy";
        case 51: case 53: case 55: return "Drizzle";
        case 56: case 57: return "Freezing drizzle";
        case 61: return "Light rain";
        case 63: return "Rain";
        case 65: return "Heavy rain";
        case 66: case 67: return "Freezing rain";
        case 71: return "Light snow";
        case 73: return "Snow";
        case 75: return "Heavy snow";
        case 77: return "Snow grains";
        case 80: case 81: return "Showers";
        case 82: return "Heavy showers";
        case 85: case 86: return "Snow showers";
        case 95: return "Thunderstorm";
        case 96: case 99: return "Thunderstorm with hail";
        default: return "";
    }
}

std::string Temperature(double celsius, bool fahrenheit)
{
    const double value = fahrenheit ? celsius * 9.0 / 5.0 + 32.0 : celsius;
    long rounded = std::lround(value);
    if (rounded == 0) {
        rounded = 0;  // no "-0"
    }
    return std::to_string(rounded) + (fahrenheit ? "\xC2\xB0" "F" : "\xC2\xB0" "C");
}

bool Showable(int64_t fetched_at, int64_t now)
{
    if (fetched_at <= 0) {
        return false;
    }
    const int64_t age = now - fetched_at;
    return age >= -600 && age < kHideAfterSeconds;  // a little clock skew is fine
}

std::string AgeText(int64_t fetched_at, int64_t now)
{
    if (!Showable(fetched_at, now)) {
        return "";
    }
    const int64_t hours = (now - fetched_at) / 3600;
    if (hours < 1) {
        return "";
    }
    return "updated " + std::to_string(hours) + " h ago";
}

bool FetchDue(int64_t fetched_at, int64_t now)
{
    return fetched_at <= 0 || now < fetched_at || now - fetched_at >= kRefreshSeconds;
}

std::string Summary(const Report& report, int64_t now, bool fahrenheit)
{
    if (!Showable(report.fetched_at, now)) {
        return "";
    }
    std::string text = Temperature(report.temperature_c, fahrenheit);
    const std::string words = Describe(report.code, report.is_day);
    if (!words.empty()) {
        std::string lower = words;
        lower[0] = static_cast<char>(lower[0] - 'A' + 'a');
        text += ", " + lower;
    }
    const std::string age = AgeText(report.fetched_at, now);
    if (!age.empty()) {
        text += " (" + age + ")";
    }
    return text;
}

std::string Serialize(const Report& report)
{
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "1;%.1f;%.1f;%.1f;%d;%d;%lld", report.temperature_c,
                  report.high_c, report.low_c, report.code, report.is_day ? 1 : 0,
                  static_cast<long long>(report.fetched_at));
    return buffer;
}

bool Deserialize(std::string_view text, Report* out)
{
    if (text.size() > 120) {
        return false;
    }
    const std::string copy(text);
    Report report;
    int is_day = 1;
    long long fetched = 0;
    int consumed = 0;
    if (std::sscanf(copy.c_str(), "1;%lf;%lf;%lf;%d;%d;%lld%n", &report.temperature_c,
                    &report.high_c, &report.low_c, &report.code, &is_day, &fetched,
                    &consumed) != 6 ||
        consumed != static_cast<int>(copy.size()) || !std::isfinite(report.temperature_c) ||
        !std::isfinite(report.high_c) || !std::isfinite(report.low_c) || report.code < 0 ||
        report.code > 99 || fetched < 0) {
        return false;
    }
    report.is_day = is_day != 0;
    report.fetched_at = fetched;
    if (out != nullptr) {
        *out = report;
    }
    return true;
}

}  // namespace weather

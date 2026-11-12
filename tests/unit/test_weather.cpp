// Weather (#37): Open-Meteo URLs and answers (canned JSON), weather codes in words, the cache,
// and when to show or hide a report.
#include <doctest/doctest.h>

#include <string>

#include "weather/weather.h"

using namespace weather;

namespace {

constexpr int64_t kNow = 1800000000;

const char* kForecast = R"({"latitude":51.5,"longitude":-0.12,"generationtime_ms":0.05,
  "utc_offset_seconds":0,"timezone":"Europe/London",
  "current_units":{"time":"iso8601","interval":"seconds","temperature_2m":"°C","weather_code":"wmo code","is_day":""},
  "current":{"time":"2027-01-15T08:00","interval":900,"temperature_2m":7.6,"weather_code":61,"is_day":1},
  "daily_units":{"time":"iso8601","weather_code":"wmo code","temperature_2m_max":"°C","temperature_2m_min":"°C"},
  "daily":{"time":["2027-01-15"],"weather_code":[63],"temperature_2m_max":[9.4],"temperature_2m_min":[3.2]}})";

}  // namespace

TEST_CASE("coordinates can be given directly")
{
    Place place;
    CHECK(ParseCoordinates("51.5072, -0.1276", &place));
    CHECK(place.latitude == doctest::Approx(51.5072));
    CHECK(place.longitude == doctest::Approx(-0.1276));
    CHECK(ParseCoordinates("-33.87,151.21", &place));
    for (const char* bad : {"London", "51.5", "51.5,", ",3", "91,0", "0,181", "1,2,3", "1, 2x", ""}) {
        CAPTURE(bad);
        CHECK_FALSE(ParseCoordinates(bad, &place));
    }
}

TEST_CASE("URLs")
{
    CHECK(GeocodeUrl("São Paulo") ==
          "https://geocoding-api.open-meteo.com/v1/search?count=1&language=en&format=json&name=S%C3%A3o%20Paulo");
    Place place;
    place.latitude = 51.50853;
    place.longitude = -0.12574;
    const std::string url = ForecastUrl(place);
    CHECK(url.find("https://api.open-meteo.com/v1/forecast?latitude=51.5085&longitude=-0.1257") == 0);
    CHECK(url.find("current=temperature_2m,weather_code,is_day") != std::string::npos);
    CHECK(url.find("timezone=auto") != std::string::npos);
}

TEST_CASE("geocoding answers")
{
    Place place;
    std::string error;
    REQUIRE(ParseGeocode(R"({"results":[{"id":2643743,"name":"London","latitude":51.50853,
        "longitude":-0.12574,"country":"United Kingdom"}],"generationtime_ms":0.6})",
                         &place, &error));
    CHECK(place.name == "London");
    CHECK(place.latitude == doctest::Approx(51.50853));
    CHECK_FALSE(ParseGeocode(R"({"generationtime_ms":0.6})", &place, &error));
    CHECK(error == "place not found");
    CHECK_FALSE(ParseGeocode(R"({"results":[{"name":"Nowhere"}]})", &place, &error));
    CHECK_FALSE(ParseGeocode("<html>", &place, &error));
}

TEST_CASE("forecast answers")
{
    Report report;
    std::string error;
    REQUIRE(ParseForecast(kForecast, &report, &error));
    CHECK(report.temperature_c == doctest::Approx(7.6));
    CHECK(report.high_c == doctest::Approx(9.4));
    CHECK(report.low_c == doctest::Approx(3.2));
    CHECK(report.code == 61);
    CHECK(report.is_day);

    for (const char* bad : {"{}", R"({"current":{},"daily":{}})",
                            R"({"current":{"temperature_2m":5,"weather_code":2},"daily":{"temperature_2m_max":[],"temperature_2m_min":[1]}})",
                            R"({"current":{"temperature_2m":500,"weather_code":2},"daily":{"temperature_2m_max":[1],"temperature_2m_min":[1]}})",
                            R"({"current":{"temperature_2m":5,"weather_code":"x"},"daily":{"temperature_2m_max":[1],"temperature_2m_min":[1]}})",
                            "{\"error\":true,\"reason\":\"Latitude must be in range\"}"}) {
        CAPTURE(bad);
        CHECK_FALSE(ParseForecast(bad, &report, &error));
        CHECK_FALSE(error.empty());
    }
}

TEST_CASE("weather codes in words")
{
    CHECK(Describe(0, true) == "Sunny");
    CHECK(Describe(0, false) == "Clear");
    CHECK(Describe(61, true) == "Light rain");
    CHECK(Describe(95, true) == "Thunderstorm");
    CHECK(Describe(42, true).empty());
    CHECK(KindOf(0) == Kind::kClear);
    CHECK(KindOf(2) == Kind::kPartlyCloudy);
    CHECK(KindOf(48) == Kind::kFog);
    CHECK(KindOf(81) == Kind::kRain);
    CHECK(KindOf(86) == Kind::kSnow);
    CHECK(KindOf(99) == Kind::kStorm);
}

TEST_CASE("temperatures")
{
    CHECK(Temperature(7.6, false) == "8\xC2\xB0" "C");
    CHECK(Temperature(-0.3, false) == "0\xC2\xB0" "C");
    CHECK(Temperature(-4.6, false) == "-5\xC2\xB0" "C");
    CHECK(Temperature(20.0, true) == "68\xC2\xB0" "F");
}

TEST_CASE("fresh, old, and too old to show")
{
    CHECK(FetchDue(0, kNow));
    CHECK_FALSE(FetchDue(kNow - 1800, kNow));
    CHECK(FetchDue(kNow - 3600, kNow));
    CHECK(FetchDue(kNow + 100, kNow));  // clock went back

    CHECK_FALSE(Showable(0, kNow));
    CHECK(Showable(kNow - 60, kNow));
    CHECK(Showable(kNow - 5 * 3600, kNow));
    CHECK_FALSE(Showable(kNow - 6 * 3600, kNow));
    CHECK(AgeText(kNow - 1800, kNow).empty());
    CHECK(AgeText(kNow - 2 * 3600 - 5, kNow) == "updated 2 h ago");

    Report report;
    REQUIRE(ParseForecast(kForecast, &report, nullptr));
    report.fetched_at = kNow - 600;
    CHECK(Summary(report, kNow, false) == "8\xC2\xB0" "C, light rain");
    report.fetched_at = kNow - 3 * 3600;
    CHECK(Summary(report, kNow, false) == "8\xC2\xB0" "C, light rain (updated 3 h ago)");
    report.fetched_at = kNow - 7 * 3600;
    CHECK(Summary(report, kNow, false).empty());
}

TEST_CASE("the cache round-trips")
{
    Report report;
    REQUIRE(ParseForecast(kForecast, &report, nullptr));
    report.fetched_at = kNow;
    Report back;
    REQUIRE(Deserialize(Serialize(report), &back));
    CHECK(back.temperature_c == doctest::Approx(7.6));
    CHECK(back.code == 61);
    CHECK(back.fetched_at == kNow);
    for (const char* bad : {"", "2;1;2;3;4;1;5", "1;1;2;3;400;1;5", "1;1;2;3;4;1;5;extra", "1;x;2;3;4;1;5"}) {
        CAPTURE(bad);
        CHECK_FALSE(Deserialize(bad, &back));
    }
}

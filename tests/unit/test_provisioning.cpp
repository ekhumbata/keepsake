// The provisioning file (#42): the JSON reader and the checks on each setting.
#include <doctest/doctest.h>

#include <fstream>
#include <sstream>
#include <string>

#include "provisioning/mini_json.h"
#include "provisioning/provision.h"

using provisioning::ParseProvision;
using provisioning::Provision;

namespace {

bool Mentions(const provisioning::ParseResult& result, const std::string& text)
{
    for (const std::string& problem : result.problems) {
        if (problem.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST_CASE("mini_json reads the JSON a person writes")
{
    mini_json::Value root;
    std::string error;
    REQUIRE(mini_json::Parse("\xEF\xBB\xBF { \"a\": [1, -2.5e2, true, false, null],\n"
                             "  \"s\": \"caf\\u00e9 \\\"q\\\" \\ud83d\\ude00\\n\" }",
                             &root, &error));
    REQUIRE(root.type == mini_json::Value::Type::kObject);
    const mini_json::Value* a = root.Find("a");
    REQUIRE(a != nullptr);
    REQUIRE(a->array.size() == 5);
    CHECK(a->array[1].number == -250.0);
    CHECK(a->array[2].boolean);
    CHECK(a->array[4].type == mini_json::Value::Type::kNull);
    CHECK(root.Find("s")->string == "caf\xC3\xA9 \"q\" \xF0\x9F\x98\x80\n");
    CHECK(root.Find("missing") == nullptr);
}

TEST_CASE("mini_json says where a hand-written file went wrong")
{
    mini_json::Value root;
    std::string error;
    CHECK_FALSE(mini_json::Parse("{\n  \"a\": 1,\n  \"b\": 2,\n}", &root, &error));
    CHECK(error.find("line 4") != std::string::npos);  // trailing comma
    CHECK_FALSE(mini_json::Parse("{\"a\": \"no end}", &root, &error));
    CHECK_FALSE(mini_json::Parse("{\"a\": 01}", &root, &error));
    CHECK_FALSE(mini_json::Parse("{} extra", &root, &error));
    CHECK_FALSE(mini_json::Parse("", &root, &error));
    CHECK_FALSE(mini_json::Parse("\"\\ud800\"", &root, &error));
    CHECK_FALSE(mini_json::Parse(std::string(40, '[') + std::string(40, ']'), &root, &error));
    CHECK(error.find("deeply") != std::string::npos);
    CHECK(root.type == mini_json::Value::Type::kNull);
}

TEST_CASE("the example file in the repo is valid")
{
    std::ifstream file(std::string(KEEPSAKE_REPO_DIR) + "/provision.example.json");
    REQUIRE(file.good());
    std::stringstream text;
    text << file.rdbuf();
    Provision provision;
    const auto result = ParseProvision(text.str(), &provision);
    CHECK(result.ok);
    CHECK(result.problems.empty());
    CHECK(result.warnings.empty());
    CHECK(provision.wifi.size() == 2);
    CHECK(provision.timezone == "Europe/London");
    CHECK(provision.birthday == "07-14");
    CHECK(provision.gemini_key.empty());
}

TEST_CASE("a full file imports, trimmed and normalised")
{
    Provision p;
    const auto result = ParseProvision(R"({
        "wifi": [ { "ssid": "Home", "password": "correct horse" },
                  { "ssid": "Cafe" } ],
        "timezone": "America/Toronto",
        "friend_name": "  Sam ",
        "birthday": "1994-02-29",
        "welcome_message": "Hi!",
        "gemini_key": "AIzaExampleKey123"
    })", &p);
    REQUIRE(result.ok);
    CHECK(p.wifi[0] == provisioning::WifiNetwork{"Home", "correct horse"});
    CHECK(p.wifi[1].password.empty());
    CHECK(p.friend_name == "Sam");
    CHECK(p.birthday == "02-29");
    CHECK(provisioning::DescribeForLog(p) ==
          "wifi: 2 networks, timezone, friend name, birthday, welcome message, gemini key");
    // The log line never contains the values.
    CHECK(provisioning::DescribeForLog(p).find("correct") == std::string::npos);
}

TEST_CASE("each kind of mistake is reported, and nothing is imported")
{
    Provision p;
    const auto result = ParseProvision(R"({
        "wifi": [ { "ssid": "Home", "password": "long enough pw" },
                  { "ssid": "Cafe", "password": "short" },
                  { "password": "no name" },
                  { "ssid": "Home", "password": "another fine one" },
                  "not an object" ],
        "timezone": "London time",
        "birthday": "14/07",
        "friend_name": 42,
        "gemini_key": "has space",
        "version": 2
    })", &p);
    CHECK_FALSE(result.ok);
    CHECK(Mentions(result, "wifi[1].password"));
    CHECK(Mentions(result, "wifi[2].ssid"));
    CHECK(Mentions(result, "wifi[3].ssid: \"Home\" is listed twice"));
    CHECK(Mentions(result, "wifi[4]"));
    CHECK(Mentions(result, "timezone"));
    CHECK(Mentions(result, "birthday"));
    CHECK(Mentions(result, "friend_name"));
    CHECK(Mentions(result, "gemini_key"));
    CHECK(Mentions(result, "version"));
    CHECK(p.empty());

    CHECK(Mentions(ParseProvision("{ not json", &p), "isn't valid JSON"));
    CHECK(Mentions(ParseProvision("[]", &p), "one { ... } object"));
    CHECK(Mentions(ParseProvision("{}", &p), "nothing to set up"));
    CHECK(Mentions(ParseProvision(R"({"birthday": "02-30"})", &p), "birthday"));
    const std::string six = R"({"wifi": [{"ssid":"a"},{"ssid":"b"},{"ssid":"c"},{"ssid":"d"},)"
                            R"({"ssid":"e"},{"ssid":"f"}]})";
    CHECK(Mentions(ParseProvision(six, &p), "at most 5"));
}

TEST_CASE("unknown settings are warnings, not errors; 64-hex keys are fine")
{
    Provision p;
    const auto result = ParseProvision(
        R"({"timezone": "UTC", "frend_name": "typo",
            "wifi": [{"ssid": "Hex", "password": ")" + std::string(64, 'a') + R"("}]})",
        &p);
    CHECK(result.ok);
    REQUIRE(result.warnings.size() == 1);
    CHECK(result.warnings[0].find("frend_name") != std::string::npos);
    CHECK(p.wifi[0].password.size() == 64);
}

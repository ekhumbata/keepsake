// Updates over the air (#40): versions, the release manifest, when to check, and the health
// check that decides whether a new firmware is kept.
#include <doctest/doctest.h>

#include <string>

#include "firmware_update/firmware_update.h"

using namespace firmware_update;

namespace {

Version V(const char* text)
{
    Version version;
    REQUIRE(ParseVersion(text, &version));
    return version;
}

const char* kSha = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

std::string Manifest(const std::string& version, const std::string& extra = "")
{
    return std::string(R"({"version": ")") + version +
           R"(", "url": "https://example.com/keepsake.bin", "size": 1500000, "sha256": ")" + kSha +
           "\"" + extra + "}";
}

}  // namespace

TEST_CASE("versions parse like SemVer")
{
    Version version;
    CHECK(ParseVersion("1.22.0", &version));
    CHECK(version.major == 1);
    CHECK(version.minor == 22);
    CHECK(version.patch == 0);
    CHECK_FALSE(version.prerelease);
    CHECK(ParseVersion("v2.0.1", &version));
    CHECK(version.major == 2);
    CHECK(ParseVersion("1.2.3-rc.1", &version));
    CHECK(version.prerelease);
    CHECK(ParseVersion("1.2.3+build-7", &version));
    CHECK_FALSE(version.prerelease);

    for (const char* bad : {"", "1", "1.2", "1.2.", "1..3", "01.2.3", "1.2.3-", "1.2.3 ",
                            "1.2.3.4", "a.b.c", "1.2.3-r c", "10000.0.0", "-1.2.3", "v"}) {
        CAPTURE(bad);
        CHECK_FALSE(ParseVersion(bad, &version));
    }
}

TEST_CASE("versions compare by number, not text")
{
    CHECK(Compare(V("1.10.0"), V("1.9.0")) > 0);
    CHECK(Compare(V("1.9.9"), V("2.0.0")) < 0);
    CHECK(Compare(V("1.2.3"), V("v1.2.3")) == 0);
    CHECK(Compare(V("1.2.3-rc1"), V("1.2.3")) < 0);
    CHECK(Compare(V("1.2.3"), V("1.2.2")) > 0);
}

TEST_CASE("a good manifest parses")
{
    Release release;
    std::string error;
    REQUIRE(ParseManifest(Manifest("v1.22.0", R"(, "notes": "Reminders are here.")"), &release,
                          &error));
    CHECK(release.version_text == "1.22.0");
    CHECK(Compare(release.version, V("1.22.0")) == 0);
    CHECK(release.url == "https://example.com/keepsake.bin");
    CHECK(release.size == 1500000);
    CHECK(release.sha256 == kSha);
    CHECK(release.notes == "Reminders are here.");
}

TEST_CASE("bad manifests are refused with a reason")
{
    const std::string cases[] = {
        "",
        "[]",
        "{}",
        R"({"version": "1.2"})",
        Manifest("1.2.3", R"(, "notes": 5)"),
        Manifest("1.2.3", R"(, "notes": "two\nlines")"),
        Manifest("1.2.3", R"(, "notes": ")" + std::string(300, 'x') + "\""),
        R"({"version": "1.2.3", "url": "http://example.com/a.bin", "size": 1, "sha256": ")" +
            std::string(kSha) + "\"}",
        R"({"version": "1.2.3", "url": "https://example.com/a.bin", "size": 0, "sha256": ")" +
            std::string(kSha) + "\"}",
        R"({"version": "1.2.3", "url": "https://example.com/a.bin", "size": 1.5, "sha256": ")" +
            std::string(kSha) + "\"}",
        R"({"version": "1.2.3", "url": "https://example.com/a.bin", "size": 10, "sha256": "ABC"})",
        R"({"version": "1.2.3", "url": "https://exa mple.com/a.bin", "size": 10, "sha256": ")" +
            std::string(kSha) + "\"}",
        "{\"version\": \"1.2.3\"" + std::string(5000, ' ') + "}",
    };
    for (const std::string& text : cases) {
        CAPTURE(text);
        Release release;
        std::string error;
        CHECK_FALSE(ParseManifest(text, &release, &error));
        CHECK_FALSE(error.empty());
    }
}

TEST_CASE("only a newer, good, fitting release is installed")
{
    Release release;
    REQUIRE(ParseManifest(Manifest("1.22.0"), &release, nullptr));
    CHECK(Decide(V("1.21.0"), release, "", 0x7E0000) == Decision::kInstall);
    CHECK(Decide(V("1.22.0"), release, "", 0x7E0000) == Decision::kUpToDate);
    CHECK(Decide(V("1.23.0"), release, "", 0x7E0000) == Decision::kUpToDate);
    CHECK(Decide(V("1.21.0"), release, "1.22.0", 0x7E0000) == Decision::kKnownBad);
    CHECK(Decide(V("1.21.0"), release, "1.21.5", 0x7E0000) == Decision::kInstall);
    CHECK(Decide(V("1.21.0"), release, "garbage", 0x7E0000) == Decision::kInstall);
    CHECK(Decide(V("1.21.0"), release, "", 1000) == Decision::kTooBig);

    REQUIRE(ParseManifest(Manifest("1.22.0-rc1"), &release, nullptr));
    CHECK(Decide(V("1.21.0"), release, "", 0x7E0000) == Decision::kPrerelease);
}

TEST_CASE("the daily check")
{
    constexpr int64_t kDay = 24 * 3600;
    constexpr int64_t kNow = 1800000000;  // 2027
    CHECK(CheckDue(0, kNow, kDay));
    CHECK(CheckDue(0, 1000, kDay));  // never checked: fine even without a clock
    CHECK_FALSE(CheckDue(kNow - 3600, kNow, kDay));
    CHECK(CheckDue(kNow - kDay, kNow, kDay));
    CHECK(CheckDue(kNow + 3600, kNow, kDay));       // the clock went back
    CHECK_FALSE(CheckDue(kNow - 3 * kDay, 1000, kDay));  // clock not set yet
}

TEST_CASE("updates wait for enough battery")
{
    CHECK(PowerAllows(true, 3, false));
    CHECK(PowerAllows(false, -1, false));
    CHECK(PowerAllows(false, 30, false));
    CHECK_FALSE(PowerAllows(false, 29, false));
    CHECK(PowerAllows(false, 20, true));
    CHECK_FALSE(PowerAllows(false, 14, true));
}

TEST_CASE("the health check keeps or rolls back a new firmware")
{
    const uint8_t with_card = RequiredChecks(true);
    const uint8_t without_card = RequiredChecks(false);
    CHECK(with_card == (kHealthHomeReached | kHealthDisplayOk | kHealthStorageOk));
    CHECK(without_card == (kHealthHomeReached | kHealthDisplayOk));

    CHECK(Judge(0, with_card, 0, 20000, 90000) == Verdict::kWaiting);
    CHECK(Judge(kHealthHomeReached | kHealthDisplayOk, with_card, 30000, 20000, 90000) ==
          Verdict::kWaiting);
    CHECK(Judge(kHealthHomeReached | kHealthDisplayOk, without_card, 30000, 20000, 90000) ==
          Verdict::kHealthy);
    // Everything passed, but it hasn't run long enough yet to trust it.
    CHECK(Judge(with_card, with_card, 5000, 20000, 90000) == Verdict::kWaiting);
    CHECK(Judge(with_card, with_card, 20000, 20000, 90000) == Verdict::kHealthy);
    CHECK(Judge(with_card, with_card, 200000, 20000, 90000) == Verdict::kHealthy);
    CHECK(Judge(kHealthHomeReached, with_card, 90000, 20000, 90000) == Verdict::kFailed);
}

TEST_CASE("status lines are plain language")
{
    Status status;
    CHECK(Describe(status) == "Updates install by themselves over Wi-Fi.");
    CHECK(ButtonLabel(status) == "Check for updates");

    status.phase = Phase::kDownloading;
    status.version = "1.22.0";
    status.percent = 140;
    CHECK(Describe(status) == "Downloading 1.22.0... 100%");
    CHECK(ButtonLabel(status) == "Downloading...");

    status.phase = Phase::kReady;
    CHECK(Describe(status) == "Version 1.22.0 is ready. Restart to finish.");
    CHECK(ButtonLabel(status) == "Restart to update");

    status.phase = Phase::kFailed;
    status.problem = Problem::kNoWifi;
    CHECK(Describe(status) == "Couldn't check: no Wi-Fi.");
    status.problem = Problem::kLowBattery;
    CHECK(Describe(status) == "Plug in to update: the battery is low.");

    status.phase = Phase::kRolledBack;
    CHECK(Describe(status).find("1.22.0") != std::string::npos);
    CHECK(ButtonLabel(status) == "Check for updates");
}

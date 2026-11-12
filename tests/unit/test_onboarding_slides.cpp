// The first-run carousel (#86): what it says depends on the device's setup.
#include <doctest/doctest.h>

#include <string>

#include "epaper_ui/onboarding_slides.h"

using epaper_ui::OnboardingContext;
using epaper_ui::OnboardingSlides;

TEST_CASE("onboarding greets by name and introduces both mascots")
{
    OnboardingContext context = {};
    context.friend_name = "Sam";
    const auto slides = OnboardingSlides(context);
    REQUIRE(slides.size() == 7);
    CHECK(slides[0].title == "Hi, Sam!");
    CHECK(slides[1].title == "Meet Bookworm");
    CHECK(slides[2].title == "Meet Mochi");
    for (const auto& slide : slides) {
        CAPTURE(slide.title);
        CHECK(slide.image != nullptr);
        CHECK_FALSE(slide.body.empty());
    }
    context.friend_name = "";
    CHECK(OnboardingSlides(context)[0].title == "Hello!");
}

TEST_CASE("no AI wording without a key")
{
    OnboardingContext context = {};
    for (const auto& slide : OnboardingSlides(context)) {
        CHECK(slide.body.find("writes it down") == std::string::npos);
        CHECK(slide.body.find("Gemini") == std::string::npos);
    }
    context.ai_enabled = true;
    CHECK(OnboardingSlides(context)[4].body.find("writes it down") != std::string::npos);
}

TEST_CASE("the last slide depends on Wi-Fi")
{
    OnboardingContext context = {};
    context.wifi_ready = true;
    CHECK(OnboardingSlides(context).back().title == "All set");
    context.wifi_ready = false;
    CHECK(OnboardingSlides(context).back().title == "One last thing");
}

#ifndef EPAPER_UI_ONBOARDING_SLIDES_H_
#define EPAPER_UI_ONBOARDING_SLIDES_H_

#include <string>
#include <string_view>
#include <vector>

struct EmbeddedImageAsset;

namespace epaper_ui {

// The first-run carousel's words and pictures (#86): Keepsake as a reader and companion, the
// two mascots, the buttons, and Wi-Fi. What's said depends on the device's setup.
struct OnboardingContext {
    std::string_view friend_name;  // from the provisioning file; may be empty
    bool ai_enabled = false;       // a Gemini key is set: voice notes get written down
    bool wifi_ready = false;       // a Wi-Fi network is already known
};

struct OnboardingSlide {
    std::string title;
    std::string body;
    const EmbeddedImageAsset* image = nullptr;
};

std::vector<OnboardingSlide> OnboardingSlides(const OnboardingContext& context);

}  // namespace epaper_ui

#endif  // EPAPER_UI_ONBOARDING_SLIDES_H_

#include "epaper_ui/onboarding_slides.h"

#include "epaper_ui/book_page.h"
#include "project_assets.h"

namespace epaper_ui {

std::vector<OnboardingSlide> OnboardingSlides(const OnboardingContext& context)
{
    using project_assets::GetImage;
    std::vector<OnboardingSlide> slides;

    const std::string hello = context.friend_name.empty()
                                  ? std::string("Hello!")
                                  : "Hi, " + ToDisplayText(context.friend_name) + "!";
    slides.push_back({hello,
                      "Welcome to Keepsake: a little reader and companion, made just for you. "
                      "Two friends live here.",
                      GetImage(EmbeddedImageId::kMascotHello)});
    slides.push_back({"Meet Bookworm",
                      "Bookworm looks after your books. They're right on Home: pick one, and "
                      "Bookworm keeps your place.",
                      GetImage(EmbeddedImageId::kMascotBookwormReading)});
    slides.push_back({"Meet Mochi",
                      "Mochi keeps you company with the time, little notes, and the days that "
                      "matter.",
                      GetImage(EmbeddedImageId::kMascotMochiHappy)});
    slides.push_back({"Find your way",
                      "Tilt the rocker up or down to move, and press it in to choose.",
                      GetImage(EmbeddedImageId::kSlide3)});
    slides.push_back({"Back, and voice notes",
                      std::string("A quick press of the Record button goes back. Hold it to "
                                  "leave yourself a voice note.") +
                          (context.ai_enabled ? " Keepsake writes it down for you." : ""),
                      GetImage(EmbeddedImageId::kSlide2)});
    slides.push_back({"Rest and power",
                      "Keepsake rests when you leave it be; press Record to wake it. To turn it "
                      "off, hold the power button for about a second.",
                      GetImage(EmbeddedImageId::kSlide4)});
    if (context.wifi_ready) {
        slides.push_back({"All set",
                          "Keepsake already knows your Wi-Fi, so the clock and updates look "
                          "after themselves. Happy reading!",
                          GetImage(EmbeddedImageId::kMascotDayCelebrate)});
    } else {
        slides.push_back({"One last thing",
                          "Wi-Fi keeps the clock right and brings little updates. Next, "
                          "Keepsake can help you set it up from your phone, or you can skip it "
                          "for now.",
                          GetImage(EmbeddedImageId::kMascotDayCelebrate)});
    }
    return slides;
}

}  // namespace epaper_ui

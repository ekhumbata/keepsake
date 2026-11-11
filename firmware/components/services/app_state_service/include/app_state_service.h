#ifndef APP_STATE_SERVICE_H_
#define APP_STATE_SERVICE_H_

#include <cstdint>
#include <string>

namespace app_state_service {

// Persisted first-run flag. Device/firmware state (not tied to the SD card), so it
// survives an SD format; a future Settings action can clear it to replay onboarding.
bool OnboardingViewed();
void MarkOnboardingViewed();

// Reader text size (#28): 0 small, 1 medium (the default), 2 large. Kept on the device rather
// than the SD card, like the onboarding flag.
uint8_t ReaderTextSize();
void SetReaderTextSize(uint8_t size);

// Personal touches (#42, #85, #86), from the provisioning file. On the device only (NVS), never
// in the repo. Empty when not set.
struct PersonalSettings {
    std::string friend_name;
    std::string birthday;          // "MM-DD"
    std::string welcome_message;
    std::string weather_location;
};
PersonalSettings Personal();
// Saves the non-empty fields of `settings` (an empty field leaves the saved one alone).
bool SavePersonal(const PersonalSettings& settings);

}  // namespace app_state_service

#endif  // APP_STATE_SERVICE_H_

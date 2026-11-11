#ifndef APP_STATE_SERVICE_H_
#define APP_STATE_SERVICE_H_

#include <cstdint>

namespace app_state_service {

// Persisted first-run flag. Device/firmware state (not tied to the SD card), so it
// survives an SD format; a future Settings action can clear it to replay onboarding.
bool OnboardingViewed();
void MarkOnboardingViewed();

// Reader text size (#28): 0 small, 1 medium (the default), 2 large. Kept on the device rather
// than the SD card, like the onboarding flag.
uint8_t ReaderTextSize();
void SetReaderTextSize(uint8_t size);

}  // namespace app_state_service

#endif  // APP_STATE_SERVICE_H_

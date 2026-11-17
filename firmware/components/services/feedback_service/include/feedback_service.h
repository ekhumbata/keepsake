#ifndef FEEDBACK_SERVICE_H_
#define FEEDBACK_SERVICE_H_

#include "esp_err.h"

namespace feedback_service {

enum class FeedbackEvent {
    kStartup,
    kGeminiConnected,
    kLock,
    kUnlock,
    kRecordingStart,
    kModalOpen,
    kButtonClick,
    kButtonDoubleClick,
    kButtonLongPress,
    kShutdown,
    kError,
};

esp_err_t Init();
esp_err_t Play(FeedbackEvent event);
// Button sounds (#163): when off, menu clicks and the picker cue are silent; other sounds play.
void SetButtonSoundsEnabled(bool enabled);

}  // namespace feedback_service

#endif  // FEEDBACK_SERVICE_H_

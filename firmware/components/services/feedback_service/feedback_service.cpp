#include "feedback_service.h"

#include <atomic>

#include "esp_log.h"
#include "waveshare_board.h"
#include "system_sound_service.h"

namespace feedback_service {
namespace {

constexpr const char* kTag = "FeedbackService";

// The sounds a menu press makes (#163): clicks, and the cue for a picker opening. Startup,
// lock/unlock, recording, shutdown and error sounds aren't affected by the setting.
std::atomic<bool> s_button_sounds_enabled{true};

bool IsButtonSound(FeedbackEvent event)
{
    switch (event) {
        case FeedbackEvent::kButtonClick:
        case FeedbackEvent::kButtonDoubleClick:
        case FeedbackEvent::kButtonLongPress:
        case FeedbackEvent::kModalOpen:
            return true;
        default:
            return false;
    }
}

// Maps a feedback event onto the closest system-sound cue (played through the
// ES8311 codec). There is no dedicated buzzer on this board.
SoundCue CueForEvent(FeedbackEvent event)
{
    switch (event) {
        case FeedbackEvent::kStartup:
            return SoundCue::kStartup;
        case FeedbackEvent::kGeminiConnected:
            return SoundCue::kOnline;
        case FeedbackEvent::kLock:
            return SoundCue::kLock;
        case FeedbackEvent::kUnlock:
            return SoundCue::kUnlock;
        case FeedbackEvent::kRecordingStart:
            return SoundCue::kButtonActivate;
        case FeedbackEvent::kModalOpen:
            return SoundCue::kModalNotification;
        case FeedbackEvent::kButtonClick:
        case FeedbackEvent::kButtonDoubleClick:
        case FeedbackEvent::kButtonLongPress:
            return SoundCue::kButtonActivate;
        case FeedbackEvent::kShutdown:
            return SoundCue::kModalNotification;
        case FeedbackEvent::kError:
        default:
            return SoundCue::kInterrupt;
    }
}

}  // namespace

esp_err_t Init()
{
    AudioCodec* codec = waveshare_board::GetAudioCodec();
    if (codec == nullptr) {
        ESP_LOGW(kTag, "Audio codec unavailable; feedback cues disabled");
        return ESP_ERR_NOT_FOUND;
    }

    // Starts the sound-service playback task and warms (decodes) the cue cache.
    SystemSoundService::GetInstance().Initialize(codec);
    ESP_LOGI(kTag, "Feedback service initialized (audio cues via ES8311)");
    return ESP_OK;
}

void SetButtonSoundsEnabled(bool enabled)
{
    s_button_sounds_enabled.store(enabled, std::memory_order_relaxed);
}

esp_err_t Play(FeedbackEvent event)
{
    if (IsButtonSound(event) && !s_button_sounds_enabled.load(std::memory_order_relaxed)) {
        return ESP_OK;  // button sounds are off (#163)
    }
    SystemSoundService::GetInstance().PlayCue(CueForEvent(event));
    return ESP_OK;
}

}  // namespace feedback_service

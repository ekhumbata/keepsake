#include "battery_runtime.h"

#include <atomic>
#include <string>

#include "battery_ux/battery_ux.h"
#include "chroninkle_task_config.h"
#include "epaper_ui/toast.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "overlay_runtime.h"
#include "power_service.h"
#include "project_assets.h"
#include "status_bar_runtime.h"

namespace battery_runtime {
namespace {

constexpr const char* kTag = "BatteryRuntime";
constexpr uint32_t kStackBytes = 4096;
// power_service refreshes its cache every 2 s; plug-in is noticed within that. The percentage
// is smoothed over readings 30 s apart, so a fuel-gauge blip barely moves it.
constexpr TickType_t kTick = pdMS_TO_TICKS(2000);
constexpr int kTicksPerPercentSample = 15;
// The charging card closes by itself after this long (or when the charger comes out).
constexpr int kChargingCardTicks = 8;  // ~16 s
constexpr uint32_t kNudgeToastMs = 5000;

std::atomic<int> s_display_percent{-1};
TaskHandle_t s_task = nullptr;

const EmbeddedImageAsset* Image(EmbeddedImageId id)
{
    return project_assets::GetImage(id);
}

void ShowCharging(int percent, std::string* shown_title)
{
    if (overlay_runtime::IsInputCaptured()) {
        return;  // don't cover a dialog; the status bar shows the bolt anyway
    }
    const battery_ux::Message message = battery_ux::ChargingMessage(percent);
    if (overlay_runtime::ShowNoteCard(message.title, message.body,
                                      Image(EmbeddedImageId::kMascotCharging)) == ESP_OK) {
        *shown_title = message.title;
    }
}

void ShowNudge(int threshold)
{
    const battery_ux::Message message = battery_ux::NudgeMessage(threshold);
    ESP_LOGI(kTag, "Low battery nudge at %d%%", threshold);
    if (message.card && !overlay_runtime::IsInputCaptured()) {
        (void)overlay_runtime::ShowNoteCard(message.title, message.body,
                                            Image(EmbeddedImageId::kMascotSleepy));
        return;
    }
    epaper_ui::ToastState toast = {};
    toast.visible = true;
    toast.body_text = message.title + ". " + message.body;
    toast.leading_icon = project_assets::GetIcon(EmbeddedIconId::kBattery2);
    (void)overlay_runtime::ShowToastForDuration(toast, kNudgeToastMs);
}

void Task(void*)
{
    battery_ux::PercentSmoother smoother;
    battery_ux::NudgeTracker nudges;
    bool usb_before = false;
    bool first = true;
    int tick = 0;
    int card_ticks_left = 0;
    std::string card_title;

    for (;;) {
        power_service::Status status = {};
        if (power_service::ReadStatus(&status) == ESP_OK) {
            const bool usb = status.usb_detected;
            const bool charging = status.charge_state == power_service::ChargeState::kCharging;
            const bool have_battery = status.battery.available;

            if (have_battery && (first || tick % kTicksPerPercentSample == 0 || usb != usb_before)) {
                const int before = s_display_percent.load();
                const int shown =
                    smoother.Update(status.battery.state_of_charge_percent, charging || usb);
                s_display_percent = shown;
                if (shown != before && !first) {
                    (void)status_bar_runtime::UpdateDisplayStateAndRequestRefresh(
                        display_service::RefreshMode::kPartial);
                }
                if (const int threshold = nudges.Update(shown, usb); threshold > 0) {
                    ShowNudge(threshold);
                }
            }

            if (!first && usb && !usb_before) {
                ESP_LOGI(kTag, "Charger plugged in");
                ShowCharging(s_display_percent.load(), &card_title);
                card_ticks_left = card_title.empty() ? 0 : kChargingCardTicks;
            } else if (!first && !usb && usb_before && !card_title.empty()) {
                ESP_LOGI(kTag, "Charger unplugged");
                (void)overlay_runtime::DismissNoteCardTitled(card_title);
                card_title.clear();
                card_ticks_left = 0;
            }
            if (card_ticks_left > 0 && --card_ticks_left == 0) {
                (void)overlay_runtime::DismissNoteCardTitled(card_title);
                card_title.clear();
            }
            usb_before = usb;
            first = false;
        }
        ++tick;
        vTaskDelay(kTick);
    }
}

}  // namespace

void Start()
{
    if (s_task != nullptr) {
        return;
    }
    if (xTaskCreatePinnedToCore(Task, "battery_ux", kStackBytes, nullptr,
                                chroninkle_task_config::kPriorityBatteryUx, &s_task,
                                chroninkle_task_config::kAppCore) != pdPASS) {
        ESP_LOGW(kTag, "Battery task create failed; no charging card or nudges");
        s_task = nullptr;
    }
}

int DisplayPercent()
{
    return s_display_percent.load();
}

}  // namespace battery_runtime

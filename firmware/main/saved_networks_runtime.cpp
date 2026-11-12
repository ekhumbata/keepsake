#include "saved_networks_runtime.h"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "epaper_ui/book_page.h"
#include "epaper_ui/select_modal.h"
#include "epaper_ui/toast.h"
#include "esp_log.h"
#include "overlay_runtime.h"
#include "project_assets.h"
#include "wifi_service.h"

namespace saved_networks_runtime {
namespace {

constexpr const char* kTag = "SavedNetworks";
constexpr uint32_t kToastMs = 3000;

std::mutex s_mutex;
std::atomic<bool> s_show_requested{false};
bool s_list_pending = false;           // our select modal is open
std::vector<std::string> s_networks;   // in the order shown
std::string s_to_forget;

void Toast(const std::string& text, EmbeddedIconId icon)
{
    epaper_ui::ToastState toast = {};
    toast.visible = true;
    toast.body_text = text;
    toast.leading_icon = project_assets::GetIcon(icon);
    (void)overlay_runtime::ShowToastForDuration(toast, kToastMs);
}

}  // namespace

void RequestShowList()
{
    s_show_requested = true;
}

bool ShowListIfRequested()
{
    if (!s_show_requested.exchange(false)) {
        return false;
    }
    const std::vector<std::string> networks = wifi_service::SavedNetworks();
    if (networks.empty()) {
        Toast("No saved Wi-Fi yet. Set one up in Network.", EmbeddedIconId::kWifiConfig);
        return true;
    }
    epaper_ui::SelectModalState modal = {};
    modal.visible = true;
    modal.title_text = "Saved Wi-Fi";
    const wifi_service::UiState ui = wifi_service::GetUiState();
    for (const std::string& ssid : networks) {
        std::string label = epaper_ui::ToDisplayText(ssid);
        if (ui.connected && ui.ssid == ssid) {
            label += " (connected)";
        }
        modal.items.push_back({label});
    }
    modal.items.push_back({"Close"});
    modal.selected_index = 0;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_networks = networks;
        s_list_pending = true;
    }
    const esp_err_t err = overlay_runtime::ShowSelectModal(modal);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_list_pending = false;
        ESP_LOGW(kTag, "Saved Wi-Fi list failed: %s", esp_err_to_name(err));
    }
    return true;
}

bool HandleSelection(int selected_index)
{
    std::string ssid;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_list_pending) {
            return false;
        }
        s_list_pending = false;
        if (selected_index < 0 || selected_index >= static_cast<int>(s_networks.size())) {
            return true;  // Close
        }
        ssid = s_networks[static_cast<size_t>(selected_index)];
        s_to_forget = ssid;
    }
    const esp_err_t err = overlay_runtime::ShowForgetNetworkCard(epaper_ui::ToDisplayText(ssid));
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Forget confirmation failed: %s", esp_err_to_name(err));
    }
    return true;
}

void ForgetConfirmed()
{
    std::string ssid;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        ssid.swap(s_to_forget);
    }
    if (ssid.empty()) {
        return;
    }
    const bool ok = wifi_service::ForgetNetwork(ssid);
    ESP_LOGI(kTag, "Forget saved network: %s", ok ? "done" : "failed");
    Toast(ok ? "Forgot " + epaper_ui::ToDisplayText(ssid) + "."
             : std::string("Couldn't forget that network. Try again?"),
          ok ? EmbeddedIconId::kCheck : EmbeddedIconId::kClose);
}

}  // namespace saved_networks_runtime

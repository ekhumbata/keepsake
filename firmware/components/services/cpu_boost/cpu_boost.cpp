#include "cpu_boost.h"

#include <atomic>

#include "esp_log.h"
#include "sdkconfig.h"

#if CONFIG_PM_ENABLE
#include "esp_pm.h"
#endif

namespace cpu_boost {
namespace {

constexpr const char* kTag = "CpuBoost";

#if CONFIG_PM_ENABLE && CONFIG_KEEPSAKE_PM_DFS
std::atomic<esp_pm_lock_handle_t> s_lock{nullptr};
#endif

}  // namespace

esp_err_t Init()
{
#if CONFIG_PM_ENABLE && CONFIG_KEEPSAKE_PM_DFS
    if (s_lock.load() != nullptr) {
        return ESP_OK;
    }
    esp_pm_config_t config = {};
    config.max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
    config.min_freq_mhz = CONFIG_KEEPSAKE_PM_MIN_CPU_FREQ_MHZ;
    // Automatic light sleep is #55; CONFIG_KEEPSAKE_DEV_NO_AUTO_SLEEP will veto it there.
    config.light_sleep_enable = false;
    esp_err_t err = esp_pm_configure(&config);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_pm_configure(%d..%d MHz) failed: %s", config.min_freq_mhz,
                 config.max_freq_mhz, esp_err_to_name(err));
        return err;
    }
    esp_pm_lock_handle_t lock = nullptr;
    err = esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "cpu_boost", &lock);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_pm_lock_create failed: %s", esp_err_to_name(err));
        return err;
    }
    s_lock.store(lock);
    ESP_LOGI(kTag, "Frequency scaling on: %d MHz idle, %d MHz while boosted",
             config.min_freq_mhz, config.max_freq_mhz);
#else
    ESP_LOGI(kTag, "Frequency scaling off (fixed %d MHz)", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
#endif
    return ESP_OK;
}

ScopedBoost::ScopedBoost(const char* reason)
{
    (void)reason;
#if CONFIG_PM_ENABLE && CONFIG_KEEPSAKE_PM_DFS
    esp_pm_lock_handle_t lock = s_lock.load();
    if (lock != nullptr && esp_pm_lock_acquire(lock) == ESP_OK) {
        held_ = true;
    }
#endif
}

ScopedBoost::~ScopedBoost()
{
#if CONFIG_PM_ENABLE && CONFIG_KEEPSAKE_PM_DFS
    if (held_) {
        esp_pm_lock_release(s_lock.load());
    }
#endif
}

}  // namespace cpu_boost

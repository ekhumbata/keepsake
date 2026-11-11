#include "net_jobs_service.h"

#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "chroninkle_task_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "wifi_service.h"

namespace net_jobs_service {
namespace {

constexpr const char* kTag = "NetJobs";
// Jobs block on HTTP/NTP and the TLS stack is deep; same budget as the other network tasks.
constexpr uint32_t kTaskStackBytes = 8192;
// While a radio-off is held back (Wi-Fi page open, hotspot up), look again this often.
constexpr uint32_t kHoldRecheckMs = 5000;

#if CONFIG_KEEPSAKE_WIFI_ON_DEMAND
constexpr bool kOnDemand = true;
constexpr net_jobs::Millis kSyncIntervalMs =
    static_cast<net_jobs::Millis>(CONFIG_KEEPSAKE_NET_SYNC_INTERVAL_HOURS) * 3600u * 1000u;
#else
constexpr bool kOnDemand = false;
constexpr net_jobs::Millis kSyncIntervalMs = 0;
#endif

struct Job {
    std::string name;
    net_jobs::Urgency urgency = net_jobs::Urgency::kBackground;
    Work work;
};

std::mutex s_mutex;
TaskHandle_t s_task = nullptr;
net_jobs::Scheduler s_scheduler;
std::map<net_jobs::JobId, Job> s_jobs;
net_jobs::JobId s_next_id = 1;
bool s_connected = false;
bool s_link_changed = false;
bool s_radio_off_pending = false;  // on-demand: a radio-off held back by the hold provider
std::deque<net_jobs::JobId> s_new_jobs;
std::deque<net_jobs::JobId> s_passthrough_waiting;  // pass-through: queued until connected
HoldProvider s_hold_provider;
Work s_sync_work;
net_jobs::Millis s_next_sync_at = 0;

net_jobs::Millis NowMs()
{
    return static_cast<net_jobs::Millis>(esp_timer_get_time() / 1000);
}

void Wake()
{
    if (s_task != nullptr) {
        xTaskNotifyGive(s_task);
    }
}

bool Held()
{
    return s_hold_provider && s_hold_provider();
}

// Runs one job (unlocked) and returns whether it should be retried later.
bool RunJob(net_jobs::JobId id)
{
    Job job;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const auto it = s_jobs.find(id);
        if (it == s_jobs.end()) {
            return false;
        }
        job = it->second;
    }
    ESP_LOGI(kTag, "Running job %lu (%s)", static_cast<unsigned long>(id), job.name.c_str());
    const bool done = job.work ? job.work() : true;
    if (done) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_jobs.erase(id);
    } else {
        ESP_LOGI(kTag, "Job %lu (%s) will retry later", static_cast<unsigned long>(id),
                 job.name.c_str());
    }
    return !done;
}

void ApplyRadio(const net_jobs::Actions& actions)
{
    if (actions.radio_on) {
        s_radio_off_pending = false;
        ESP_LOGI(kTag, "Radio on");
        wifi_service::SetWifiEnabled(true);
    }
    if (actions.radio_off) {
        s_radio_off_pending = true;  // carried out below once nothing holds the radio
    }
    for (const net_jobs::JobId id : actions.unreachable) {
        ESP_LOGW(kTag, "Job %lu: no network", static_cast<unsigned long>(id));
        std::lock_guard<std::mutex> lock(s_mutex);
        s_jobs.erase(id);
    }
    for (const net_jobs::JobId id : actions.rejected) {
        ESP_LOGW(kTag, "Job %lu rejected: queue full", static_cast<unsigned long>(id));
        std::lock_guard<std::mutex> lock(s_mutex);
        s_jobs.erase(id);
    }
}

void LoopOnDemand()
{
    while (true) {
        const net_jobs::Millis now = NowMs();
        net_jobs::Millis wake_at = s_scheduler.next_deadline();
        if (kSyncIntervalMs > 0 && s_sync_work && (wake_at == 0 || s_next_sync_at < wake_at)) {
            wake_at = s_next_sync_at;
        }
        TickType_t wait = portMAX_DELAY;
        if (s_radio_off_pending) {
            wait = pdMS_TO_TICKS(kHoldRecheckMs);
        } else if (wake_at != 0) {
            wait = wake_at > now ? pdMS_TO_TICKS(wake_at - now) + 1 : 0;
        }
        ulTaskNotifyTake(pdTRUE, wait);

        std::vector<net_jobs::Actions> actions;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            const net_jobs::Millis t = NowMs();
            if (kSyncIntervalMs > 0 && s_sync_work && t >= s_next_sync_at) {
                const net_jobs::JobId id = s_next_id++;
                s_jobs[id] = Job{"periodic sync", net_jobs::Urgency::kBackground, s_sync_work};
                s_new_jobs.push_back(id);
                s_next_sync_at = t + kSyncIntervalMs;
            }
            while (!s_new_jobs.empty()) {
                const net_jobs::JobId id = s_new_jobs.front();
                s_new_jobs.pop_front();
                actions.push_back(s_scheduler.Submit(id, s_jobs[id].urgency, t));
            }
            if (s_link_changed) {
                s_link_changed = false;
                if (s_connected) {
                    // Someone else brought the radio up (Wi-Fi page) while work waited out a
                    // backoff: use the connection now.
                    if (s_scheduler.state() == net_jobs::RadioState::kBackoff) {
                        actions.push_back(s_scheduler.Tick(s_scheduler.next_deadline()));
                    }
                    actions.push_back(s_scheduler.OnConnected(t));
                } else {
                    actions.push_back(s_scheduler.OnDisconnected(t));
                }
            }
            actions.push_back(s_scheduler.Tick(t));
        }

        std::vector<net_jobs::JobId> to_run;
        for (const net_jobs::Actions& a : actions) {
            ApplyRadio(a);
            to_run.insert(to_run.end(), a.run.begin(), a.run.end());
        }
        for (const net_jobs::JobId id : to_run) {
            const bool retry = RunJob(id);
            net_jobs::Actions finished;
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                finished = s_scheduler.OnJobFinished(id, retry, NowMs());
            }
            ApplyRadio(finished);  // takes the lock itself
        }
        if (s_radio_off_pending && !Held() &&
            (s_scheduler.state() == net_jobs::RadioState::kOff ||
             s_scheduler.state() == net_jobs::RadioState::kBackoff)) {
            s_radio_off_pending = false;
            ESP_LOGI(kTag, "Radio off (%s)", net_jobs::RadioStateName(s_scheduler.state()));
            wifi_service::SetWifiEnabled(false);
        }
    }
}

void LoopPassthrough()
{
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        std::vector<net_jobs::JobId> to_run;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            while (!s_new_jobs.empty()) {
                s_passthrough_waiting.push_back(s_new_jobs.front());
                s_new_jobs.pop_front();
            }
            s_link_changed = false;
            if (!s_connected) {
                continue;
            }
            to_run.assign(s_passthrough_waiting.begin(), s_passthrough_waiting.end());
            s_passthrough_waiting.clear();
        }
        for (const net_jobs::JobId id : to_run) {
            if (RunJob(id)) {
                // Wait for the next connection rather than retrying on this one.
                std::lock_guard<std::mutex> lock(s_mutex);
                s_passthrough_waiting.push_back(id);
            }
        }
    }
}

void Task(void*)
{
    if (kOnDemand) {
        LoopOnDemand();
    } else {
        LoopPassthrough();
    }
}

}  // namespace

esp_err_t Init()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_task != nullptr) {
        return ESP_OK;
    }
    s_next_sync_at = NowMs();  // first sync right away, then every interval
    const BaseType_t created = xTaskCreatePinnedToCore(
        Task, "net_jobs", kTaskStackBytes, nullptr, chroninkle_task_config::kPriorityNetJobs,
        &s_task, chroninkle_task_config::kSystemCore);
    if (created != pdPASS) {
        s_task = nullptr;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(kTag, "Started (%s)", kOnDemand ? "Wi-Fi on demand" : "pass-through");
    return ESP_OK;
}

bool OnDemandEnabled()
{
    return kOnDemand;
}

void Submit(net_jobs::Urgency urgency, const char* name, Work work)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const net_jobs::JobId id = s_next_id++;
        s_jobs[id] = Job{name != nullptr ? name : "job", urgency, std::move(work)};
        s_new_jobs.push_back(id);
    }
    Wake();
}

void OnWifiStateChanged(bool connected)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (connected == s_connected) {
            return;
        }
        s_connected = connected;
        s_link_changed = true;
    }
    Wake();
}

void SetHoldProvider(HoldProvider provider)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_hold_provider = std::move(provider);
}

void SetPeriodicSync(Work work)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_sync_work = std::move(work);
    }
    Wake();
}

}  // namespace net_jobs_service

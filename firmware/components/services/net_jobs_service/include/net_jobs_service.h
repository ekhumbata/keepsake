#ifndef NET_JOBS_SERVICE_H_
#define NET_JOBS_SERVICE_H_

#include <functional>

#include "esp_err.h"
#include "net_jobs/scheduler.h"

// Runs network work as jobs (#17). With CONFIG_KEEPSAKE_WIFI_ON_DEMAND it owns the Wi-Fi radio
// through net_jobs::Scheduler (off by default, batch, linger, back off); without it, it's a
// pass-through: the radio behaves as before and jobs run whenever Wi-Fi is connected.
//
// Jobs run one at a time on the service's own task, so a job may block (HTTP, NTP). A job
// returns true when done, false to be retried on a later connection.
namespace net_jobs_service {

using Work = std::function<bool()>;
// True while something needs the radio on regardless of jobs (the Wi-Fi page is open, the
// setup hotspot is up). Polled; keep it cheap.
using HoldProvider = std::function<bool()>;

esp_err_t Init();
bool OnDemandEnabled();

// Queues `work`. `name` is for logs only (keep it a string literal).
void Submit(net_jobs::Urgency urgency, const char* name, Work work);

// Wi-Fi link state, from the wifi_service event handler.
void OnWifiStateChanged(bool connected);

void SetHoldProvider(HoldProvider provider);

// With Wi-Fi on demand: `work` is submitted as a background job at start-up and then every
// CONFIG_KEEPSAKE_NET_SYNC_INTERVAL_HOURS, so time sync and queued uploads still happen when
// nothing else brings the radio up. Ignored in pass-through mode (Wi-Fi is always on there).
void SetPeriodicSync(Work work);

}  // namespace net_jobs_service

#endif  // NET_JOBS_SERVICE_H_

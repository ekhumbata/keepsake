#include "crash_report_service.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "crash_report/crash_report.h"
#include "esp_app_desc.h"
#include "esp_attr.h"
#include "esp_core_dump.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "storage_service.h"

namespace crash_report_service {
namespace {

constexpr const char* kTag = "CrashReport";
constexpr size_t kRingBytes = 8 * 1024;
constexpr size_t kKeepReports = 10;
// Lines are copied through a buffer on the logging task's stack, so it stays small; longer
// lines are cut short in the ring (the serial log still gets them whole).
constexpr size_t kLineBytes = 160;

// Not cleared on a software reset or panic (only at power-up), so the log from before a crash
// is still here on the next boot. LogRing checks it before trusting it.
__NOINIT_ATTR uint8_t s_ring_memory[kRingBytes];
crash_report::LogRing s_ring;
std::string s_previous_log;  // what the ring held at boot
portMUX_TYPE s_ring_lock = portMUX_INITIALIZER_UNLOCKED;
vprintf_like_t s_original_vprintf = nullptr;

int CaptureVprintf(const char* format, va_list args)
{
    va_list copy;
    va_copy(copy, args);
    char line[kLineBytes];
    const int length = vsnprintf(line, sizeof(line), format, copy);
    va_end(copy);
    if (length > 0) {
        const size_t kept = static_cast<size_t>(length) < sizeof(line) ? static_cast<size_t>(length)
                                                                       : sizeof(line) - 1;
        portENTER_CRITICAL(&s_ring_lock);
        s_ring.Append(std::string_view(line, kept));
        portEXIT_CRITICAL(&s_ring_lock);
    }
    return s_original_vprintf != nullptr ? s_original_vprintf(format, args) : vprintf(format, args);
}

const char* ResetReasonName(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_PANIC: return "panic (crash)";
        case ESP_RST_INT_WDT: return "interrupt watchdog";
        case ESP_RST_TASK_WDT: return "task watchdog";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_BROWNOUT: return "brownout (power dipped)";
        case ESP_RST_SW: return "software restart";
        case ESP_RST_POWERON: return "power on";
        case ESP_RST_DEEPSLEEP: return "wake from deep sleep";
        default: return "other";
    }
}

bool IsCrash(esp_reset_reason_t reason)
{
    return reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT ||
           reason == ESP_RST_WDT || reason == ESP_RST_BROWNOUT;
}

std::string Join(const std::string& left, const std::string& right)
{
    return left.empty() || left.back() == '/' ? left + right : left + "/" + right;
}

std::vector<std::string> ListNames(const std::string& folder)
{
    std::vector<std::string> names;
    DIR* dir = opendir(folder.c_str());
    if (dir == nullptr) {
        return names;
    }
    while (dirent* entry = readdir(dir)) {
        names.emplace_back(entry->d_name);
    }
    closedir(dir);
    return names;
}

// Copies the core dump image out of flash into `path`. False if there's none or it fails.
bool SaveCoreDump(const std::string& path)
{
    size_t address = 0;
    size_t size = 0;
    if (esp_core_dump_image_get(&address, &size) != ESP_OK || size == 0) {
        return false;
    }
    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
    if (partition == nullptr || address < partition->address ||
        address + size > partition->address + partition->size) {
        return false;
    }
    FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        return false;
    }
    std::vector<uint8_t> buffer(4096);
    bool ok = true;
    for (size_t done = 0; ok && done < size; done += buffer.size()) {
        const size_t chunk = std::min(buffer.size(), size - done);
        ok = esp_partition_read(partition, address - partition->address + done, buffer.data(),
                                chunk) == ESP_OK &&
             std::fwrite(buffer.data(), 1, chunk, file) == chunk;
    }
    std::fclose(file);
    if (!ok) {
        unlink(path.c_str());
    }
    return ok;
}

std::string NowText()
{
    const time_t now = time(nullptr);
    tm local = {};
    localtime_r(&now, &local);
    if (local.tm_year + 1900 < 2024) {
        return {};  // clock not set yet
    }
    char text[32];
    std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M (after the restart)", &local);
    return text;
}

}  // namespace

void StartLogCapture()
{
    if (s_ring.capacity() != 0) {
        return;
    }
    if (s_ring.Attach(s_ring_memory, sizeof(s_ring_memory))) {
        s_previous_log = s_ring.Contents();
        s_ring.Clear();
    }
    s_original_vprintf = esp_log_set_vprintf(&CaptureVprintf);
}

int CollectAfterReset()
{
    if (!storage_service::IsMounted()) {
        return 0;
    }
    const std::string folder = Join(Join(storage_service::MountPoint(), ".keepsake"), "crash");
    const esp_reset_reason_t reason = esp_reset_reason();
    size_t dump_address = 0;
    size_t dump_size = 0;
    const bool has_dump = esp_core_dump_image_get(&dump_address, &dump_size) == ESP_OK;

    std::vector<std::string> names = ListNames(folder);
    if (IsCrash(reason) || has_dump) {
        mkdir(Join(storage_service::MountPoint(), ".keepsake").c_str(), 0775);
        mkdir(folder.c_str(), 0775);
        const int number = crash_report::NextReportNumber(names);

        crash_report::Report report;
        report.firmware_version = esp_app_get_description()->version;
        report.reset_reason = ResetReasonName(reason);
        report.when = NowText();
        report.last_log = s_previous_log;
        if (has_dump) {
            report.has_core_dump = true;
            esp_core_dump_summary_t summary = {};
            if (esp_core_dump_get_summary(&summary) == ESP_OK) {
                report.panic_task = summary.exc_task;
                report.panic_pc = summary.exc_pc;
                for (uint32_t i = 0; i < summary.exc_bt_info.depth && i < 16; ++i) {
                    report.backtrace.push_back(summary.exc_bt_info.bt[i]);
                }
                report.backtrace_corrupted = summary.exc_bt_info.corrupted;
            }
            const std::string elf_name = crash_report::ReportName(number, "elf");
            if (SaveCoreDump(Join(folder, elf_name))) {
                report.core_dump_file = elf_name;
            }
        }
        const std::string text = crash_report::FormatReport(report);
        const std::string txt_name = crash_report::ReportName(number, "txt");
        if (FILE* file = std::fopen(Join(folder, txt_name).c_str(), "wb")) {
            std::fwrite(text.data(), 1, text.size(), file);
            std::fclose(file);
            ESP_LOGW(kTag, "Saved a crash report: %s (%s)", txt_name.c_str(),
                     report.reset_reason.c_str());
        } else {
            ESP_LOGE(kTag, "Couldn't write the crash report");
        }
        if (has_dump) {
            esp_core_dump_image_erase();  // so it's reported once
        }
        names = ListNames(folder);
    }

    for (const std::string& old : crash_report::ReportsToPrune(names, kKeepReports)) {
        unlink(Join(folder, old).c_str());
    }
    int count = 0;
    for (const std::string& name : ListNames(folder)) {
        count += name.size() > 4 && name.compare(name.size() - 4, 4, ".txt") == 0 ? 1 : 0;
    }
    return count;
}

}  // namespace crash_report_service

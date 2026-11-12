#include "test_console_runtime.h"

#include "sdkconfig.h"

#if CONFIG_KEEPSAKE_TEST_CONSOLE

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "board_test/board_test.h"
#include "button_service.h"
#include "chroninkle_task_config.h"
#include "climate_service.h"
#include "display_service.h"
#include "driver/usb_serial_jtag.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "imu_service.h"
#include "nvs.h"
#include "power_service.h"
#include "storage_service.h"
#include "wifi_service.h"

namespace test_console_runtime {
namespace {

constexpr const char* kTag = "TestConsole";
constexpr uint32_t kStackBytes = 6144;
constexpr size_t kMaxLineBytes = 1024;
constexpr size_t kDataChunkBytes = 512;  // bytes per @@KSTD line, before base64
constexpr size_t kMaxFileBytes = 16 * 1024;

using board_test::JsonWriter;

void Reply(const std::string& json)
{
    // One printf per line, so a log line from another task can't land in the middle.
    std::printf("%s%s\n", std::string(board_test::kReplyPrefix).c_str(), json.c_str());
    std::fflush(stdout);
}

void ReplyError(const std::string& command, const std::string& message)
{
    Reply(JsonWriter().Add("cmd", command).Add("ok", false).Add("error", message).Finish());
}

void SendData(const std::vector<uint8_t>& data)
{
    size_t index = 0;
    for (size_t offset = 0; offset < data.size(); offset += kDataChunkBytes, ++index) {
        const size_t chunk = std::min(kDataChunkBytes, data.size() - offset);
        const std::string encoded = board_test::Base64Encode(data.data() + offset, chunk);
        std::printf("%s%u %s\n", std::string(board_test::kDataPrefix).c_str(),
                    static_cast<unsigned>(index), encoded.c_str());
        std::fflush(stdout);
        if (index % 16 == 15) {
            vTaskDelay(pdMS_TO_TICKS(5));  // let the USB FIFO drain
        }
    }
}

size_t DataLineCount(size_t bytes)
{
    return (bytes + kDataChunkBytes - 1) / kDataChunkBytes;
}

std::string CardPath(const std::string& path)
{
    return std::string(storage_service::MountPoint()) + path;
}

// --- self-test --------------------------------------------------------------------------

struct Check {
    std::string name;
    std::string status;  // "pass", "warn", "fail", "skip"
    std::string detail;
};

std::string ChecksJson(const std::vector<Check>& checks)
{
    std::string out = "[";
    for (size_t i = 0; i < checks.size(); ++i) {
        out += (i ? "," : "") + JsonWriter()
                                    .Add("name", checks[i].name)
                                    .Add("status", checks[i].status)
                                    .Add("detail", checks[i].detail)
                                    .Finish();
    }
    return out + "]";
}

std::string Format(const char* format, ...)
{
    char text[160];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    return text;
}

Check CheckFirmware()
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* coredump = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
    const std::string detail =
        Format("%s on %s, coredump partition %s", esp_app_get_description()->version,
               running != nullptr ? running->label : "?", coredump != nullptr ? "yes" : "missing");
    return {"firmware", coredump != nullptr ? "pass" : "fail", detail};
}

Check CheckMemory()
{
    const size_t internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t psram = esp_psram_is_initialized() ? esp_psram_get_size() : 0;
    const size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const bool ok = psram >= 4u * 1024u * 1024u && internal >= 30u * 1024u;
    return {"memory", ok ? "pass" : "fail",
            Format("internal free %u KB (largest %u KB), PSRAM %u KB (%u KB free)",
                   static_cast<unsigned>(internal / 1024), static_cast<unsigned>(largest / 1024),
                   static_cast<unsigned>(psram / 1024), static_cast<unsigned>(psram_free / 1024))};
}

Check CheckNvs()
{
    nvs_handle_t handle = 0;
    if (nvs_open("kst_selftest", NVS_READWRITE, &handle) != ESP_OK) {
        return {"nvs", "fail", "can't open"};
    }
    uint32_t value = 0;
    const bool ok = nvs_set_u32(handle, "probe", 0xC0FFEE) == ESP_OK && nvs_commit(handle) == ESP_OK &&
                    nvs_get_u32(handle, "probe", &value) == ESP_OK && value == 0xC0FFEE;
    nvs_erase_all(handle);
    nvs_commit(handle);
    nvs_close(handle);
    return {"nvs", ok ? "pass" : "fail", ok ? "write/read ok" : "write/read mismatch"};
}

Check CheckPower()
{
    power_service::Status status = {};
    if (power_service::ReadStatus(&status) != ESP_OK || !status.initialized) {
        return {"pmic", "fail", "AXP2101 status unreadable"};
    }
    const auto& battery = status.battery;
    std::string detail = Format("usb %s, charger %s", status.usb_detected ? "yes" : "no",
                                status.charger_enabled ? "on" : "off");
    if (battery.available) {
        detail += Format(", battery %u%% %u mV", battery.state_of_charge_percent,
                         battery.voltage_mv);
    } else {
        detail += ", no battery reading";
    }
    return {"pmic", battery.available ? "pass" : "warn", detail};
}

Check CheckRtc()
{
    std::tm first = {};
    if (power_service::ReadRtcTime(&first) != ESP_OK) {
        return {"rtc", "fail", "PCF85063 unreadable"};
    }
    vTaskDelay(pdMS_TO_TICKS(1100));
    std::tm second = {};
    power_service::ReadRtcTime(&second);
    const bool ticking = first.tm_sec != second.tm_sec;
    const bool set = first.tm_year + 1900 >= 2024;
    char when[32];
    std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", &second);
    return {"rtc", !ticking ? "fail" : (set ? "pass" : "warn"),
            std::string(when) + (ticking ? ", ticking" : ", NOT ticking") +
                (set ? "" : ", not set yet")};
}

Check CheckImu()
{
    if (!imu_service::IsInitialized()) {
        return {"imu", "fail", "QMI8658 not initialised"};
    }
    imu_service::ImuSample sample = {};
    if (imu_service::ReadSample(&sample) != ESP_OK) {
        return {"imu", "fail", "read failed"};
    }
    const float g = std::sqrt(sample.accel_x_g * sample.accel_x_g +
                              sample.accel_y_g * sample.accel_y_g +
                              sample.accel_z_g * sample.accel_z_g);
    const bool ok = g > 0.7f && g < 1.3f;
    return {"imu", ok ? "pass" : "fail", Format("|a| = %.2f g (expect ~1 at rest)", g)};
}

Check CheckClimate()
{
    climate_service::Reading reading = {};
    if (climate_service::Read(&reading) != ESP_OK) {
        return {"climate", "warn", "SHTC3 unreadable (sensor rail? see #35)"};
    }
    const bool ok = reading.temperature_c > -10 && reading.temperature_c < 60 &&
                    reading.relative_humidity >= 0 && reading.relative_humidity <= 100;
    return {"climate", ok ? "pass" : "fail",
            Format("%.1f C, %.0f%% RH", reading.temperature_c, reading.relative_humidity)};
}

Check CheckSdCard()
{
    if (!storage_service::IsMounted()) {
        return {"sd", "fail", "not mounted (card in?)"};
    }
    const std::string folder = CardPath("/.keepsake");
    mkdir(folder.c_str(), 0775);
    const std::string path = folder + "/selftest.tmp";
    const char kProbe[] = "keepsake selftest";
    bool ok = false;
    if (FILE* file = std::fopen(path.c_str(), "wb")) {
        ok = std::fwrite(kProbe, 1, sizeof(kProbe), file) == sizeof(kProbe);
        std::fclose(file);
    }
    char back[sizeof(kProbe)] = {};
    if (ok) {
        if (FILE* file = std::fopen(path.c_str(), "rb")) {
            ok = std::fread(back, 1, sizeof(back), file) == sizeof(back) &&
                 std::memcmp(back, kProbe, sizeof(kProbe)) == 0;
            std::fclose(file);
        } else {
            ok = false;
        }
    }
    unlink(path.c_str());
    storage_service::StorageStats stats = {};
    std::string detail = ok ? "write/read ok" : "write/read failed";
    if (storage_service::GetStorageStats(&stats)) {
        detail += Format(", %llu MB free of %llu MB",
                         static_cast<unsigned long long>(stats.free_bytes / (1024 * 1024)),
                         static_cast<unsigned long long>(stats.total_bytes / (1024 * 1024)));
    }
    return {"sd", ok ? "pass" : "fail", detail};
}

Check CheckDisplay()
{
    if (!display_service::IsInitialized()) {
        return {"display", "fail", "not initialised"};
    }
    std::vector<uint8_t> frame;
    int width = 0;
    int height = 0;
    const bool copied = display_service::CopyFramebuffer(&frame, &width, &height);
    size_t black = 0;
    for (const uint8_t byte : frame) {
        black += static_cast<size_t>(__builtin_popcount(static_cast<uint8_t>(~byte)));
    }
    const bool drawn = copied && black > 0;
    return {"display", drawn ? "pass" : "fail",
            Format("%dx%d, screen %s, %u black pixels", width, height,
                   display_service::ScreenName(display_service::GetCurrentScreen()),
                   static_cast<unsigned>(black))};
}

Check CheckWifi()
{
    const wifi_service::UiState state = wifi_service::GetUiState();
    const std::vector<std::string> saved = wifi_service::SavedNetworks();
    std::string detail = Format("%s, %u saved network(s)",
                                state.connected ? "connected" : (state.wifi_enabled ? "not connected" : "off"),
                                static_cast<unsigned>(saved.size()));
    if (state.connected) {
        detail += Format(", rssi %d dBm", state.rssi);
    }
    // Not connected is fine for a board on a desk with no network saved.
    return {"wifi", state.connected || saved.empty() ? "pass" : "warn", detail};
}

void CommandSelftest()
{
    std::vector<Check> checks = {CheckFirmware(), CheckMemory(), CheckNvs(),     CheckPower(),
                                 CheckRtc(),      CheckImu(),    CheckClimate(), CheckSdCard(),
                                 CheckDisplay(),  CheckWifi()};
    bool ok = true;
    for (const Check& check : checks) {
        ok = ok && check.status != "fail";
    }
    Reply(JsonWriter().Add("cmd", "selftest").Add("ok", ok).AddRaw("checks", ChecksJson(checks)).Finish());
}

// --- other commands -----------------------------------------------------------------------

void CommandVersion()
{
    const esp_app_desc_t* app = esp_app_get_description();
    const esp_partition_t* running = esp_ota_get_running_partition();
    Reply(JsonWriter()
              .Add("cmd", "version")
              .Add("ok", true)
              .Add("version", app->version)
              .Add("idf", app->idf_ver)
              .Add("built", std::string(app->date) + " " + app->time)
              .Add("partition", running != nullptr ? running->label : "?")
              .Add("reset_reason", static_cast<int>(esp_reset_reason()))
              .Add("uptime_ms", static_cast<int64_t>(esp_timer_get_time() / 1000))
              .Finish());
}

void CommandScreen()
{
    Reply(JsonWriter()
              .Add("cmd", "screen")
              .Add("ok", true)
              .Add("screen", display_service::ScreenName(display_service::GetCurrentScreen()))
              .Finish());
}

void CommandScreenshot()
{
    std::vector<uint8_t> frame;
    int width = 0;
    int height = 0;
    if (!display_service::CopyFramebuffer(&frame, &width, &height)) {
        ReplyError("screenshot", "display not ready");
        return;
    }
    Reply(JsonWriter()
              .Add("cmd", "screenshot")
              .Add("ok", true)
              .Add("format", "raw1-landscape-white1")
              .Add("width", width)
              .Add("height", height)
              .Add("screen", display_service::ScreenName(display_service::GetCurrentScreen()))
              .Add("bytes", static_cast<int64_t>(frame.size()))
              .Add("data_lines", static_cast<int64_t>(DataLineCount(frame.size())))
              .Finish());
    SendData(frame);
}

bool ParseButton(const std::string& name, button_service::ButtonId* out)
{
    if (name == "action" || name == "record" || name == "back") *out = button_service::ButtonId::kAction;
    else if (name == "fn" || name == "select" || name == "press") *out = button_service::ButtonId::kFunction;
    else if (name == "up") *out = button_service::ButtonId::kUp;
    else if (name == "down") *out = button_service::ButtonId::kDown;
    else return false;
    return true;
}

void CommandPress(const std::vector<std::string>& args)
{
    button_service::ButtonId button = button_service::ButtonId::kAction;
    if (args.size() < 2 || !ParseButton(args[1], &button)) {
        ReplyError("press", "usage: press <action|fn|up|down> [click|hold|double]");
        return;
    }
    const std::string how = args.size() > 2 ? args[2] : "click";
    using button_service::ButtonEvent;
    const auto send = [button](ButtonEvent event, uint32_t pressed_ms) {
        button_service::InjectEvent({.button = button, .event = event, .pressed_ms = pressed_ms});
    };
    if (how == "click") {
        send(ButtonEvent::kPressDown, 0);
        vTaskDelay(pdMS_TO_TICKS(80));
        send(ButtonEvent::kPressUp, 80);
        send(ButtonEvent::kSingleClick, 80);
    } else if (how == "double") {
        send(ButtonEvent::kPressDown, 0);
        send(ButtonEvent::kPressUp, 60);
        send(ButtonEvent::kPressDown, 0);
        send(ButtonEvent::kPressRepeat, 0);
        send(ButtonEvent::kPressUp, 60);
        send(ButtonEvent::kDoubleClick, 60);
    } else if (how == "hold") {
        send(ButtonEvent::kPressDown, 0);
        vTaskDelay(pdMS_TO_TICKS(600));
        send(ButtonEvent::kLongPressStart, 600);
        vTaskDelay(pdMS_TO_TICKS(200));
        send(ButtonEvent::kPressUp, 800);
        send(ButtonEvent::kLongPressUp, 800);
    } else {
        ReplyError("press", "how must be click, hold or double");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    Reply(JsonWriter().Add("cmd", "press").Add("ok", true).Add("button", args[1]).Add("how", how)
              .Add("screen", display_service::ScreenName(display_service::GetCurrentScreen()))
              .Finish());
}

void CommandList(const std::vector<std::string>& args)
{
    const std::string path = args.size() > 1 ? args[1] : "/";
    if (!board_test::IsSafeCardPath(path) || !storage_service::IsMounted()) {
        ReplyError("ls", "bad path or no card");
        return;
    }
    DIR* dir = opendir(CardPath(path).c_str());
    if (dir == nullptr) {
        ReplyError("ls", "no such folder");
        return;
    }
    std::string entries = "[";
    size_t count = 0;
    while (dirent* entry = readdir(dir)) {
        if (count >= 200) {
            break;
        }
        struct stat st = {};
        const std::string full = CardPath(path) + "/" + entry->d_name;
        const bool is_dir = stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
        entries += (count++ ? "," : "") + JsonWriter()
                                              .Add("name", entry->d_name)
                                              .Add("dir", is_dir)
                                              .Add("size", static_cast<int64_t>(is_dir ? 0 : st.st_size))
                                              .Finish();
    }
    closedir(dir);
    Reply(JsonWriter().Add("cmd", "ls").Add("ok", true).Add("path", path).AddRaw("entries", entries + "]").Finish());
}

void CommandGet(const std::vector<std::string>& args)
{
    if (args.size() < 2 || !board_test::IsSafeCardPath(args[1]) || !storage_service::IsMounted()) {
        ReplyError("get", "usage: get /path (on the card)");
        return;
    }
    FILE* file = std::fopen(CardPath(args[1]).c_str(), "rb");
    if (file == nullptr) {
        ReplyError("get", "no such file");
        return;
    }
    std::vector<uint8_t> data;
    uint8_t buffer[512];
    size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0 && data.size() < kMaxFileBytes) {
        data.insert(data.end(), buffer, buffer + read);
    }
    const bool truncated = !std::feof(file);
    std::fclose(file);
    Reply(JsonWriter().Add("cmd", "get").Add("ok", true).Add("path", args[1])
              .Add("bytes", static_cast<int64_t>(data.size())).Add("truncated", truncated)
              .Add("data_lines", static_cast<int64_t>(DataLineCount(data.size()))).Finish());
    SendData(data);
}

void CommandPut(const std::vector<std::string>& args)
{
    // put /path <base64>: small files only (provisioning, test books).
    std::vector<uint8_t> data;
    if (args.size() < 3 || !board_test::IsSafeCardPath(args[1]) || !storage_service::IsMounted() ||
        !board_test::Base64Decode(args[2], &data) || data.size() > kMaxFileBytes) {
        ReplyError("put", "usage: put /path <base64> (at most 16 KB, card mounted)");
        return;
    }
    // Make the parent folders.
    std::string folder;
    for (size_t slash = args[1].find('/', 1); slash != std::string::npos;
         slash = args[1].find('/', slash + 1)) {
        folder = CardPath(args[1].substr(0, slash));
        mkdir(folder.c_str(), 0775);
    }
    FILE* file = std::fopen(CardPath(args[1]).c_str(), "wb");
    const bool ok = file != nullptr && std::fwrite(data.data(), 1, data.size(), file) == data.size();
    if (file != nullptr) {
        std::fclose(file);
    }
    if (!ok) {
        ReplyError("put", "write failed");
        return;
    }
    Reply(JsonWriter().Add("cmd", "put").Add("ok", true).Add("path", args[1])
              .Add("bytes", static_cast<int64_t>(data.size())).Finish());
}

void CommandRemove(const std::vector<std::string>& args)
{
    if (args.size() < 2 || !board_test::IsSafeCardPath(args[1]) || args[1] == "/" ||
        !storage_service::IsMounted()) {
        ReplyError("rm", "usage: rm /path (a file on the card)");
        return;
    }
    const bool ok = unlink(CardPath(args[1]).c_str()) == 0;
    Reply(JsonWriter().Add("cmd", "rm").Add("ok", ok).Add("path", args[1]).Finish());
}

void CommandHelp()
{
    Reply(JsonWriter()
              .Add("cmd", "help")
              .Add("ok", true)
              .AddRaw("commands",
                      R"(["help","version","selftest","screen","screenshot",)"
                      R"("press <action|fn|up|down> [click|hold|double]","ls [/path]",)"
                      R"("get /path","put /path <base64>","rm /path","reboot","crash yes"])")
              .Finish());
}

void Run(const std::vector<std::string>& args)
{
    const std::string& command = args.empty() ? std::string() : args[0];
    if (command == "help" || command.empty()) CommandHelp();
    else if (command == "version") CommandVersion();
    else if (command == "selftest") CommandSelftest();
    else if (command == "screen") CommandScreen();
    else if (command == "screenshot") CommandScreenshot();
    else if (command == "press") CommandPress(args);
    else if (command == "ls") CommandList(args);
    else if (command == "get") CommandGet(args);
    else if (command == "put") CommandPut(args);
    else if (command == "rm") CommandRemove(args);
    else if (command == "reboot") {
        Reply(JsonWriter().Add("cmd", "reboot").Add("ok", true).Finish());
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
    } else if (command == "crash") {
        if (args.size() < 2 || args[1] != "yes") {
            ReplyError("crash", "say \"crash yes\": this crashes the firmware on purpose (#41 test)");
            return;
        }
        Reply(JsonWriter().Add("cmd", "crash").Add("ok", true).Finish());
        vTaskDelay(pdMS_TO_TICKS(100));
        std::abort();
    } else {
        ReplyError(command, "unknown command (try help)");
    }
}

void ConsoleTask(void*)
{
    std::string line;
    uint8_t buffer[128];
    while (true) {
        const int got = usb_serial_jtag_read_bytes(buffer, sizeof(buffer), portMAX_DELAY);
        for (int i = 0; i < got; ++i) {
            const char c = static_cast<char>(buffer[i]);
            if (c != '\n' && c != '\r') {
                if (line.size() < kMaxLineBytes * 24) {  // room for a 16 KB file in base64
                    line.push_back(c);
                }
                continue;
            }
            std::vector<std::string> words;
            if (board_test::ParseCommandLine(line, &words)) {
                Run(words);
            }
            line.clear();
        }
    }
}

}  // namespace

void Start()
{
    usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    config.rx_buffer_size = 2048;
    if (usb_serial_jtag_driver_install(&config) != ESP_OK) {
        ESP_LOGW(kTag, "USB serial driver unavailable; board test console off");
        return;
    }
    if (xTaskCreatePinnedToCore(ConsoleTask, "test_console", kStackBytes, nullptr,
                                chroninkle_task_config::kPriorityTestConsole, nullptr,
                                chroninkle_task_config::kSystemCore) != pdPASS) {
        ESP_LOGW(kTag, "Couldn't start the board test console");
        return;
    }
    ESP_LOGI(kTag, "Board test console ready (send \"kst help\")");
}

}  // namespace test_console_runtime

#else

namespace test_console_runtime {
void Start() {}
}  // namespace test_console_runtime

#endif

#include "crash_report/crash_report.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace crash_report {

struct LogRing::Header {
    uint32_t magic;
    uint32_t capacity;  // must match the block, or it's not ours
    uint32_t head;      // next write position
    uint32_t used;      // bytes held, <= capacity
};
static_assert(sizeof(uint32_t) * 4 == LogRing::kHeaderBytes, "header layout");

namespace {

constexpr uint32_t kMagic = 0x4B4C4F47;  // "KLOG"

bool ParseNumber(const std::string& name, int* number)
{
    // "crash-NNN.ext"
    if (name.size() < 10 || name.compare(0, 6, "crash-") != 0 || name[9] != '.') {
        return false;
    }
    int value = 0;
    for (size_t i = 6; i < 9; ++i) {
        if (name[i] < '0' || name[i] > '9') {
            return false;
        }
        value = value * 10 + (name[i] - '0');
    }
    if (value < 1) {
        return false;
    }
    *number = value;
    return true;
}

}  // namespace

LogRing::Header* LogRing::header() const
{
    return reinterpret_cast<Header*>(memory_);
}

bool LogRing::Attach(uint8_t* memory, size_t bytes)
{
    memory_ = nullptr;
    text_ = nullptr;
    capacity_ = 0;
    if (memory == nullptr || bytes <= kHeaderBytes) {
        return false;
    }
    memory_ = memory;
    text_ = reinterpret_cast<char*>(memory + kHeaderBytes);
    capacity_ = bytes - kHeaderBytes;
    Header* h = header();
    const bool valid = h->magic == kMagic && h->capacity == capacity_ && h->head < capacity_ &&
                       h->used <= capacity_;
    if (!valid) {
        Clear();
    }
    return valid;
}

void LogRing::Clear()
{
    if (memory_ == nullptr) {
        return;
    }
    Header* h = header();
    h->magic = kMagic;
    h->capacity = static_cast<uint32_t>(capacity_);
    h->head = 0;
    h->used = 0;
}

void LogRing::Append(std::string_view text)
{
    if (memory_ == nullptr || text.empty()) {
        return;
    }
    if (text.size() > capacity_) {
        text = text.substr(text.size() - capacity_);  // only the newest part fits
    }
    Header* h = header();
    size_t head = h->head;
    for (const char c : text) {
        text_[head] = c;
        head = head + 1 == capacity_ ? 0 : head + 1;
    }
    h->head = static_cast<uint32_t>(head);
    h->used = static_cast<uint32_t>(std::min(capacity_, static_cast<size_t>(h->used) + text.size()));
}

std::string LogRing::Contents() const
{
    if (memory_ == nullptr) {
        return {};
    }
    const Header* h = header();
    std::string out;
    out.reserve(h->used);
    size_t start = (h->head + capacity_ - h->used) % capacity_;
    for (size_t i = 0; i < h->used; ++i) {
        out.push_back(text_[(start + i) % capacity_]);
    }
    return out;
}

std::string FormatReport(const Report& report)
{
    std::string out = "Keepsake crash report\n=====================\n\n";
    const auto line = [&out](const char* label, const std::string& value) {
        if (!value.empty()) {
            out += std::string(label) + ": " + value + "\n";
        }
    };
    line("Firmware", report.firmware_version);
    line("Reset reason", report.reset_reason);
    line("When", report.when);
    if (report.has_core_dump) {
        line("Crashed task", report.panic_task);
        char pc[16];
        std::snprintf(pc, sizeof(pc), "0x%08lx", static_cast<unsigned long>(report.panic_pc));
        line("PC", pc);
        if (!report.backtrace.empty()) {
            std::string bt;
            for (const uint32_t address : report.backtrace) {
                char hex[16];
                std::snprintf(hex, sizeof(hex), "0x%08lx", static_cast<unsigned long>(address));
                bt += (bt.empty() ? "" : " ") + std::string(hex);
            }
            if (report.backtrace_corrupted) {
                bt += " (corrupted)";
            }
            line("Backtrace", bt);
        }
        line("Core dump", report.core_dump_file.empty() ? "not saved" : report.core_dump_file);
        out += "\nTo decode: build this exact firmware version, then\n"
               "  xtensa-esp32s3-elf-addr2line -pfiaC -e build/keepsake.elf <addresses above>\n";
        if (!report.core_dump_file.empty()) {
            out += "  idf.py coredump-info -c " + report.core_dump_file + "\n";
        }
    }
    out += "\nLast log lines (oldest first)\n-----------------------------\n";
    out += report.last_log.empty() ? "(none kept)\n" : report.last_log;
    if (!out.empty() && out.back() != '\n') {
        out.push_back('\n');
    }
    return out;
}

std::string ReportName(int number, std::string_view extension)
{
    char name[24];
    std::snprintf(name, sizeof(name), "crash-%03d", number);
    return std::string(name) + "." + std::string(extension);
}

int NextReportNumber(const std::vector<std::string>& existing_names)
{
    int newest = 0;
    for (const std::string& name : existing_names) {
        int number = 0;
        if (ParseNumber(name, &number)) {
            newest = std::max(newest, number);
        }
    }
    return std::min(newest + 1, 999);  // past 999, the newest report is overwritten
}

std::vector<std::string> ReportsToPrune(const std::vector<std::string>& existing_names, size_t keep)
{
    std::vector<int> numbers;
    for (const std::string& name : existing_names) {
        int number = 0;
        if (ParseNumber(name, &number) &&
            std::find(numbers.begin(), numbers.end(), number) == numbers.end()) {
            numbers.push_back(number);
        }
    }
    std::sort(numbers.begin(), numbers.end());
    std::vector<std::string> prune;
    if (numbers.size() <= keep) {
        return prune;
    }
    const std::vector<int> old(numbers.begin(), numbers.end() - static_cast<long>(keep));
    for (const std::string& name : existing_names) {
        int number = 0;
        if (ParseNumber(name, &number) && std::find(old.begin(), old.end(), number) != old.end()) {
            prune.push_back(name);
        }
    }
    return prune;
}

}  // namespace crash_report

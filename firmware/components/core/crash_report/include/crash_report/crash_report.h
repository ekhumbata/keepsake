#ifndef CRASH_REPORT_CRASH_REPORT_H_
#define CRASH_REPORT_CRASH_REPORT_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Crash reports (#41) for Eric, who can't watch the serial log on a device he doesn't have.
// Portable: crash_report_service keeps the ring in RAM that survives a reset, and writes the
// report to the SD card on the next boot.
namespace crash_report {

// The most recent log output, in a fixed block of memory that isn't cleared on a reset (only
// on power-up). After a reset Attach() checks the block still holds a ring; anything that
// doesn't add up (power-on garbage) is reset to empty rather than trusted.
class LogRing {
public:
    // The block's layout: a small header, then the text. `bytes` must be > kHeaderBytes.
    static constexpr size_t kHeaderBytes = 16;

    // Takes over `memory` (bytes long). Returns true when it already held a valid ring (what
    // was logged before the reset), false when it was reset to empty.
    bool Attach(uint8_t* memory, size_t bytes);

    void Append(std::string_view text);
    // Everything kept, oldest first.
    std::string Contents() const;
    void Clear();

    size_t capacity() const { return capacity_; }

private:
    struct Header;
    Header* header() const;

    uint8_t* memory_ = nullptr;
    char* text_ = nullptr;
    size_t capacity_ = 0;
};

struct Report {
    std::string firmware_version;
    std::string reset_reason;   // "panic", "task watchdog", "brownout" ...
    std::string when;           // local time if known, else ""
    bool has_core_dump = false;
    std::string core_dump_file;  // file name next to the report, if saved
    std::string panic_task;
    uint32_t panic_pc = 0;
    std::vector<uint32_t> backtrace;
    bool backtrace_corrupted = false;
    std::string last_log;       // from the LogRing
};

// The report as plain text: a short summary, how to decode the backtrace, then the log.
std::string FormatReport(const Report& report);

// Report files are "crash-NNN.txt" (NNN = 001..999; past that, 999 is reused). Given the names in
// the folder, the next number to use; and which old ones to delete to keep at most `keep`.
int NextReportNumber(const std::vector<std::string>& existing_names);
std::vector<std::string> ReportsToPrune(const std::vector<std::string>& existing_names, size_t keep);
std::string ReportName(int number, std::string_view extension);  // ("crash-007", "txt")

}  // namespace crash_report

#endif  // CRASH_REPORT_CRASH_REPORT_H_

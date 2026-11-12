// Crash reports (#41): the log ring that survives a reset, report text and file names.
#include <doctest/doctest.h>

#include <cstring>
#include <string>
#include <vector>

#include "crash_report/crash_report.h"

using crash_report::LogRing;

TEST_CASE("the log ring keeps the newest text and survives a re-attach")
{
    std::vector<uint8_t> memory(LogRing::kHeaderBytes + 16, 0xA5);  // power-on garbage
    LogRing ring;
    CHECK_FALSE(ring.Attach(memory.data(), memory.size()));  // garbage isn't trusted
    CHECK(ring.Contents().empty());
    CHECK(ring.capacity() == 16);

    ring.Append("hello ");
    ring.Append("world");
    CHECK(ring.Contents() == "hello world");
    ring.Append(" and more text");  // wraps: only the newest 16 bytes stay
    CHECK(ring.Contents() == "ld and more text");

    // "After the reset": the same memory, a fresh object.
    LogRing after;
    CHECK(after.Attach(memory.data(), memory.size()));
    CHECK(after.Contents() == ring.Contents());

    after.Append(std::string(100, 'x'));  // longer than the whole ring
    CHECK(after.Contents() == std::string(16, 'x'));
    after.Clear();
    CHECK(after.Contents().empty());
}

TEST_CASE("a ring from a different size block is not trusted")
{
    std::vector<uint8_t> memory(LogRing::kHeaderBytes + 32, 0);
    LogRing ring;
    ring.Attach(memory.data(), memory.size());
    ring.Append("kept");
    LogRing smaller;
    CHECK_FALSE(smaller.Attach(memory.data(), memory.size() - 8));
    CHECK(smaller.Contents().empty());
    LogRing none;
    CHECK_FALSE(none.Attach(nullptr, 100));
    none.Append("ignored");
    CHECK(none.Contents().empty());
}

TEST_CASE("the report reads well and says how to decode it")
{
    crash_report::Report report;
    report.firmware_version = "1.16.0";
    report.reset_reason = "panic";
    report.has_core_dump = true;
    report.core_dump_file = "crash-004.elf";
    report.panic_task = "net_jobs";
    report.panic_pc = 0x4200abcd;
    report.backtrace = {0x4200abcd, 0x42001234};
    report.last_log = "I (100) Main: hello\nE (200) Oops: bad\n";
    const std::string text = crash_report::FormatReport(report);
    CHECK(text.find("Firmware: 1.16.0") != std::string::npos);
    CHECK(text.find("Crashed task: net_jobs") != std::string::npos);
    CHECK(text.find("PC: 0x4200abcd") != std::string::npos);
    CHECK(text.find("Backtrace: 0x4200abcd 0x42001234") != std::string::npos);
    CHECK(text.find("coredump-info -c crash-004.elf") != std::string::npos);
    CHECK(text.find("E (200) Oops: bad") != std::string::npos);

    crash_report::Report brownout;
    brownout.reset_reason = "brownout";
    const std::string short_text = crash_report::FormatReport(brownout);
    CHECK(short_text.find("Reset reason: brownout") != std::string::npos);
    CHECK(short_text.find("Backtrace") == std::string::npos);
    CHECK(short_text.find("(none kept)") != std::string::npos);
}

TEST_CASE("report files are numbered and old ones pruned")
{
    CHECK(crash_report::ReportName(7, "txt") == "crash-007.txt");
    CHECK(crash_report::NextReportNumber({}) == 1);
    const std::vector<std::string> names = {"crash-001.txt", "crash-001.elf", "crash-002.txt",
                                            "crash-003.txt", "notes.txt",     "crash-abc.txt"};
    CHECK(crash_report::NextReportNumber(names) == 4);
    const auto prune = crash_report::ReportsToPrune(names, 2);
    CHECK(prune == std::vector<std::string>{"crash-001.txt", "crash-001.elf"});
    CHECK(crash_report::ReportsToPrune(names, 5).empty());
    CHECK(crash_report::NextReportNumber({"crash-999.txt"}) == 999);
}

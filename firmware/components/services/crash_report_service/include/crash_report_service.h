#ifndef CRASH_REPORT_SERVICE_H_
#define CRASH_REPORT_SERVICE_H_

// Crash reports on the SD card (#41), for Eric to collect over USB. A crash leaves a core dump
// in flash (#39) and the last log lines in RAM that survives the reset; the next boot writes
// them to /.keepsake/crash/crash-NNN.txt (+ .elf) and erases the dump. Only the last 10 reports
// are kept. Nothing is shown to the friend.
namespace crash_report_service {

// Call first thing at boot: keeps what was logged before the reset, then starts copying new
// log lines into the ring.
void StartLogCapture();

// Call once the SD card is mounted. Writes a report when the last reset was a crash (panic,
// watchdog, brownout) or a core dump is waiting. Returns how many reports are on the card.
int CollectAfterReset();

}  // namespace crash_report_service

#endif  // CRASH_REPORT_SERVICE_H_

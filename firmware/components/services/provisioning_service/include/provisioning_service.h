#ifndef PROVISIONING_SERVICE_H_
#define PROVISIONING_SERVICE_H_

#include <string>

// Imports the provisioning file (#42) from the SD card: /.keepsake/provision.json, written by
// Eric on a computer (see provision.example.json). Run once at boot, after the SD card is
// mounted and the Wi-Fi, timezone and Gemini services are initialised, but before Wi-Fi starts.
//
// A good file is applied (Wi-Fi network, timezone, Gemini key, personal settings), then
// overwritten and deleted so no passwords stay on the card. A refused file is left in place
// with /.keepsake/provision-problems.txt next to it explaining why, and nothing is imported.
namespace provisioning_service {

enum class Status {
    kNoFile,     // nothing to do (the usual case)
    kImported,   // applied and wiped
    kRefused,    // invalid: see provision-problems.txt
    kError,      // couldn't read, apply or wipe it (see the log)
};

struct Outcome {
    Status status = Status::kNoFile;
    // For a toast: "All set up!" / "The setup file has a problem" ... Empty for kNoFile.
    std::string message;
};

Outcome ImportFromSdCard();

}  // namespace provisioning_service

#endif  // PROVISIONING_SERVICE_H_

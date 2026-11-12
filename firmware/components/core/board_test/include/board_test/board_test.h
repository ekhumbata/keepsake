#ifndef BOARD_TEST_BOARD_TEST_H_
#define BOARD_TEST_BOARD_TEST_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The board test console's wire format. tools/board/ (on the computer the board is plugged
// into) sends one command per line over the USB serial port and reads framed replies out of
// the log stream:
//
//   computer -> board:  kst <command> [args...]        (args may be "quoted")
//   board -> computer:  @@KST {json}                    one reply per command
//                       @@KSTD <index> <base64>         data lines, when the reply says so
//
// The prefixes keep replies apart from ordinary log lines.
namespace board_test {

inline constexpr std::string_view kCommandPrefix = "kst";
inline constexpr std::string_view kReplyPrefix = "@@KST ";
inline constexpr std::string_view kDataPrefix = "@@KSTD ";

// Splits a command line into words ("quoted words" may hold spaces; \" and \\ escape). False
// when the line isn't for us (no "kst" first word) or a quote is left open.
bool ParseCommandLine(std::string_view line, std::vector<std::string>* words);

// Builds one JSON object, in order. Strings are escaped; non-finite numbers become null.
class JsonWriter {
public:
    JsonWriter& Add(std::string_view key, std::string_view value);
    JsonWriter& Add(std::string_view key, const char* value) { return Add(key, std::string_view(value)); }
    JsonWriter& Add(std::string_view key, bool value);
    JsonWriter& Add(std::string_view key, int64_t value);
    JsonWriter& Add(std::string_view key, int value) { return Add(key, static_cast<int64_t>(value)); }
    JsonWriter& Add(std::string_view key, double value);
    // `json` is already valid JSON (an object or array built elsewhere).
    JsonWriter& AddRaw(std::string_view key, std::string_view json);
    std::string Finish() const { return body_ + "}"; }

private:
    void Key(std::string_view key);
    std::string body_ = "{";
    bool first_ = true;
};

std::string JsonEscape(std::string_view text);

std::string Base64Encode(const uint8_t* data, size_t size);
bool Base64Decode(std::string_view text, std::vector<uint8_t>* out);

// A path the console may read or write on the SD card: absolute ("/..."), no "..", no "//",
// printable ASCII, at most 128 characters.
bool IsSafeCardPath(std::string_view path);

}  // namespace board_test

#endif  // BOARD_TEST_BOARD_TEST_H_

#ifndef EPUB_ENGINE_INFLATE_H_
#define EPUB_ENGINE_INFLATE_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace epub {

enum class InflateStatus {
    kOk,
    kCorrupt,   // malformed or truncated stream
    kTooLarge,  // output would exceed max_output
};

// Decompresses a raw DEFLATE stream (RFC 1951, no zlib/gzip header), as stored in zip
// entries with method 8. Appends to *out. Never reads past `input + input_length` and never
// writes more than `max_output` bytes, whatever the input.
InflateStatus InflateRaw(const uint8_t* input, size_t input_length, size_t max_output,
                         std::string* out);

// CRC-32 (IEEE 802.3, the zip checksum). Pass the previous value to continue a running CRC.
uint32_t Crc32(const void* data, size_t length, uint32_t crc = 0);

}  // namespace epub

#endif  // EPUB_ENGINE_INFLATE_H_

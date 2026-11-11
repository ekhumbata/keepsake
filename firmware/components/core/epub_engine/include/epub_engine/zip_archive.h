#ifndef EPUB_ENGINE_ZIP_ARCHIVE_H_
#define EPUB_ENGINE_ZIP_ARCHIVE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "epub_engine/byte_source.h"

namespace epub {

struct ZipEntry {
    std::string name;
    uint16_t method = 0;  // 0 = stored, 8 = deflate
    bool encrypted = false;
    uint32_t crc32 = 0;
    uint32_t compressed_size = 0;
    uint32_t uncompressed_size = 0;
    uint32_t local_header_offset = 0;
};

enum class ZipStatus {
    kOk,
    kNotZip,       // no end-of-central-directory record
    kCorrupt,      // inconsistent headers, bad CRC, bad compressed data
    kUnsupported,  // ZIP64, encryption or a compression method other than stored/deflate
    kTooLarge,     // the entry is bigger than the caller's limit
    kNotFound,
    kIoError,
};

// Reads the central directory of a zip file and extracts single entries into memory.
// Built for EPUBs: entries are small (chapters, OPF, images), ZIP64 is not supported, and
// every size and offset is checked against the source before it is used.
class ZipArchive {
public:
    // At most this many entries are read from the directory (a real EPUB has tens to a few
    // thousand); a bigger directory is treated as corrupt.
    static constexpr size_t kMaxEntries = 20000;

    ZipStatus Open(ByteSource* source);

    const std::vector<ZipEntry>& entries() const { return entries_; }
    // Exact, case-sensitive name match first, then an ASCII case-insensitive one (some
    // tools write "META-INF/Container.xml"; FAT-copied books lose case).
    const ZipEntry* Find(std::string_view name) const;

    // Decompresses `entry` into *out (replacing its contents) and checks its CRC.
    ZipStatus Read(const ZipEntry& entry, size_t max_size, std::string* out) const;
    ZipStatus Read(std::string_view name, size_t max_size, std::string* out) const;

private:
    ByteSource* source_ = nullptr;
    std::vector<ZipEntry> entries_;
};

const char* ZipStatusName(ZipStatus status);

}  // namespace epub

#endif  // EPUB_ENGINE_ZIP_ARCHIVE_H_

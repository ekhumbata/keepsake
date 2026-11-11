#include "epub_engine/zip_archive.h"

#include <algorithm>
#include <cctype>

#include "epub_engine/inflate.h"

namespace epub {
namespace {

constexpr uint32_t kEndOfCentralDirectorySignature = 0x06054b50;
constexpr uint32_t kCentralDirectorySignature = 0x02014b50;
constexpr uint32_t kLocalHeaderSignature = 0x04034b50;
constexpr size_t kEndOfCentralDirectorySize = 22;
constexpr size_t kCentralHeaderSize = 46;
constexpr size_t kLocalHeaderSize = 30;
constexpr size_t kMaxCommentSize = 0xFFFF;
// The central directory itself is read into memory in one go; cap it.
constexpr uint32_t kMaxCentralDirectorySize = 4u * 1024u * 1024u;

uint16_t Le16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t Le32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

}  // namespace

ZipStatus ZipArchive::Open(ByteSource* source)
{
    source_ = source;
    entries_.clear();
    if (source == nullptr) {
        return ZipStatus::kIoError;
    }
    const uint64_t size = source->Size();
    if (size < kEndOfCentralDirectorySize) {
        return ZipStatus::kNotZip;
    }

    // The end record sits in the last 22 bytes plus an optional comment of up to 64 KiB.
    const uint64_t tail_size = std::min<uint64_t>(size, kEndOfCentralDirectorySize + kMaxCommentSize);
    std::string tail(static_cast<size_t>(tail_size), '\0');
    if (!source->ReadAt(size - tail_size, tail.data(), tail.size())) {
        return ZipStatus::kIoError;
    }
    const auto* bytes = reinterpret_cast<const uint8_t*>(tail.data());
    size_t end_record = std::string::npos;
    for (size_t i = tail.size() - kEndOfCentralDirectorySize + 1; i-- > 0;) {
        if (Le32(bytes + i) == kEndOfCentralDirectorySignature) {
            end_record = i;
            break;
        }
    }
    if (end_record == std::string::npos) {
        return ZipStatus::kNotZip;
    }

    const uint8_t* eocd = bytes + end_record;
    const uint16_t entry_count = Le16(eocd + 10);
    const uint32_t directory_size = Le32(eocd + 12);
    const uint32_t directory_offset = Le32(eocd + 16);
    if (entry_count == 0xFFFF || directory_size == 0xFFFFFFFFu || directory_offset == 0xFFFFFFFFu) {
        return ZipStatus::kUnsupported;  // ZIP64
    }
    if (directory_size > kMaxCentralDirectorySize ||
        static_cast<uint64_t>(directory_offset) + directory_size > size) {
        return ZipStatus::kCorrupt;
    }

    std::string directory(directory_size, '\0');
    if (!source->ReadAt(directory_offset, directory.data(), directory.size())) {
        return ZipStatus::kIoError;
    }
    const auto* dir = reinterpret_cast<const uint8_t*>(directory.data());
    size_t offset = 0;
    entries_.reserve(std::min<size_t>(entry_count, kMaxEntries));
    for (uint32_t i = 0; i < entry_count; ++i) {
        if (entries_.size() >= kMaxEntries) {
            entries_.clear();
            return ZipStatus::kCorrupt;
        }
        if (directory.size() - offset < kCentralHeaderSize ||
            Le32(dir + offset) != kCentralDirectorySignature) {
            entries_.clear();
            return ZipStatus::kCorrupt;
        }
        const uint8_t* header = dir + offset;
        ZipEntry entry;
        const uint16_t flags = Le16(header + 8);
        entry.method = Le16(header + 10);
        entry.encrypted = (flags & 0x0001u) != 0;
        entry.crc32 = Le32(header + 16);
        entry.compressed_size = Le32(header + 20);
        entry.uncompressed_size = Le32(header + 24);
        const uint16_t name_length = Le16(header + 28);
        const uint16_t extra_length = Le16(header + 30);
        const uint16_t comment_length = Le16(header + 32);
        entry.local_header_offset = Le32(header + 42);
        const size_t record_size =
            kCentralHeaderSize + size_t{name_length} + extra_length + comment_length;
        if (directory.size() - offset < record_size) {
            entries_.clear();
            return ZipStatus::kCorrupt;
        }
        entry.name.assign(reinterpret_cast<const char*>(header + kCentralHeaderSize), name_length);
        offset += record_size;
        entries_.push_back(std::move(entry));
    }
    return ZipStatus::kOk;
}

const ZipEntry* ZipArchive::Find(std::string_view name) const
{
    for (const ZipEntry& entry : entries_) {
        if (entry.name == name) {
            return &entry;
        }
    }
    for (const ZipEntry& entry : entries_) {
        if (EqualsIgnoreCase(entry.name, name)) {
            return &entry;
        }
    }
    return nullptr;
}

ZipStatus ZipArchive::Read(std::string_view name, size_t max_size, std::string* out) const
{
    const ZipEntry* entry = Find(name);
    if (entry == nullptr) {
        return ZipStatus::kNotFound;
    }
    return Read(*entry, max_size, out);
}

ZipStatus ZipArchive::Read(const ZipEntry& entry, size_t max_size, std::string* out) const
{
    out->clear();
    if (source_ == nullptr) {
        return ZipStatus::kIoError;
    }
    if (entry.encrypted) {
        return ZipStatus::kUnsupported;
    }
    if (entry.method != 0 && entry.method != 8) {
        return ZipStatus::kUnsupported;
    }
    if (entry.uncompressed_size > max_size) {
        return ZipStatus::kTooLarge;
    }

    uint8_t local[kLocalHeaderSize] = {};
    if (!source_->ReadAt(entry.local_header_offset, local, sizeof(local))) {
        return ZipStatus::kCorrupt;
    }
    if (Le32(local) != kLocalHeaderSignature) {
        return ZipStatus::kCorrupt;
    }
    // The local name/extra lengths can differ from the central directory's copy.
    const uint64_t data_offset = uint64_t{entry.local_header_offset} + kLocalHeaderSize +
                                 Le16(local + 26) + Le16(local + 28);
    if (data_offset > source_->Size() ||
        entry.compressed_size > source_->Size() - data_offset) {
        return ZipStatus::kCorrupt;
    }

    if (entry.method == 0) {
        if (entry.compressed_size != entry.uncompressed_size) {
            return ZipStatus::kCorrupt;
        }
        out->resize(entry.uncompressed_size);
        if (!source_->ReadAt(data_offset, out->data(), out->size())) {
            out->clear();
            return ZipStatus::kIoError;
        }
    } else {
        std::string compressed(entry.compressed_size, '\0');
        if (!source_->ReadAt(data_offset, compressed.data(), compressed.size())) {
            return ZipStatus::kIoError;
        }
        out->reserve(entry.uncompressed_size);
        const InflateStatus status =
            InflateRaw(reinterpret_cast<const uint8_t*>(compressed.data()), compressed.size(),
                       entry.uncompressed_size, out);
        // The output limit is the declared size, so kTooLarge also means "lied about its size".
        if (status != InflateStatus::kOk || out->size() != entry.uncompressed_size) {
            out->clear();
            return ZipStatus::kCorrupt;
        }
    }

    if (Crc32(out->data(), out->size()) != entry.crc32) {
        out->clear();
        return ZipStatus::kCorrupt;
    }
    return ZipStatus::kOk;
}

const char* ZipStatusName(ZipStatus status)
{
    switch (status) {
        case ZipStatus::kOk:
            return "ok";
        case ZipStatus::kNotZip:
            return "not a zip file";
        case ZipStatus::kCorrupt:
            return "corrupt";
        case ZipStatus::kUnsupported:
            return "unsupported";
        case ZipStatus::kTooLarge:
            return "too large";
        case ZipStatus::kNotFound:
            return "not found";
        case ZipStatus::kIoError:
            return "I/O error";
    }
    return "unknown";
}

}  // namespace epub

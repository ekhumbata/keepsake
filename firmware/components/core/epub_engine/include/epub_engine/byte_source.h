#ifndef EPUB_ENGINE_BYTE_SOURCE_H_
#define EPUB_ENGINE_BYTE_SOURCE_H_

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace epub {

// Random-access read-only bytes: an EPUB file on the SD card, or a buffer in tests/fuzzers.
class ByteSource {
public:
    virtual ~ByteSource() = default;
    virtual uint64_t Size() const = 0;
    // Reads exactly `length` bytes at `offset`; false on a short read or I/O error.
    virtual bool ReadAt(uint64_t offset, void* destination, size_t length) = 0;
};

// Bytes held in memory (the source keeps its own copy).
class MemoryByteSource final : public ByteSource {
public:
    explicit MemoryByteSource(std::string bytes) : bytes_(std::move(bytes)) {}
    uint64_t Size() const override { return bytes_.size(); }
    bool ReadAt(uint64_t offset, void* destination, size_t length) override;

private:
    std::string bytes_;
};

// A file through stdio, which works the same on the host and on ESP-IDF's VFS/FATFS.
class FileByteSource final : public ByteSource {
public:
    // Returns nullptr-equivalent (is_open() == false) when the file can't be opened.
    explicit FileByteSource(const std::string& path);
    ~FileByteSource() override;
    FileByteSource(const FileByteSource&) = delete;
    FileByteSource& operator=(const FileByteSource&) = delete;

    bool is_open() const { return file_ != nullptr; }
    uint64_t Size() const override { return size_; }
    bool ReadAt(uint64_t offset, void* destination, size_t length) override;

private:
    std::FILE* file_ = nullptr;
    uint64_t size_ = 0;
};

}  // namespace epub

#endif  // EPUB_ENGINE_BYTE_SOURCE_H_

#include "epub_engine/byte_source.h"

#include <cstring>

namespace epub {

bool MemoryByteSource::ReadAt(uint64_t offset, void* destination, size_t length)
{
    if (offset > bytes_.size() || length > bytes_.size() - offset) {
        return false;
    }
    if (length > 0) {
        std::memcpy(destination, bytes_.data() + offset, length);
    }
    return true;
}

FileByteSource::FileByteSource(const std::string& path)
{
    file_ = std::fopen(path.c_str(), "rb");
    if (file_ == nullptr) {
        return;
    }
    if (std::fseek(file_, 0, SEEK_END) != 0) {
        std::fclose(file_);
        file_ = nullptr;
        return;
    }
    const long end = std::ftell(file_);
    if (end < 0) {
        std::fclose(file_);
        file_ = nullptr;
        return;
    }
    size_ = static_cast<uint64_t>(end);
}

FileByteSource::~FileByteSource()
{
    if (file_ != nullptr) {
        std::fclose(file_);
    }
}

bool FileByteSource::ReadAt(uint64_t offset, void* destination, size_t length)
{
    if (file_ == nullptr || offset > size_ || length > size_ - offset) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    if (std::fseek(file_, static_cast<long>(offset), SEEK_SET) != 0) {
        return false;
    }
    return std::fread(destination, 1, length, file_) == length;
}

}  // namespace epub

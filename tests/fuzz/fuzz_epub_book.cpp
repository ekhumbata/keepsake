// libFuzzer target: open arbitrary bytes as an EPUB and read every chapter. Seed it with the
// generated test shelf (build/tests/fixtures/*.epub) to start from valid structure.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "epub_engine/epub_book.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    auto source = std::make_unique<epub::MemoryByteSource>(
        std::string(reinterpret_cast<const char*>(data), size));
    epub::OpenLimits limits;
    limits.max_document_bytes = 1u << 20;
    limits.max_package_bytes = 1u << 18;
    epub::OpenError error = epub::OpenError::kNone;
    auto book = epub::EpubBook::Open(std::move(source), &error, limits);
    if (book == nullptr) {
        return 0;
    }
    epub::XhtmlLimits chapter_limits;
    chapter_limits.max_blocks = 2000;
    for (size_t i = 0; i < book->spine().size() && i < 16; ++i) {
        epub::Chapter chapter;
        (void)book->LoadChapter(i, &chapter, chapter_limits);
    }
    std::string cover;
    (void)book->ReadFile(book->metadata().cover_path, 1u << 20, &cover);
    return 0;
}

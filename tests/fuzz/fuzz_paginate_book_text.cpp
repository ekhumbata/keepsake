// libFuzzer target: PaginateBookText on arbitrary bytes (any .txt the friend copies over USB,
// in any encoding, cut at any chunk boundary). Checks the invariants the reader coordinator
// relies on.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "book_formats/book_formats.h"
#include "epaper_ui/book_reader_page.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1) {
        return 0;
    }
    // First byte: one of a few page heights (short and tall pages both get covered) and an
    // encoding.
    static constexpr int kHeights[] = {800, 600, 300, 120};
    static constexpr book_formats::TextEncoding kEncodings[] = {
        book_formats::TextEncoding::kUtf8, book_formats::TextEncoding::kUtf16Le,
        book_formats::TextEncoding::kUtf16Be, book_formats::TextEncoding::kLatin1};
    const int height = kHeights[data[0] % 4];
    const auto encoding = kEncodings[(data[0] / 4) % 4];
    const std::string text(reinterpret_cast<const char*>(data + 1), size - 1);
    (void)book_formats::DetectTextEncoding(text);

    const auto layout = epaper_ui::PaginateBookText(480, height, text, encoding);
    if (layout.consumed_bytes > text.size()) {
        std::abort();
    }
    // Not exhausted means there is text left past the page, so the reader can page on.
    if (!layout.text_exhausted && layout.consumed_bytes >= text.size()) {
        std::abort();
    }
    const bool utf16 = encoding == book_formats::TextEncoding::kUtf16Le ||
                       encoding == book_formats::TextEncoding::kUtf16Be;
    if (utf16 && layout.consumed_bytes % 2 != 0) {
        std::abort();  // the next read must start on a whole UTF-16 unit
    }
    // A page that drew lines consumed bytes. Exhausted means at most one cut-off character
    // (under 4 bytes) is left over for the next read.
    if (!layout.lines.empty()) {
        if (layout.consumed_bytes == 0) {
            std::abort();
        }
        if (layout.text_exhausted && text.size() - layout.consumed_bytes >= 4) {
            std::abort();
        }
    }
    return 0;
}

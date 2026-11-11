// libFuzzer target: arbitrary markup -> blocks -> pages, checking the invariants the reader
// relies on (lines inside the page, positions never going backwards).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#include "book_layout/book_layout.h"
#include "epub_engine/xhtml_parser.h"

namespace {

class FuzzMeasurer final : public book_layout::TextMeasurer {
public:
    int Width(std::string_view text, uint8_t style, book_layout::TextRole) const override
    {
        return static_cast<int>(text.size()) * ((style & epub::kStyleBold) ? 9 : 8);
    }
    int LineHeight(book_layout::TextRole role) const override
    {
        return role == book_layout::TextRole::kHeading1 ? 40 : 22;
    }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 2) {
        return 0;
    }
    book_layout::LayoutOptions options;
    options.width = 16 + data[0] * 2;    // 16..526 px, including absurdly narrow pages
    options.height = 30 + data[1] * 3;   // 30..795 px, including pages shorter than a heading
    epub::XhtmlLimits limits;
    limits.max_blocks = 300;
    const epub::Chapter chapter = epub::ParseXhtml(
        std::string_view(reinterpret_cast<const char*>(data + 2), size - 2), "a.xhtml", limits);
    const auto pages = book_layout::PaginateChapter(chapter, FuzzMeasurer(), options);
    if (pages.empty()) {
        std::abort();
    }
    book_layout::Position previous{0, 0};
    for (const auto& page : pages) {
        for (const auto& line : page.lines) {
            if (line.y < 0 || line.y + line.height > options.height) {
                std::abort();
            }
            const book_layout::Position here{line.block, line.offset};
            if (here < previous) {
                std::abort();
            }
            previous = here;
        }
    }
    return 0;
}

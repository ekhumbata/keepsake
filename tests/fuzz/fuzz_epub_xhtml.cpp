// libFuzzer target: XHTML -> blocks on arbitrary markup, checking the block/anchor invariants
// the reader relies on.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#include "epub_engine/xhtml_parser.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    epub::XhtmlLimits limits;
    limits.max_blocks = 500;
    const epub::Chapter chapter = epub::ParseXhtml(
        std::string_view(reinterpret_cast<const char*>(data), size), "OEBPS/text/a.xhtml", limits);
    if (chapter.blocks.size() > limits.max_blocks) {
        std::abort();
    }
    for (const auto& [id, index] : chapter.anchors) {
        if (index > chapter.blocks.size()) {
            std::abort();
        }
    }
    for (const epub::Block& block : chapter.blocks) {
        if (block.type != epub::BlockType::kImage && block.type != epub::BlockType::kRule &&
            block.runs.empty()) {
            std::abort();  // text blocks are never empty
        }
    }
    return 0;
}

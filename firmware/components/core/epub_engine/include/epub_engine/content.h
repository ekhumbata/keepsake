#ifndef EPUB_ENGINE_CONTENT_H_
#define EPUB_ENGINE_CONTENT_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace epub {

// The block model a chapter is reduced to: what a reflowing 1-bit reader needs, nothing
// about CSS boxes. TXT (and later FB2/MOBI) books can produce the same model.

enum TextStyle : uint8_t {
    kStyleNone = 0,
    kStyleBold = 1u << 0,
    kStyleItalic = 1u << 1,
};

struct TextRun {
    std::string text;  // UTF-8; a run of exactly "\n" is a forced line break (<br>)
    uint8_t style = kStyleNone;

    bool operator==(const TextRun& other) const = default;
};

enum class BlockType : uint8_t {
    kParagraph,
    kHeading,     // level 1..6
    kListItem,    // level = list nesting depth (1 = top level)
    kBlockquote,  // level = quote nesting depth
    kPreformatted,
    kImage,       // image_href (+ alt text in runs), drawn as a placeholder for now (#69)
    kRule,        // <hr>, a scene break
};

struct Block {
    BlockType type = BlockType::kParagraph;
    uint8_t level = 0;
    std::vector<TextRun> runs;
    std::string image_href;  // kImage only: resolved zip path

    bool operator==(const Block& other) const = default;
};

struct Chapter {
    std::vector<Block> blocks;
    // Element ids ("chapter-3", "note12") -> index of the first block at or after that
    // element, so TOC entries and links with a #fragment can land on the right block.
    std::vector<std::pair<std::string, size_t>> anchors;
    // True when a size limit cut the chapter short (the reader can say so).
    bool truncated = false;

    // Block index for `id`, or 0 when the id isn't known.
    size_t AnchorBlock(const std::string& id) const;
};

// Plain text of a block's runs joined together (line breaks as '\n').
std::string BlockText(const Block& block);

}  // namespace epub

#endif  // EPUB_ENGINE_CONTENT_H_

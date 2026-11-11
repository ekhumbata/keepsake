#ifndef BOOK_LAYOUT_BOOK_LAYOUT_H_
#define BOOK_LAYOUT_BOOK_LAYOUT_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "epub_engine/content.h"

// Lays a chapter's blocks out into lines and pages for the reader (#28). Font-agnostic: all
// measuring goes through TextMeasurer, so the same code works with today's bitmap fonts,
// Literata at any size, and a fixed-width fake in tests.
namespace book_layout {

// What kind of text a line holds; the renderer picks a font per role.
enum class TextRole : uint8_t {
    kBody,
    kHeading1,  // <h1>
    kHeading2,  // <h2>
    kHeading3,  // <h3>..<h6>
    kPreformatted,
};

class TextMeasurer {
public:
    virtual ~TextMeasurer() = default;
    // Width in pixels of UTF-8 `text` in `role` with epub::TextStyle flags `style`.
    virtual int Width(std::string_view text, uint8_t style, TextRole role) const = 0;
    // Distance from one baseline to the next for `role`.
    virtual int LineHeight(TextRole role) const = 0;
};

struct LayoutOptions {
    int width = 432;               // content area, pixels
    int height = 640;
    int paragraph_gap = 8;         // vertical space between blocks
    int first_line_indent = 0;     // paragraphs only; 0 = block style with gaps
    int list_indent = 24;          // per nesting level
    int quote_indent = 24;         // per nesting level
    int heading_gap = 16;          // extra space above a heading (not at a page top)
    int rule_height = 24;          // a scene break
    int image_height = 48;         // placeholder box until images are drawn (#69)
};

struct Span {
    std::string text;  // UTF-8, never contains '\n'
    uint8_t style = epub::kStyleNone;
    int x = 0;         // from the content area's left edge

    bool operator==(const Span& other) const = default;
};

enum class LineKind : uint8_t {
    kText,
    kRule,       // draw a centered scene-break mark
    kImage,      // draw a placeholder (alt text, if any, is in spans)
    kBullet,     // a text line that starts a list item: draw a bullet left of spans[0]
};

struct Line {
    LineKind kind = LineKind::kText;
    TextRole role = TextRole::kBody;
    int y = 0;          // top of the line, from the content area's top edge
    int height = 0;
    int x_indent = 0;   // left edge of the text (bullet goes just left of it)
    std::vector<Span> spans;
    // Where this line starts in the chapter: the block, and the byte offset into that block's
    // text (epub::BlockText). Stable across font changes, so it is what reading progress
    // stores.
    size_t block = 0;
    size_t offset = 0;

    bool operator==(const Line& other) const = default;
};

struct Page {
    std::vector<Line> lines;

    bool operator==(const Page& other) const = default;
};

struct Position {
    size_t block = 0;
    size_t offset = 0;

    bool operator<(const Position& other) const
    {
        return block != other.block ? block < other.block : offset < other.offset;
    }
    bool operator==(const Position& other) const = default;
};

// Lays out the whole chapter. Always returns at least one page (an empty one for an empty
// chapter). A word wider than the line is broken between characters.
std::vector<Page> PaginateChapter(const epub::Chapter& chapter, const TextMeasurer& measurer,
                                  const LayoutOptions& options);

// Position of the first line of `page` (where to resume reading).
Position PageStart(const Page& page);

// The page containing `position` (the last page whose start is at or before it); 0 for an
// empty list. Used to reopen a book at the saved place after a font-size change.
size_t PageForPosition(const std::vector<Page>& pages, Position position);

}  // namespace book_layout

#endif  // BOOK_LAYOUT_BOOK_LAYOUT_H_

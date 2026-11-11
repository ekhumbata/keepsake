#ifndef EPAPER_UI_BOOK_PAGE_H_
#define EPAPER_UI_BOOK_PAGE_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "book_layout/book_layout.h"
#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"

// Bridges book_layout to this UI's bitmap fonts: measuring for layout and drawing a laid-out
// page (#28). Today's fonts are Inter, ASCII only, with no italic; Literata replaces them in
// #28's font step without changing this interface.
namespace epaper_ui {

// Font for a line role + span style (bold picks the bold weight; italic has no font yet).
design::TypographyRole BookTypographyRole(book_layout::TextRole role, uint8_t style);

// UTF-8 -> what the ASCII bitmap fonts can draw: typographic quotes, dashes, ellipsis, spaces
// and Latin-1 letters become their closest ASCII; anything else becomes '?' (one per character,
// not one per byte). Used for both measuring and drawing, so layout matches what's drawn.
std::string ToDisplayText(std::string_view utf8);

class EpaperTextMeasurer final : public book_layout::TextMeasurer {
public:
    int Width(std::string_view text, uint8_t style, book_layout::TextRole role) const override;
    int LineHeight(book_layout::TextRole role) const override;
};

// Draws `page` with its content area's top-left at `origin` (portrait coordinates). Clears
// nothing: the caller paints the background.
void DrawBookLayoutPage(uint8_t* framebuffer, int raw_width, int raw_height, int portrait_width,
                        int portrait_height, int origin_x, int origin_y,
                        const book_layout::Page& page);

}  // namespace epaper_ui

#endif  // EPAPER_UI_BOOK_PAGE_H_

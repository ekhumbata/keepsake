#include "epaper_ui/font_renderer.h"

#include <atomic>

namespace epaper_ui {
namespace {

// Set from the input task (reader menu), read by the display task while drawing.
std::atomic<uint8_t> s_book_text_size{static_cast<uint8_t>(BookTextSize::kMedium)};

}  // namespace

void SetBookTextSize(BookTextSize size)
{
    if (static_cast<int>(size) >= kBookTextSizeCount) {
        size = BookTextSize::kMedium;
    }
    s_book_text_size.store(static_cast<uint8_t>(size), std::memory_order_relaxed);
}

BookTextSize GetBookTextSize()
{
    return static_cast<BookTextSize>(s_book_text_size.load(std::memory_order_relaxed));
}

const char* BookTextSizeName(BookTextSize size)
{
    switch (size) {
        case BookTextSize::kSmall:
            return "Small";
        case BookTextSize::kLarge:
            return "Large";
        case BookTextSize::kMedium:
        default:
            return "Medium";
    }
}

int MeasureText(design::TypographyRole role, std::string_view text)
{
    using font_renderer_detail::FontForRole;
    using font_renderer_detail::ScaleMetric;

    const font_renderer_detail::FontSelection selection = FontForRole(role);
    return ScaleMetric(epaper_font::MeasureText(*selection.font, text, selection.tracking),
                       selection);
}

int LineHeight(design::TypographyRole role)
{
    using font_renderer_detail::FontForRole;
    using font_renderer_detail::ScaleMetric;

    const font_renderer_detail::FontSelection selection = FontForRole(role);
    if (selection.line_height_override > 0) {
        return selection.line_height_override;
    }
    return ScaleMetric(selection.font->line_height, selection);
}

}  // namespace epaper_ui

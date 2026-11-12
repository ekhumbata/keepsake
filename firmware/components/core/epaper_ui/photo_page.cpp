#include "epaper_ui/photo_page.h"

#include <algorithm>
#include <string>
#include <vector>

#include "asset_types.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kCaptionHeight = 40;  // photo::kCaptionHeight
constexpr int kSide = design::spacing::k16;
constexpr auto kCaptionRole = design::TypographyRole::kLabelSmall;

void DrawMessage(uint8_t* framebuffer, int raw_width, int raw_height, int portrait_width,
                 int portrait_height, const PhotoPageState& state)
{
    const int width = portrait_width - 2 * kSide;
    int y = portrait_height / 4;
    if (state.message_image != nullptr && state.message_image->width > 0) {
        const int size = 160;
        const int height = size * static_cast<int>(state.message_image->height) /
                           static_cast<int>(state.message_image->width);
        DrawScaledPortraitMonoAsset(framebuffer, raw_width, raw_height, portrait_width,
                                    portrait_height, {(portrait_width - size) / 2, y, size, height},
                                    state.message_image, design::color::kBlack);
        y += height + design::spacing::k24;
    }
    const auto title_role = design::TypographyRole::kHeadingH2;
    for (const std::string& line : WrapTextToWidth(title_role, state.message_title, width)) {
        const int w = MeasureText(title_role, line);
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           (portrait_width - w) / 2, y, line, title_role, design::color::kBlack);
        y += LineHeight(title_role);
    }
    y += design::spacing::k12;
    const auto body_role = design::TypographyRole::kBody;
    for (const std::string& line : WrapTextToWidth(body_role, state.message_body, width)) {
        const int w = MeasureText(body_role, line);
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           (portrait_width - w) / 2, y, line, body_role, design::color::kBlack);
        y += LineHeight(body_role) + design::spacing::k4;
    }
}

}  // namespace

void DrawPhotoPage(uint8_t* framebuffer,
                   int raw_width,
                   int raw_height,
                   int portrait_width,
                   int portrait_height,
                   const PhotoPageState& state)
{
    if (framebuffer == nullptr) {
        return;
    }
    FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     {0, 0, portrait_width, portrait_height}, design::color::kWhite);

    const cover_image::Thumbnail* photo = state.photo.get();
    if (photo == nullptr || photo->width <= 0 || photo->height <= 0) {
        DrawMessage(framebuffer, raw_width, raw_height, portrait_width, portrait_height, state);
        return;
    }

    // Centred in the area above the caption line.
    const int area_height = portrait_height - kCaptionHeight;
    const int w = std::min(photo->width, portrait_width);
    const int h = std::min(photo->height, area_height);
    const int x0 = (portrait_width - w) / 2;
    const int y0 = (area_height - h) / 2;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (photo->black(x, y)) {
                DrawPortraitPixel(framebuffer, raw_width, raw_height, portrait_width,
                                  portrait_height, x0 + x, y0 + y, true);
            }
        }
    }

    if (!state.show_caption) {
        return;
    }
    const int text_y = area_height + (kCaptionHeight - LineHeight(kCaptionRole)) / 2;
    const int position_width = MeasureText(kCaptionRole, state.position_text);
    const int caption_width =
        std::max(0, portrait_width - 2 * kSide - position_width - design::spacing::k12);
    std::string caption = state.caption_text;
    while (!caption.empty() && MeasureText(kCaptionRole, caption) > caption_width) {
        caption.pop_back();  // a long title just gets cut; there's no room for two lines
    }
    DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height, kSide,
                       text_y, caption, kCaptionRole, design::color::kBlack);
    DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       portrait_width - kSide - position_width, text_y, state.position_text,
                       kCaptionRole, design::color::kTextSecondary);
}

}  // namespace epaper_ui

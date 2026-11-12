#ifndef EPAPER_UI_PHOTO_PAGE_H_
#define EPAPER_UI_PHOTO_PAGE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "cover_image/cover_image.h"

struct EmbeddedImageAsset;

namespace epaper_ui {

// The photo viewer (#136): one photo, full screen on white, the caption line under it.
// Without a photo (none yet, or one that can't be opened) it shows a message with Mochi.
struct PhotoPageState {
    // The prepared 1-bit photo (photo::Prepare); shared so state copies stay cheap.
    std::shared_ptr<const cover_image::Thumbnail> photo;
    std::string caption_text;   // "First date, May 20, 2019"
    std::string position_text;  // "3 / 12"
    bool show_caption = true;
    // When there's no photo to show.
    std::string message_title;  // "No photos yet", "Mochi can't open this one"
    std::string message_body;
    const EmbeddedImageAsset* message_image = nullptr;

    bool operator==(const PhotoPageState& other) const = default;
};

void DrawPhotoPage(uint8_t* framebuffer,
                   int raw_width,
                   int raw_height,
                   int portrait_width,
                   int portrait_height,
                   const PhotoPageState& state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_PHOTO_PAGE_H_

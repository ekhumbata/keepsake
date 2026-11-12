#ifndef PHOTO_SERVICE_H_
#define PHOTO_SERVICE_H_

#include <memory>
#include <string>

#include "cover_image/cover_image.h"
#include "esp_err.h"
#include "photo/photo.h"

// Photos (#136): the pictures in /photos on the SD card, oldest first, each prepared once for
// the 1-bit panel and cached in /.keepsake/photo-cache. Flipping to a cached photo is a ~50 KB
// read; the neighbours of the photo on screen are prepared in the background.
namespace photo_service {

esp_err_t Init();

// Lists /photos again (after USB mode, or when the viewer opens). Returns the count.
int Rescan();
int Count();
photo::Caption CaptionAt(int index);

struct Loaded {
    std::shared_ptr<const cover_image::Thumbnail> bitmap;  // null unless result is kOk
    photo::Result result = photo::Result::kUndecodable;
    std::string format;  // "HEIC" etc. when result is kUnsupported
};

// The prepared photo: from the cache, or prepared now (blocking: hold a cpu_boost while
// calling). Safe to call from any task.
Loaded Load(int index);
// Prepares `index` in the background if it isn't cached yet.
void Prefetch(int index);

// Where the viewer was: the index of the photo last shown (0 if it's gone).
int LastIndex();
void RememberIndex(int index);

}  // namespace photo_service

#endif  // PHOTO_SERVICE_H_

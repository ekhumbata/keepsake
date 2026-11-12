#ifndef PHOTO_SERVICE_TJPGD_DECODE_H_
#define PHOTO_SERVICE_TJPGD_DECODE_H_

#include <cstdio>

#include "photo/photo.h"

namespace photo_service {

// Decodes a baseline JPEG straight from `file` with the ESP32-S3 ROM's TJpgDec, at 1/1, 1/2,
// 1/4 or 1/8 scale (the smallest that still covers the photo area), to 8-bit gray. Phone photos
// (12 MP) come out around 500 x 375, so memory stays small. False for progressive JPEGs and
// anything TJpgDec can't read; the caller falls back to stb_image.
bool DecodeJpegScaled(FILE* file, photo::Gray* out);

}  // namespace photo_service

#endif  // PHOTO_SERVICE_TJPGD_DECODE_H_

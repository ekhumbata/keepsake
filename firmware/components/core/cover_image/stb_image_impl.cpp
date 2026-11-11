// The one translation unit that compiles stb_image (third_party/stb_image.h). Warnings are off
// for this file in both builds (CMake COMPILE_OPTIONS -w and the pragmas below): it's vendored
// code we don't edit.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_THREAD_LOCALS
// Never abort the device on a malformed image: stb reports failure by returning null.
#define STBI_ASSERT(x) ((void)0)
// Covers are a few thousand pixels on a side at most; anything bigger is refused up front.
#define STBI_MAX_DIMENSIONS 8192
#include "third_party/stb_image.h"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

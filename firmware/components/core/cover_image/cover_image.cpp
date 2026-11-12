#include "cover_image/cover_image.h"

#include <algorithm>
#include <cstring>

#include "third_party/stb_image.h"

namespace cover_image {
namespace {

constexpr char kMagic[4] = {'K', 'C', 'O', 'V'};
constexpr uint8_t kVersion = 1;
constexpr size_t kHeaderBytes = 4 + 1 + 2 + 2;
constexpr int kMaxThumbnailSide = 4096;
constexpr int kMaxEnlarge = 2;

// Box-filter resample of an 8-bit gray image: each output pixel averages the source pixels
// it covers (or takes the nearest one when enlarging).
void PutU16(std::string* out, int value)
{
    out->push_back(static_cast<char>(value & 0xFF));
    out->push_back(static_cast<char>((value >> 8) & 0xFF));
}

int GetU16(std::string_view data, size_t at)
{
    return static_cast<unsigned char>(data[at]) | (static_cast<unsigned char>(data[at + 1]) << 8);
}

}  // namespace

std::vector<uint8_t> Resample(const uint8_t* src, int width, int height, int out_width,
                              int out_height)
{
    std::vector<uint8_t> out(static_cast<size_t>(out_width) * static_cast<size_t>(out_height));
    for (int ty = 0; ty < out_height; ++ty) {
        const int y0 = static_cast<int>(static_cast<int64_t>(ty) * height / out_height);
        const int y1 = std::max(y0 + 1, static_cast<int>(static_cast<int64_t>(ty + 1) * height /
                                                         out_height));
        for (int tx = 0; tx < out_width; ++tx) {
            const int x0 = static_cast<int>(static_cast<int64_t>(tx) * width / out_width);
            const int x1 = std::max(x0 + 1, static_cast<int>(static_cast<int64_t>(tx + 1) *
                                                             width / out_width));
            uint64_t sum = 0;
            for (int y = y0; y < y1 && y < height; ++y) {
                const uint8_t* row = src + static_cast<size_t>(y) * static_cast<size_t>(width);
                for (int x = x0; x < x1 && x < width; ++x) {
                    sum += row[x];
                }
            }
            const uint64_t count = static_cast<uint64_t>(std::min(y1, height) - y0) *
                                   static_cast<uint64_t>(std::min(x1, width) - x0);
            out[static_cast<size_t>(ty) * static_cast<size_t>(out_width) +
                static_cast<size_t>(tx)] = static_cast<uint8_t>(count ? sum / count : 255);
        }
    }
    return out;
}

Result DecodeGray(std::string_view encoded, std::vector<uint8_t>* gray, int* width, int* height,
                  const Limits& limits)
{
    gray->clear();
    *width = 0;
    *height = 0;
    if (encoded.empty()) {
        return Result::kEmpty;
    }
    if (encoded.size() > limits.max_input_bytes || encoded.size() > static_cast<size_t>(INT32_MAX)) {
        return Result::kTooBig;
    }
    const auto* bytes = reinterpret_cast<const stbi_uc*>(encoded.data());
    const int length = static_cast<int>(encoded.size());
    int w = 0;
    int h = 0;
    int channels = 0;
    if (!stbi_info_from_memory(bytes, length, &w, &h, &channels) || w <= 0 || h <= 0) {
        return Result::kUndecodable;
    }
    if (static_cast<uint64_t>(w) * static_cast<uint64_t>(h) > limits.max_source_pixels) {
        return Result::kTooBig;
    }
    stbi_uc* pixels = stbi_load_from_memory(bytes, length, &w, &h, &channels, 1);
    if (pixels == nullptr) {
        return Result::kUndecodable;
    }
    gray->assign(pixels, pixels + static_cast<size_t>(w) * static_cast<size_t>(h));
    stbi_image_free(pixels);
    *width = w;
    *height = h;
    return Result::kOk;
}

Thumbnail Dither(const std::vector<uint8_t>& gray, int width, int height)
{
    Thumbnail out;
    if (width <= 0 || height <= 0 ||
        gray.size() < static_cast<size_t>(width) * static_cast<size_t>(height)) {
        return out;
    }
    out.width = width;
    out.height = height;
    out.bits.assign(static_cast<size_t>(out.stride()) * static_cast<size_t>(height), 0);

    // Atkinson: pass 6/8 of the error on (1/8 to each of six neighbours) and drop the rest,
    // which keeps light areas clean instead of speckled.
    std::vector<int> level(gray.begin(), gray.begin() + static_cast<long>(width) * height);
    const auto spread = [&](int x, int y, int amount) {
        if (x >= 0 && x < width && y < height) {
            level[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] +=
                amount;
        }
    };
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int old = level[static_cast<size_t>(y) * static_cast<size_t>(width) +
                                  static_cast<size_t>(x)];
            const bool white = old >= 128;
            if (!white) {
                out.bits[static_cast<size_t>(y) * static_cast<size_t>(out.stride()) +
                         static_cast<size_t>(x) / 8] |= static_cast<uint8_t>(0x80 >> (x % 8));
            }
            const int share = (old - (white ? 255 : 0)) / 8;
            spread(x + 1, y, share);
            spread(x + 2, y, share);
            spread(x - 1, y + 1, share);
            spread(x, y + 1, share);
            spread(x + 1, y + 1, share);
            spread(x, y + 2, share);
        }
    }
    return out;
}

Result MakeThumbnail(std::string_view encoded, int box_width, int box_height, Thumbnail* out,
                     const Limits& limits)
{
    *out = Thumbnail{};
    if (encoded.empty()) {
        return Result::kEmpty;
    }
    if (encoded.size() > limits.max_input_bytes ||
        encoded.size() > static_cast<size_t>(INT32_MAX) || box_width <= 0 || box_height <= 0) {
        return Result::kTooBig;
    }
    const auto* bytes = reinterpret_cast<const stbi_uc*>(encoded.data());
    const int length = static_cast<int>(encoded.size());
    int width = 0;
    int height = 0;
    int channels = 0;
    if (!stbi_info_from_memory(bytes, length, &width, &height, &channels) || width <= 0 ||
        height <= 0) {
        return Result::kUndecodable;
    }
    if (static_cast<uint64_t>(width) * static_cast<uint64_t>(height) > limits.max_source_pixels) {
        return Result::kTooBig;
    }
    stbi_uc* gray = stbi_load_from_memory(bytes, length, &width, &height, &channels, 1);
    if (gray == nullptr) {
        return Result::kUndecodable;
    }

    box_width = std::min(box_width, kMaxThumbnailSide);
    box_height = std::min(box_height, kMaxThumbnailSide);
    // Fit inside the box, keeping the shape; enlarge small images at most kMaxEnlarge times.
    int out_width = box_width;
    int out_height = static_cast<int>(static_cast<int64_t>(height) * box_width / width);
    if (out_height > box_height) {
        out_height = box_height;
        out_width = static_cast<int>(static_cast<int64_t>(width) * box_height / height);
    }
    out_width = std::clamp(out_width, 1, std::max(1, width * kMaxEnlarge));
    out_height = std::clamp(out_height, 1, std::max(1, height * kMaxEnlarge));

    const std::vector<uint8_t> scaled = Resample(gray, width, height, out_width, out_height);
    stbi_image_free(gray);
    *out = Dither(scaled, out_width, out_height);
    return Result::kOk;
}

std::string Serialize(const Thumbnail& thumbnail)
{
    std::string out(kMagic, sizeof(kMagic));
    out.push_back(static_cast<char>(kVersion));
    PutU16(&out, thumbnail.width);
    PutU16(&out, thumbnail.height);
    out.append(reinterpret_cast<const char*>(thumbnail.bits.data()), thumbnail.bits.size());
    return out;
}

bool Parse(std::string_view data, Thumbnail* out)
{
    *out = Thumbnail{};
    if (data.size() < kHeaderBytes || std::memcmp(data.data(), kMagic, sizeof(kMagic)) != 0 ||
        static_cast<uint8_t>(data[4]) != kVersion) {
        return false;
    }
    Thumbnail parsed;
    parsed.width = GetU16(data, 5);
    parsed.height = GetU16(data, 7);
    if (parsed.width <= 0 || parsed.height <= 0 || parsed.width > kMaxThumbnailSide ||
        parsed.height > kMaxThumbnailSide) {
        return false;
    }
    const size_t expected = static_cast<size_t>(parsed.stride()) * static_cast<size_t>(parsed.height);
    if (data.size() != kHeaderBytes + expected) {
        return false;
    }
    parsed.bits.assign(data.begin() + static_cast<long>(kHeaderBytes), data.end());
    *out = std::move(parsed);
    return true;
}

}  // namespace cover_image

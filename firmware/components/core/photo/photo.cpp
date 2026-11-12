#include "photo/photo.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace photo {
namespace {

// --- bytes ------------------------------------------------------------------------------------

uint16_t U16(const uint8_t* p, bool little)
{
    return little ? static_cast<uint16_t>(p[0] | (p[1] << 8))
                  : static_cast<uint16_t>((p[0] << 8) | p[1]);
}

uint32_t U32(const uint8_t* p, bool little)
{
    return little ? static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) |
                                          (static_cast<uint32_t>(p[3]) << 24))
                  : static_cast<uint32_t>((static_cast<uint32_t>(p[0]) << 24) | (p[1] << 16) |
                                          (p[2] << 8) | p[3]);
}

// One IFD inside a TIFF block: calls `entry(tag, type, count, value_offset_ptr)` per entry.
template <typename Fn>
void WalkIfd(const uint8_t* tiff, size_t size, uint32_t offset, bool little, Fn&& entry)
{
    if (offset > size || size - offset < 2) {
        return;
    }
    const uint16_t count = U16(tiff + offset, little);
    for (uint32_t i = 0; i < count; ++i) {
        const uint64_t at = static_cast<uint64_t>(offset) + 2 + i * 12ull;
        if (at + 12 > size) {
            return;
        }
        const uint8_t* e = tiff + at;
        entry(U16(e, little), U16(e + 2, little), U32(e + 4, little), e + 8);
    }
}

constexpr uint16_t kTagOrientation = 0x0112;
constexpr uint16_t kTagExifIfd = 0x8769;
constexpr uint16_t kTagDateTimeOriginal = 0x9003;
constexpr uint16_t kTypeShort = 3;
constexpr uint16_t kTypeLong = 4;
constexpr uint16_t kTypeAscii = 2;

void ParseTiff(const uint8_t* tiff, size_t size, Exif* out)
{
    if (size < 8) {
        return;
    }
    bool little = false;
    if (tiff[0] == 'I' && tiff[1] == 'I') {
        little = true;
    } else if (!(tiff[0] == 'M' && tiff[1] == 'M')) {
        return;
    }
    if (U16(tiff + 2, little) != 42) {
        return;
    }
    const uint32_t ifd0 = U32(tiff + 4, little);
    uint32_t exif_ifd = 0;
    WalkIfd(tiff, size, ifd0, little,
            [&](uint16_t tag, uint16_t type, uint32_t count, const uint8_t* value) {
                if (tag == kTagOrientation && type == kTypeShort && count >= 1) {
                    const int orientation = U16(value, little);
                    if (orientation >= 1 && orientation <= 8) {
                        out->orientation = orientation;
                    }
                } else if (tag == kTagExifIfd && type == kTypeLong && count == 1) {
                    exif_ifd = U32(value, little);
                }
            });
    if (exif_ifd == 0 || exif_ifd == ifd0) {
        return;
    }
    WalkIfd(tiff, size, exif_ifd, little,
            [&](uint16_t tag, uint16_t type, uint32_t count, const uint8_t* value) {
                if (tag != kTagDateTimeOriginal || type != kTypeAscii || count < 19 || count > 64) {
                    return;
                }
                const uint32_t offset = U32(value, little);
                if (offset > size || size - offset < 19) {
                    return;
                }
                std::string text(reinterpret_cast<const char*>(tiff + offset), 19);
                for (const char c : text) {
                    if (c < 0x20 || c > 0x7e) {
                        return;
                    }
                }
                out->date_time = text;
            });
}

// --- dates and names ------------------------------------------------------------------------

bool ValidDate(int year, int month, int day)
{
    static constexpr int kDays[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year < 1900 || year > 2100 || month < 1 || month > 12 || day < 1) {
        return false;
    }
    if (day > kDays[month - 1]) {
        return false;
    }
    if (month == 2 && day == 29) {
        return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    }
    return true;
}

bool Digits(std::string_view text, size_t at, size_t count, int* value)
{
    if (at + count > text.size()) {
        return false;
    }
    int v = 0;
    for (size_t i = 0; i < count; ++i) {
        const char c = text[at + i];
        if (c < '0' || c > '9') {
            return false;
        }
        v = v * 10 + (c - '0');
    }
    *value = v;
    return true;
}

bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

// A date at the start of `text`: "2019-05-20", "2019_05_20", "2019.05.20" or "20190520", not
// followed by another digit. Sets *length to the characters used.
bool LeadingDate(std::string_view text, Date* date, size_t* length)
{
    int y = 0;
    int m = 0;
    int d = 0;
    if (Digits(text, 0, 4, &y) && text.size() >= 10 &&
        (text[4] == '-' || text[4] == '_' || text[4] == '.') && text[7] == text[4] &&
        Digits(text, 5, 2, &m) && Digits(text, 8, 2, &d) &&
        (text.size() == 10 || !IsDigit(text[10])) && ValidDate(y, m, d)) {
        *date = {y, m, d};
        *length = 10;
        return true;
    }
    if (Digits(text, 0, 4, &y) && Digits(text, 4, 2, &m) && Digits(text, 6, 2, &d) &&
        (text.size() == 8 || !IsDigit(text[8])) && ValidDate(y, m, d)) {
        *date = {y, m, d};
        *length = 8;
        return true;
    }
    return false;
}

// The first 8-digit run anywhere in `text` that is a date (camera names: "PXL_20190520_1234").
bool DateInside(std::string_view text, Date* date)
{
    for (size_t i = 0; i + 8 <= text.size(); ++i) {
        if (i > 0 && IsDigit(text[i - 1])) {
            continue;
        }
        size_t length = 0;
        if (LeadingDate(text.substr(i), date, &length)) {
            return true;
        }
    }
    return false;
}

char Lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool StartsWithNoCase(std::string_view text, std::string_view prefix)
{
    if (text.size() < prefix.size()) {
        return false;
    }
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (Lower(text[i]) != Lower(prefix[i])) {
            return false;
        }
    }
    return true;
}

bool IsCameraName(std::string_view stem)
{
    static constexpr std::string_view kPrefixes[] = {
        "img", "dsc", "dscn", "dscf", "pxl", "mvimg", "pano", "vid", "screenshot", "photo",
        "image", "p", "dji", "gopr", "sam", "wp"};
    for (const std::string_view prefix : kPrefixes) {
        if (!StartsWithNoCase(stem, prefix)) {
            continue;
        }
        const std::string_view rest = stem.substr(prefix.size());
        // The rest is only digits and separators: a camera's counter, not words.
        bool only_counter = !rest.empty();
        for (const char c : rest) {
            if (!(IsDigit(c) || c == '_' || c == '-' || c == ' ' || c == '.' || c == '(' ||
                  c == ')' || c == '~')) {
                only_counter = false;
                break;
            }
        }
        if (only_counter) {
            return true;
        }
    }
    // Nothing but digits and separators at all ("1234", "2019-05-20_183000").
    return std::all_of(stem.begin(), stem.end(), [](char c) {
        return IsDigit(c) || c == '_' || c == '-' || c == ' ' || c == '.';
    });
}

std::string Tidy(std::string_view text)
{
    std::string out;
    bool space = false;
    for (char c : text) {
        if (c == '_') {
            c = ' ';
        }
        if (c == ' ') {
            space = !out.empty();
            continue;
        }
        if (space) {
            out.push_back(' ');
            space = false;
        }
        out.push_back(c);
    }
    // Separators left at the start ("- First date") go too.
    size_t start = 0;
    while (start < out.size() && (out[start] == '-' || out[start] == '.' || out[start] == ' ')) {
        ++start;
    }
    return out.substr(start);
}

constexpr const char* kMonths[] = {"January", "February", "March",     "April",   "May",
                                   "June",    "July",     "August",    "September",
                                   "October", "November", "December"};

int Percentile(const std::array<uint32_t, 256>& histogram, uint64_t total, double fraction)
{
    const uint64_t target = static_cast<uint64_t>(static_cast<double>(total) * fraction);
    uint64_t seen = 0;
    for (int v = 0; v < 256; ++v) {
        seen += histogram[static_cast<size_t>(v)];
        if (seen > target) {
            return v;
        }
    }
    return 255;
}

}  // namespace

Exif ReadExif(std::string_view jpeg)
{
    Exif exif;
    const auto* bytes = reinterpret_cast<const uint8_t*>(jpeg.data());
    const size_t size = jpeg.size();
    if (size < 4 || bytes[0] != 0xFF || bytes[1] != 0xD8) {
        return exif;
    }
    size_t at = 2;
    while (at + 4 <= size) {
        if (bytes[at] != 0xFF) {
            return exif;
        }
        const uint8_t marker = bytes[at + 1];
        if (marker == 0xFF) {  // fill byte
            ++at;
            continue;
        }
        if (marker == 0xDA || marker == 0xD9) {  // image data or end: no more metadata
            return exif;
        }
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {  // no length
            at += 2;
            continue;
        }
        const size_t length = U16(bytes + at + 2, false);
        if (length < 2 || at + 2 + length > size) {
            return exif;
        }
        const uint8_t* data = bytes + at + 4;
        const size_t data_size = length - 2;
        if (marker == 0xE1 && data_size >= 6 && std::memcmp(data, "Exif\0\0", 6) == 0) {
            ParseTiff(data + 6, data_size - 6, &exif);
            return exif;
        }
        at += 2 + length;
    }
    return exif;
}

Gray Orient(const Gray& image, int orientation)
{
    if (orientation < 2 || orientation > 8 || image.width <= 0 || image.height <= 0 ||
        image.pixels.size() < static_cast<size_t>(image.width) * static_cast<size_t>(image.height)) {
        return image;
    }
    const int w = image.width;
    const int h = image.height;
    const bool swap = orientation >= 5;
    Gray out;
    out.width = swap ? h : w;
    out.height = swap ? w : h;
    out.pixels.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
    for (int oy = 0; oy < out.height; ++oy) {
        for (int ox = 0; ox < out.width; ++ox) {
            int ix = ox;
            int iy = oy;
            switch (orientation) {
                case 2: ix = w - 1 - ox; iy = oy; break;
                case 3: ix = w - 1 - ox; iy = h - 1 - oy; break;
                case 4: ix = ox; iy = h - 1 - oy; break;
                case 5: ix = oy; iy = ox; break;
                case 6: ix = oy; iy = h - 1 - ox; break;
                case 7: ix = w - 1 - oy; iy = h - 1 - ox; break;
                case 8: ix = w - 1 - oy; iy = ox; break;
                default: break;
            }
            out.pixels[static_cast<size_t>(oy) * static_cast<size_t>(out.width) +
                       static_cast<size_t>(ox)] =
                image.pixels[static_cast<size_t>(iy) * static_cast<size_t>(w) +
                             static_cast<size_t>(ix)];
        }
    }
    return out;
}

Gray FitInside(const Gray& image, int box_width, int box_height)
{
    if (image.width <= 0 || image.height <= 0 || box_width <= 0 || box_height <= 0 ||
        image.pixels.size() < static_cast<size_t>(image.width) * static_cast<size_t>(image.height)) {
        return {};
    }
    int out_width = box_width;
    int out_height = static_cast<int>(static_cast<int64_t>(image.height) * box_width / image.width);
    if (out_height > box_height) {
        out_height = box_height;
        out_width = static_cast<int>(static_cast<int64_t>(image.width) * box_height / image.height);
    }
    out_width = std::clamp(out_width, 1, std::max(1, image.width * 2));
    out_height = std::clamp(out_height, 1, std::max(1, image.height * 2));
    Gray out;
    out.width = out_width;
    out.height = out_height;
    out.pixels = cover_image::Resample(image.pixels.data(), image.width, image.height, out_width,
                                       out_height);
    return out;
}

void AutoLevels(Gray* image)
{
    if (image == nullptr || image->pixels.empty()) {
        return;
    }
    std::array<uint32_t, 256> histogram = {};
    for (const uint8_t v : image->pixels) {
        ++histogram[v];
    }
    const uint64_t total = image->pixels.size();
    int low = Percentile(histogram, total, 0.01);
    int high = Percentile(histogram, total, 0.99);
    if (high - low < 32) {  // a flat image (a page of text, fog): leave the range alone
        low = 0;
        high = 255;
    }
    std::array<uint8_t, 256> lut = {};
    for (int v = 0; v < 256; ++v) {
        double t = static_cast<double>(v - low) / static_cast<double>(high - low);
        t = std::clamp(t, 0.0, 1.0);
        t = std::pow(t, 0.8);  // lift the mid-tones: faces stay faces after dithering
        lut[static_cast<size_t>(v)] = static_cast<uint8_t>(std::lround(t * 255.0));
    }
    for (uint8_t& v : image->pixels) {
        v = lut[v];
    }
}

std::string FormatName(std::string_view head)
{
    const auto* b = reinterpret_cast<const uint8_t*>(head.data());
    const size_t n = head.size();
    if (n >= 3 && b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF) return "JPEG";
    if (n >= 8 && std::memcmp(b, "\x89PNG\r\n\x1a\n", 8) == 0) return "PNG";
    if (n >= 4 && std::memcmp(b, "GIF8", 4) == 0) return "GIF";
    if (n >= 12 && std::memcmp(b, "RIFF", 4) == 0 && std::memcmp(b + 8, "WEBP", 4) == 0) {
        return "WebP";
    }
    if (n >= 12 && std::memcmp(b + 4, "ftyp", 4) == 0) {
        const std::string_view brand(reinterpret_cast<const char*>(b + 8), 4);
        if (brand == "avif" || brand == "avis") return "AVIF";
        if (brand == "heic" || brand == "heix" || brand == "hevc" || brand == "heim" ||
            brand == "heis" || brand == "mif1" || brand == "msf1") {
            return "HEIC";
        }
        return "";
    }
    if (n >= 4 && (std::memcmp(b, "II*\0", 4) == 0 || std::memcmp(b, "MM\0*", 4) == 0)) {
        return "TIFF";
    }
    if (n >= 2 && b[0] == 'B' && b[1] == 'M') return "BMP";
    return "";
}

bool CanDecode(std::string_view format_name)
{
    return format_name == "JPEG" || format_name == "PNG" || format_name == "GIF";
}

cover_image::Thumbnail Prepare(const Gray& decoded, int orientation)
{
    // Fit before turning (the turned copy is then small); for a quarter turn the box turns too.
    const bool swap = orientation >= 5 && orientation <= 8;
    Gray fitted = FitInside(decoded, swap ? kBoxHeight : kBoxWidth, swap ? kBoxWidth : kBoxHeight);
    Gray oriented = Orient(fitted, orientation);
    AutoLevels(&oriented);
    return cover_image::Dither(oriented.pixels, oriented.width, oriented.height);
}

Result PrepareEncoded(std::string_view encoded, cover_image::Thumbnail* out,
                      const cover_image::Limits& limits)
{
    *out = cover_image::Thumbnail{};
    if (encoded.empty()) {
        return Result::kEmpty;
    }
    const std::string format = FormatName(encoded.substr(0, 16));
    if (!CanDecode(format)) {
        return format.empty() ? Result::kUndecodable : Result::kUnsupported;
    }
    Gray decoded;
    switch (cover_image::DecodeGray(encoded, &decoded.pixels, &decoded.width, &decoded.height,
                                    limits)) {
        case cover_image::Result::kOk:
            break;
        case cover_image::Result::kEmpty:
            return Result::kEmpty;
        case cover_image::Result::kTooBig:
            return Result::kTooBig;
        case cover_image::Result::kUndecodable:
        default:
            return Result::kUndecodable;
    }
    const int orientation = format == "JPEG" ? ReadExif(encoded).orientation : 1;
    *out = Prepare(decoded, orientation);
    return out->width > 0 ? Result::kOk : Result::kUndecodable;
}

bool IsPhotoName(std::string_view filename)
{
    const size_t slash = filename.find_last_of('/');
    const std::string_view name = slash == std::string_view::npos ? filename : filename.substr(slash + 1);
    if (name.empty() || name[0] == '.') {
        return false;
    }
    const size_t dot = name.find_last_of('.');
    if (dot == std::string_view::npos || dot == 0) {
        return false;
    }
    std::string ext;
    for (const char c : name.substr(dot + 1)) {
        ext.push_back(Lower(c));
    }
    return ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif";
}

std::string Caption::Text() const
{
    std::string date_text;
    if (date.valid()) {
        date_text = std::string(kMonths[date.month - 1]) + " " + std::to_string(date.day) + ", " +
                    std::to_string(date.year);
    }
    if (title.empty()) {
        return date_text;
    }
    if (date_text.empty()) {
        return title;
    }
    return title + ", " + date_text;
}

Caption CaptionFor(std::string_view filename, std::string_view exif_date)
{
    Caption caption;
    const size_t slash = filename.find_last_of('/');
    std::string_view stem = slash == std::string_view::npos ? filename : filename.substr(slash + 1);
    if (const size_t dot = stem.find_last_of('.'); dot != std::string_view::npos && dot > 0) {
        stem = stem.substr(0, dot);
    }

    size_t date_length = 0;
    if (LeadingDate(stem, &caption.date, &date_length)) {
        const std::string rest = Tidy(stem.substr(date_length));
        if (!IsCameraName(rest)) {
            caption.title = rest;
        }
    } else if (IsCameraName(stem)) {
        DateInside(stem, &caption.date);
    } else {
        caption.title = Tidy(stem);
    }
    if (caption.title.size() > 80) {
        caption.title.resize(80);
    }

    int y = 0;
    int m = 0;
    int d = 0;
    if (!caption.date.valid() && exif_date.size() >= 10 && exif_date[4] == ':' &&
        exif_date[7] == ':' && Digits(exif_date, 0, 4, &y) && Digits(exif_date, 5, 2, &m) &&
        Digits(exif_date, 8, 2, &d) && ValidDate(y, m, d)) {
        caption.date = {y, m, d};
    }
    return caption;
}

void SortOldestFirst(std::vector<Entry>* entries)
{
    std::stable_sort(entries->begin(), entries->end(), [](const Entry& a, const Entry& b) {
        const Date& da = a.caption.date;
        const Date& db = b.caption.date;
        if (da.valid() != db.valid()) {
            return da.valid();
        }
        if (da.valid()) {
            if (da.year != db.year) return da.year < db.year;
            if (da.month != db.month) return da.month < db.month;
            if (da.day != db.day) return da.day < db.day;
        }
        return a.path < b.path;
    });
}

std::string CacheName(std::string_view path, uint64_t size, int64_t modified)
{
    uint64_t hash = 1469598103934665603ull;  // FNV-1a
    const auto mix = [&hash](uint8_t byte) {
        hash ^= byte;
        hash *= 1099511628211ull;
    };
    for (const char c : path) {
        mix(static_cast<uint8_t>(c));
    }
    mix(0);
    for (int i = 0; i < 8; ++i) {
        mix(static_cast<uint8_t>(size >> (8 * i)));
    }
    for (int i = 0; i < 8; ++i) {
        mix(static_cast<uint8_t>(static_cast<uint64_t>(modified) >> (8 * i)));
    }
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%016llx.kpb", static_cast<unsigned long long>(hash));
    return buffer;
}

}  // namespace photo

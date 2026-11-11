#include "book_formats/book_formats.h"

#include <cctype>
#include <cstdint>
#include <string>

namespace book_formats {
namespace {

std::string LowerExtension(std::string_view file_name)
{
    const size_t dot = file_name.rfind('.');
    const size_t slash = file_name.find_last_of("/\\");
    if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) {
        return {};
    }
    std::string ext(file_name.substr(dot + 1));
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}

bool StartsWith(std::string_view text, std::string_view prefix)
{
    return text.substr(0, prefix.size()) == prefix;
}

uint32_t Be32(std::string_view bytes, size_t offset)
{
    return (static_cast<uint32_t>(static_cast<unsigned char>(bytes[offset])) << 24) |
           (static_cast<uint32_t>(static_cast<unsigned char>(bytes[offset + 1])) << 16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(bytes[offset + 2])) << 8) |
           static_cast<uint32_t>(static_cast<unsigned char>(bytes[offset + 3]));
}

uint16_t Be16(std::string_view bytes, size_t offset)
{
    return static_cast<uint16_t>((static_cast<unsigned char>(bytes[offset]) << 8) |
                                 static_cast<unsigned char>(bytes[offset + 1]));
}

// PalmDB: type+creator at 60 ("BOOKMOBI"), first record offset at 78. Record 0 starts with
// the PalmDOC header, whose encryption type (0 = none) is at +12.
bool MobiIsEncrypted(std::string_view head)
{
    if (head.size() < 82) {
        return false;
    }
    const uint32_t record0 = Be32(head, 78);
    if (record0 > head.size() || head.size() - record0 < 14) {
        return false;  // header beyond what we sniffed: unknown, let the reader decide (#34)
    }
    return Be16(head, record0 + 12) != 0;
}

// Text if there are no NUL bytes and most bytes are printable or UTF-8.
bool LooksLikeText(std::string_view head)
{
    if (head.empty()) {
        return false;
    }
    size_t odd = 0;
    for (const char c : head) {
        const auto u = static_cast<unsigned char>(c);
        if (u == 0) {
            return false;
        }
        if (u < 0x20 && c != '\n' && c != '\r' && c != '\t' && c != '\f') {
            ++odd;
        }
    }
    return odd * 20 < head.size();  // < 5% control characters
}

}  // namespace

Detection Detect(std::string_view file_name, std::string_view head)
{
    const std::string ext = LowerExtension(file_name);
    Detection detection;
    if (head.empty()) {
        detection.format = Format::kEmpty;
        return detection;
    }
    if (StartsWith(head, "%PDF-")) {
        detection.format = Format::kPdf;
        return detection;
    }
    if (head.size() >= 68 && head.substr(60, 8) == "BOOKMOBI") {
        detection.format = Format::kMobi;
        detection.drm = MobiIsEncrypted(head);
        return detection;
    }
    if (StartsWith(head, "\xEA" "DRMION\xEE")) {
        detection.format = Format::kKindleKfx;
        detection.drm = true;
        return detection;
    }
    if (StartsWith(head, "CONT") && (ext == "kfx" || ext == "azw" || ext == "azw8")) {
        detection.format = Format::kKindleKfx;
        return detection;
    }
    if (StartsWith(head, "PK\x03\x04")) {
        // An EPUB's first entry is a stored "mimetype" file.
        if (head.size() >= 58 && head.substr(30, 8) == "mimetype" &&
            head.substr(38, 20) == "application/epub+zip") {
            detection.format = Format::kEpub;
        } else if (ext == "epub") {
            detection.format = Format::kEpub;  // sloppy zipping; let the engine decide
        } else if (ext == "cbz") {
            detection.format = Format::kComicZip;
        } else {
            detection.format = Format::kOtherZip;
        }
        return detection;
    }
    if (ext == "epub") {
        detection.format = Format::kEpub;  // not a zip: the engine reports "damaged/not EPUB"
        return detection;
    }
    if ((ext == "txt" || ext == "md" || ext == "text" || ext.empty()) && LooksLikeText(head)) {
        detection.format = Format::kText;
        return detection;
    }
    if (ext == "mobi" || ext == "azw" || ext == "azw3" || ext == "prc") {
        detection.format = Format::kMobi;  // claims MOBI but no PalmDB header: damaged
        return detection;
    }
    detection.format = Format::kUnknown;
    return detection;
}

Problem ProblemForDetection(const Detection& detection)
{
    if (detection.drm) {
        return Problem::kDrm;
    }
    switch (detection.format) {
        case Format::kEpub:
        case Format::kText:
            return Problem::kNone;
        case Format::kPdf:
            return Problem::kPdf;
        case Format::kMobi:  // DRM-free MOBI/AZW3 support is #34
        case Format::kKindleKfx:
            return Problem::kKindle;
        case Format::kComicZip:
            return Problem::kComic;
        case Format::kEmpty:
            return Problem::kEmpty;
        case Format::kOtherZip:
        case Format::kUnknown:
            return Problem::kNotABook;
    }
    return Problem::kNotABook;
}

Problem ProblemForOpenError(epub::OpenError error)
{
    switch (error) {
        case epub::OpenError::kNone:
            return Problem::kNone;
        case epub::OpenError::kDrm:
            return Problem::kDrm;
        case epub::OpenError::kNotEpub:
        case epub::OpenError::kDamaged:
        case epub::OpenError::kNoContent:
            return Problem::kDamaged;
        case epub::OpenError::kIoError:
            return Problem::kCouldNotRead;
    }
    return Problem::kDamaged;
}

Message MessageFor(Problem problem)
{
    switch (problem) {
        case Problem::kNone:
            return {"", ""};
        case Problem::kDrm:
            return {"This book is locked",
                    "It came from a store or library that locks its books to their own app, so "
                    "Keepsake can't open it. Books without locks, like the free classics from "
                    "Project Gutenberg, work great."};
        case Problem::kPdf:
            return {"PDFs aren't supported yet",
                    "PDF pages are made for bigger screens. A way to read them here is planned "
                    "for a future update."};
        case Problem::kKindle:
            return {"This is a Kindle book",
                    "Kindle books need a Kindle or the Kindle app. If you have the same book as "
                    "an EPUB, that one will open here."};
        case Problem::kComic:
            return {"Comics aren't supported yet",
                    "Picture books and comics are on the list for a future update."};
        case Problem::kDamaged:
            return {"This book seems damaged",
                    "Part of the file is missing or broken. Try copying it onto Keepsake again."};
        case Problem::kNotABook:
            return {"Keepsake can't read this file",
                    "It doesn't look like a book. Keepsake reads EPUB books and plain text "
                    "(.txt) files."};
        case Problem::kEmpty:
            return {"This file is empty",
                    "There's nothing inside it. Try copying it onto Keepsake again."};
        case Problem::kCouldNotRead:
            return {"Keepsake couldn't read this",
                    "Something went wrong reading the memory card. Try again in a moment."};
    }
    return {"Keepsake can't read this file", ""};
}

namespace {

// Windows-1252's 0x80-0x9F row (0 = unassigned).
constexpr uint16_t kCp1252High[32] = {
    0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
    0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178,
};

constexpr uint32_t kReplacement = 0xFFFD;

// Length of the UTF-8 sequence starting with `lead`, or 0 for a byte that can't start one.
size_t Utf8Length(unsigned char lead)
{
    if (lead < 0x80) {
        return 1;
    }
    if (lead >= 0xC2 && lead <= 0xDF) {
        return 2;
    }
    if (lead >= 0xE0 && lead <= 0xEF) {
        return 3;
    }
    if (lead >= 0xF0 && lead <= 0xF4) {
        return 4;
    }
    return 0;
}

}  // namespace

TextEncoding DetectTextEncoding(std::string_view head)
{
    const auto byte = [&head](size_t i) { return static_cast<unsigned char>(head[i]); };
    if (head.size() >= 2 && byte(0) == 0xFF && byte(1) == 0xFE) {
        return TextEncoding::kUtf16Le;
    }
    if (head.size() >= 2 && byte(0) == 0xFE && byte(1) == 0xFF) {
        return TextEncoding::kUtf16Be;
    }
    if (head.size() >= 3 && byte(0) == 0xEF && byte(1) == 0xBB && byte(2) == 0xBF) {
        return TextEncoding::kUtf8;
    }

    // UTF-16 without a BOM: mostly-ASCII text has a NUL in every other byte.
    size_t pairs = 0;
    size_t zero_even = 0;
    size_t zero_odd = 0;
    for (size_t i = 0; i + 1 < head.size(); i += 2) {
        ++pairs;
        zero_even += byte(i) == 0 ? 1 : 0;
        zero_odd += byte(i + 1) == 0 ? 1 : 0;
    }
    if (pairs >= 2) {
        if (zero_odd * 10 >= pairs * 4 && zero_even * 10 < pairs) {
            return TextEncoding::kUtf16Le;
        }
        if (zero_even * 10 >= pairs * 4 && zero_odd * 10 < pairs) {
            return TextEncoding::kUtf16Be;
        }
    }

    // Valid UTF-8 (allowing a sequence cut off by the end of `head`)?
    size_t i = 0;
    while (i < head.size()) {
        const size_t length = Utf8Length(byte(i));
        if (length == 0) {
            return TextEncoding::kLatin1;
        }
        for (size_t k = 1; k < length; ++k) {
            if (i + k >= head.size()) {
                return TextEncoding::kUtf8;  // cut off at the end: fine
            }
            if ((byte(i + k) & 0xC0) != 0x80) {
                return TextEncoding::kLatin1;
            }
        }
        i += length;
    }
    return TextEncoding::kUtf8;
}

bool DecodeCharacter(std::string_view text, size_t pos, TextEncoding encoding,
                     uint32_t* codepoint, size_t* length)
{
    if (pos >= text.size()) {
        return false;
    }
    const auto byte = [&text](size_t i) { return static_cast<unsigned char>(text[i]); };
    switch (encoding) {
        case TextEncoding::kLatin1: {
            const unsigned char b = byte(pos);
            uint32_t cp = b;
            if (b >= 0x80 && b <= 0x9F) {
                cp = kCp1252High[b - 0x80] != 0 ? kCp1252High[b - 0x80] : kReplacement;
            }
            *codepoint = cp;
            *length = 1;
            return true;
        }
        case TextEncoding::kUtf16Le:
        case TextEncoding::kUtf16Be: {
            const bool le = encoding == TextEncoding::kUtf16Le;
            const auto unit = [&](size_t at) -> uint32_t {
                return le ? (byte(at) | (byte(at + 1) << 8)) : ((byte(at) << 8) | byte(at + 1));
            };
            if (pos + 2 > text.size()) {
                return false;
            }
            const uint32_t first = unit(pos);
            if (first >= 0xD800 && first <= 0xDBFF) {
                if (pos + 4 > text.size()) {
                    return false;
                }
                const uint32_t second = unit(pos + 2);
                if (second >= 0xDC00 && second <= 0xDFFF) {
                    *codepoint = 0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00);
                    *length = 4;
                    return true;
                }
            }
            *codepoint = (first >= 0xD800 && first <= 0xDFFF) ? kReplacement : first;
            *length = 2;
            return true;
        }
        case TextEncoding::kUtf8:
        default: {
            const size_t length_needed = Utf8Length(byte(pos));
            if (length_needed == 0) {
                *codepoint = kReplacement;
                *length = 1;
                return true;
            }
            if (pos + length_needed > text.size()) {
                // Cut off at the end of the buffer, unless it's already malformed.
                for (size_t k = 1; pos + k < text.size(); ++k) {
                    if ((byte(pos + k) & 0xC0) != 0x80) {
                        *codepoint = kReplacement;
                        *length = 1;
                        return true;
                    }
                }
                return false;
            }
            uint32_t cp = length_needed == 1   ? byte(pos)
                          : length_needed == 2 ? (byte(pos) & 0x1F)
                          : length_needed == 3 ? (byte(pos) & 0x0F)
                                               : (byte(pos) & 0x07);
            for (size_t k = 1; k < length_needed; ++k) {
                if ((byte(pos + k) & 0xC0) != 0x80) {
                    *codepoint = kReplacement;
                    *length = 1;
                    return true;
                }
                cp = (cp << 6) | (byte(pos + k) & 0x3F);
            }
            *codepoint = cp;
            *length = length_needed;
            return true;
        }
    }
}

const char* TextEncodingName(TextEncoding encoding)
{
    switch (encoding) {
        case TextEncoding::kUtf16Le: return "UTF-16LE";
        case TextEncoding::kUtf16Be: return "UTF-16BE";
        case TextEncoding::kLatin1: return "Windows-1252";
        case TextEncoding::kUtf8:
        default: return "UTF-8";
    }
}

}  // namespace book_formats

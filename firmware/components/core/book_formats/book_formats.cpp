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

}  // namespace book_formats

#ifndef BOOK_FORMATS_BOOK_FORMATS_H_
#define BOOK_FORMATS_BOOK_FORMATS_H_

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "epub_engine/epub_book.h"

// What kind of file a "book" really is, and what to tell the friend when it can't be opened
// (#29). Sniffs content, not just the extension, so a PDF renamed to .epub still gets the
// PDF message.
namespace book_formats {

enum class Format {
    kEpub,
    kText,      // .txt / .md, or unknown extension with text-looking content
    kPdf,
    kMobi,      // MOBI / AZW / AZW3 (PalmDB "BOOKMOBI")
    kKindleKfx, // Amazon's newer KFX container
    kComicZip,  // .cbz
    kOtherZip,
    kEmpty,
    kUnknown,
};

struct Detection {
    Format format = Format::kUnknown;
    bool drm = false;  // known to be DRM-locked from the header alone (MOBI encryption, KFX DRMION)
};

// How many leading bytes Detect() wants (MOBI's encryption flag sits a little way in).
constexpr size_t kSniffBytes = 1024;

// `head` is the first min(file size, kSniffBytes) bytes of the file.
Detection Detect(std::string_view file_name, std::string_view head);

// Plain-text encodings (#27). Windows-1252 stands in for "Latin-1": it's what old .txt files
// from Windows and Project Gutenberg's 8-bit files use, and it's a superset of ISO-8859-1's
// printable range.
enum class TextEncoding {
    kUtf8,     // also plain ASCII, with or without a BOM
    kUtf16Le,
    kUtf16Be,
    kLatin1,   // Windows-1252
};

// Guesses a .txt file's encoding from its first bytes: a BOM if there is one, then NUL bytes
// on alternate positions (UTF-16 without a BOM), then whether the bytes are valid UTF-8 (a
// sequence cut off at the end of `head` is fine). Anything else is Latin-1.
TextEncoding DetectTextEncoding(std::string_view head);

// Decodes the character at `pos`: its code point and length in bytes. False when the bytes
// at `pos` are an incomplete character at the end of `text` (read more and try again). A
// malformed byte decodes as U+FFFD with length 1 (UTF-16: a lone surrogate, length 2).
bool DecodeCharacter(std::string_view text, size_t pos, TextEncoding encoding,
                     uint32_t* codepoint, size_t* length);

const char* TextEncodingName(TextEncoding encoding);

enum class Problem {
    kNone,
    kDrm,           // locked by a store or library (Adobe, Kindle, Apple ...)
    kPdf,           // not supported yet (#66)
    kKindle,        // KFX / not-yet-supported MOBI
    kComic,         // comics aren't supported yet (#59)
    kDamaged,       // a book whose file is broken or incomplete
    kNotABook,      // some other file
    kEmpty,
    kCouldNotRead,  // SD card / I/O trouble
};

// The problem for a detected format before trying to open it (kNone = go ahead and open).
Problem ProblemForDetection(const Detection& detection);

// The problem when the EPUB engine refused a file that looked like an EPUB.
Problem ProblemForOpenError(epub::OpenError error);

// Short, warm, plain-language copy (ASCII, for today's fonts) for the "oops" card.
struct Message {
    const char* title;
    const char* body;
};

Message MessageFor(Problem problem);

}  // namespace book_formats

#endif  // BOOK_FORMATS_BOOK_FORMATS_H_

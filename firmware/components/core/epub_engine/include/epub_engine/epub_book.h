#ifndef EPUB_ENGINE_EPUB_BOOK_H_
#define EPUB_ENGINE_EPUB_BOOK_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "epub_engine/byte_source.h"
#include "epub_engine/content.h"
#include "epub_engine/xhtml_parser.h"
#include "epub_engine/zip_archive.h"

namespace epub {

// Why a book couldn't be opened, in terms the UI turns into a friendly message (#29).
enum class OpenError {
    kNone,
    kNotEpub,     // not a zip, or a zip with no EPUB package (wrong file type, e.g. a renamed PDF)
    kDrm,         // DRM-protected (Adobe ADEPT, Apple FairPlay, Kindle, ...): can't be opened
    kDamaged,     // a zip that is truncated/corrupt, or an unreadable package document
    kNoContent,   // a valid package with nothing readable in its reading order
    kIoError,
};

const char* OpenErrorName(OpenError error);

struct SpineItem {
    std::string path;        // zip path of the content document
    std::string media_type;  // usually application/xhtml+xml
    bool linear = true;      // linear="no" items (pop-up notes) are skipped when paging
    uint32_t bytes = 0;      // uncompressed size: how much of the book it is, for progress
};

struct TocEntry {
    std::string title;
    int depth = 0;           // 0 = top level
    int spine_index = -1;    // -1 when the target isn't in the reading order
    std::string fragment;    // "#id" target inside the document, without '#'
};

struct BookMetadata {
    std::string title;
    std::string author;
    std::string language;
    std::string cover_path;  // zip path of the cover image, if the book declares one
};

struct OpenLimits {
    size_t max_package_bytes = 2u * 1024u * 1024u;  // OPF, NCX, nav
    size_t max_document_bytes = 8u * 1024u * 1024u;  // one chapter's XHTML
    size_t max_toc_entries = 4000;
    size_t max_spine_items = 5000;
};

class EpubBook {
public:
    // Opens the book from `source` (the book keeps it). On failure, returns nullptr and sets
    // *error. Reads only the zip directory and the small package files; chapters are loaded
    // on demand.
    static std::unique_ptr<EpubBook> Open(std::unique_ptr<ByteSource> source, OpenError* error,
                                          const OpenLimits& limits = {});

    const BookMetadata& metadata() const { return metadata_; }
    const std::vector<SpineItem>& spine() const { return spine_; }
    const std::vector<TocEntry>& toc() const { return toc_; }
    const std::string& package_path() const { return package_path_; }

    // Parses spine item `index` into blocks. false if the index is out of range or the
    // document can't be read (the reader shows a "this part couldn't be read" page).
    bool LoadChapter(size_t index, Chapter* chapter, const XhtmlLimits& limits = {}) const;

    // Raw bytes of any file in the book (cover image, stylesheet ...), by zip path.
    bool ReadFile(const std::string& path, size_t max_size, std::string* out) const;

private:
    EpubBook() = default;
    OpenError Load(const OpenLimits& limits);
    bool IsDrmProtected() const;
    bool ParsePackage(const std::string& opf, std::string* toc_path, std::string* nav_path);
    void ParseNav(const std::string& nav_path, const std::string& document);
    void ParseNcx(const std::string& ncx_path, const std::string& document);
    int SpineIndexOf(const std::string& path) const;

    std::unique_ptr<ByteSource> source_;
    ZipArchive zip_;
    OpenLimits limits_;
    std::string package_path_;
    BookMetadata metadata_;
    std::vector<SpineItem> spine_;
    std::vector<TocEntry> toc_;
};

}  // namespace epub

#endif  // EPUB_ENGINE_EPUB_BOOK_H_

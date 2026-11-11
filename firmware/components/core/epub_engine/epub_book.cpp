#include "epub_engine/epub_book.h"

#include <algorithm>
#include <cctype>
#include <string_view>

#include "epub_engine/href.h"
#include "epub_engine/xml_tokenizer.h"

namespace epub {
namespace {

constexpr const char* kContainerPath = "META-INF/container.xml";
constexpr size_t kMaxContainerBytes = 64u * 1024u;
constexpr size_t kMaxEncryptionBytes = 256u * 1024u;
constexpr size_t kMaxTitleBytes = 512;

// Encryption algorithms that only obfuscate embedded fonts; the text is readable.
constexpr std::string_view kFontObfuscationAlgorithms[] = {
    "http://www.idpf.org/2008/embedding",
    "http://ns.adobe.com/pdf/enc#RC",
};

// Collapses whitespace runs to one space and trims both ends, capped at `max_bytes`.
std::string CollapseWhitespace(std::string_view text, size_t max_bytes = kMaxTitleBytes)
{
    std::string out;
    bool space = false;
    for (const char c : text) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') {
            space = !out.empty();
            continue;
        }
        if (space) {
            out.push_back(' ');
            space = false;
        }
        if (out.size() >= max_bytes) {
            break;
        }
        out.push_back(c);
    }
    return out;
}

bool HasToken(const std::string& list, std::string_view token)
{
    size_t start = 0;
    while (start < list.size()) {
        size_t end = list.find(' ', start);
        if (end == std::string::npos) {
            end = list.size();
        }
        if (std::string_view(list).substr(start, end - start) == token) {
            return true;
        }
        start = end + 1;
    }
    return false;
}

bool EndsWithIgnoreCase(std::string_view text, std::string_view suffix)
{
    if (text.size() < suffix.size()) {
        return false;
    }
    for (size_t i = 0; i < suffix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(text[text.size() - suffix.size() + i])) !=
            std::tolower(static_cast<unsigned char>(suffix[i]))) {
            return false;
        }
    }
    return true;
}

struct ManifestItem {
    std::string id;
    std::string path;
    std::string media_type;
    std::string properties;
};

}  // namespace

const char* OpenErrorName(OpenError error)
{
    switch (error) {
        case OpenError::kNone:
            return "none";
        case OpenError::kNotEpub:
            return "not an EPUB";
        case OpenError::kDrm:
            return "DRM-protected";
        case OpenError::kDamaged:
            return "damaged";
        case OpenError::kNoContent:
            return "no readable content";
        case OpenError::kIoError:
            return "I/O error";
    }
    return "unknown";
}

std::unique_ptr<EpubBook> EpubBook::Open(std::unique_ptr<ByteSource> source, OpenError* error,
                                         const OpenLimits& limits)
{
    OpenError ignored = OpenError::kNone;
    if (error == nullptr) {
        error = &ignored;
    }
    if (source == nullptr) {
        *error = OpenError::kIoError;
        return nullptr;
    }
    std::unique_ptr<EpubBook> book(new EpubBook());
    book->source_ = std::move(source);
    *error = book->Load(limits);
    if (*error != OpenError::kNone) {
        return nullptr;
    }
    return book;
}

OpenError EpubBook::Load(const OpenLimits& limits)
{
    limits_ = limits;
    switch (zip_.Open(source_.get())) {
        case ZipStatus::kOk:
            break;
        case ZipStatus::kNotZip:
            return OpenError::kNotEpub;
        case ZipStatus::kIoError:
            return OpenError::kIoError;
        default:
            return OpenError::kDamaged;
    }

    if (IsDrmProtected()) {
        return OpenError::kDrm;
    }

    // container.xml names the package document; fall back to the first .opf in the zip.
    std::string container;
    if (zip_.Read(kContainerPath, kMaxContainerBytes, &container) == ZipStatus::kOk) {
        XmlTokenizer tokens(container);
        while (tokens.Next() != XmlTokenizer::Type::kEnd) {
            if (tokens.type() == XmlTokenizer::Type::kStartTag &&
                tokens.local_name() == "rootfile") {
                const std::string* path = tokens.Attribute("full-path");
                const std::string* media_type = tokens.Attribute("media-type");
                if (path != nullptr && !path->empty() &&
                    (media_type == nullptr ||
                     *media_type == "application/oebps-package+xml")) {
                    package_path_ = ResolveHref("", *path);
                    break;
                }
            }
        }
    }
    if (package_path_.empty() || zip_.Find(package_path_) == nullptr) {
        package_path_.clear();
        for (const ZipEntry& entry : zip_.entries()) {
            if (EndsWithIgnoreCase(entry.name, ".opf")) {
                package_path_ = entry.name;
                break;
            }
        }
    }
    if (package_path_.empty()) {
        return OpenError::kNotEpub;
    }

    std::string opf;
    if (zip_.Read(package_path_, limits_.max_package_bytes, &opf) != ZipStatus::kOk) {
        return OpenError::kDamaged;
    }
    std::string ncx_path;
    std::string nav_path;
    if (!ParsePackage(opf, &ncx_path, &nav_path)) {
        return OpenError::kDamaged;
    }
    if (spine_.empty()) {
        return OpenError::kNoContent;
    }

    std::string document;
    if (!nav_path.empty() &&
        zip_.Read(nav_path, limits_.max_package_bytes, &document) == ZipStatus::kOk) {
        ParseNav(nav_path, document);
    }
    if (toc_.empty() && !ncx_path.empty() &&
        zip_.Read(ncx_path, limits_.max_package_bytes, &document) == ZipStatus::kOk) {
        ParseNcx(ncx_path, document);
    }
    return OpenError::kNone;
}

bool EpubBook::IsDrmProtected() const
{
    // Adobe ADEPT licence, Apple FairPlay.
    if (zip_.Find("META-INF/rights.xml") != nullptr || zip_.Find("META-INF/sinf.xml") != nullptr) {
        return true;
    }
    const ZipEntry* encryption = zip_.Find("META-INF/encryption.xml");
    if (encryption == nullptr) {
        return false;
    }
    std::string xml;
    if (zip_.Read(*encryption, kMaxEncryptionBytes, &xml) != ZipStatus::kOk) {
        return true;  // can't tell what's encrypted: assume the worst
    }
    XmlTokenizer tokens(xml);
    while (tokens.Next() != XmlTokenizer::Type::kEnd) {
        if (tokens.type() != XmlTokenizer::Type::kStartTag ||
            tokens.local_name() != "encryptionmethod") {
            continue;
        }
        const std::string* algorithm = tokens.Attribute("algorithm");
        if (algorithm == nullptr) {
            return true;
        }
        const bool font_only = std::any_of(
            std::begin(kFontObfuscationAlgorithms), std::end(kFontObfuscationAlgorithms),
            [&](std::string_view known) { return *algorithm == known; });
        if (!font_only) {
            return true;
        }
    }
    return false;
}

bool EpubBook::ParsePackage(const std::string& opf, std::string* ncx_path, std::string* nav_path)
{
    const std::string base = DirectoryOf(package_path_);
    std::vector<ManifestItem> manifest;
    std::vector<std::pair<std::string, bool>> spine_refs;  // idref, linear
    std::string spine_toc_id;
    std::string cover_id;

    enum class Capture { kNone, kTitle, kCreator, kLanguage };
    Capture capture = Capture::kNone;
    std::string captured;
    bool in_metadata = false;
    bool saw_package = false;

    XmlTokenizer tokens(opf);
    while (tokens.Next() != XmlTokenizer::Type::kEnd) {
        const std::string& name = tokens.local_name();
        switch (tokens.type()) {
            case XmlTokenizer::Type::kStartTag:
                if (name == "package") {
                    saw_package = true;
                } else if (name == "metadata") {
                    in_metadata = true;
                } else if (in_metadata && capture == Capture::kNone && !tokens.self_closing()) {
                    if (name == "title" && metadata_.title.empty()) {
                        capture = Capture::kTitle;
                    } else if (name == "creator" && metadata_.author.empty()) {
                        capture = Capture::kCreator;
                    } else if (name == "language" && metadata_.language.empty()) {
                        capture = Capture::kLanguage;
                    }
                    captured.clear();
                }
                if (in_metadata && name == "meta") {
                    const std::string* meta_name = tokens.Attribute("name");
                    const std::string* content = tokens.Attribute("content");
                    if (meta_name != nullptr && *meta_name == "cover" && content != nullptr) {
                        cover_id = *content;
                    }
                } else if (name == "item") {
                    const std::string* id = tokens.Attribute("id");
                    const std::string* href = tokens.Attribute("href");
                    if (id == nullptr || href == nullptr || manifest.size() >= limits_.max_spine_items * 4) {
                        break;
                    }
                    ManifestItem item;
                    item.id = *id;
                    item.path = ResolveHref(base, *href);
                    if (const std::string* type = tokens.Attribute("media-type")) {
                        item.media_type = *type;
                    }
                    if (const std::string* properties = tokens.Attribute("properties")) {
                        item.properties = *properties;
                    }
                    manifest.push_back(std::move(item));
                } else if (name == "spine") {
                    if (const std::string* toc = tokens.Attribute("toc")) {
                        spine_toc_id = *toc;
                    }
                } else if (name == "itemref") {
                    const std::string* idref = tokens.Attribute("idref");
                    if (idref != nullptr && spine_refs.size() < limits_.max_spine_items) {
                        const std::string* linear = tokens.Attribute("linear");
                        spine_refs.emplace_back(*idref, linear == nullptr || *linear != "no");
                    }
                }
                break;
            case XmlTokenizer::Type::kText:
                if (capture != Capture::kNone && captured.size() < kMaxTitleBytes * 2) {
                    captured += tokens.text();
                }
                break;
            case XmlTokenizer::Type::kEndTag:
                if (name == "metadata") {
                    in_metadata = false;
                    capture = Capture::kNone;
                } else if (capture != Capture::kNone &&
                           ((capture == Capture::kTitle && name == "title") ||
                            (capture == Capture::kCreator && name == "creator") ||
                            (capture == Capture::kLanguage && name == "language"))) {
                    std::string value = CollapseWhitespace(captured);
                    if (capture == Capture::kTitle) {
                        metadata_.title = std::move(value);
                    } else if (capture == Capture::kCreator) {
                        metadata_.author = std::move(value);
                    } else {
                        metadata_.language = std::move(value);
                    }
                    capture = Capture::kNone;
                }
                break;
            case XmlTokenizer::Type::kEnd:
                break;
        }
    }
    if (!saw_package && manifest.empty()) {
        return false;
    }

    auto find_item = [&](const std::string& id) -> const ManifestItem* {
        for (const ManifestItem& item : manifest) {
            if (item.id == id) {
                return &item;
            }
        }
        return nullptr;
    };

    for (const auto& [idref, linear] : spine_refs) {
        const ManifestItem* item = find_item(idref);
        if (item == nullptr || item->path.empty()) {
            continue;
        }
        spine_.push_back(SpineItem{item->path, item->media_type, linear});
    }

    for (const ManifestItem& item : manifest) {
        if (HasToken(item.properties, "nav") && nav_path->empty()) {
            *nav_path = item.path;
        }
        if (HasToken(item.properties, "cover-image") && metadata_.cover_path.empty()) {
            metadata_.cover_path = item.path;
        }
    }
    if (metadata_.cover_path.empty() && !cover_id.empty()) {
        if (const ManifestItem* item = find_item(cover_id)) {
            metadata_.cover_path = item->path;
        }
    }
    if (!spine_toc_id.empty()) {
        if (const ManifestItem* item = find_item(spine_toc_id)) {
            *ncx_path = item->path;
        }
    }
    if (ncx_path->empty()) {
        for (const ManifestItem& item : manifest) {
            if (item.media_type == "application/x-dtbncx+xml") {
                *ncx_path = item.path;
                break;
            }
        }
    }
    return true;
}

int EpubBook::SpineIndexOf(const std::string& path) const
{
    for (size_t i = 0; i < spine_.size(); ++i) {
        if (spine_[i].path == path) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void EpubBook::ParseNav(const std::string& nav_path, const std::string& document)
{
    const std::string base = DirectoryOf(nav_path);
    XmlTokenizer tokens(document);
    int nav_depth = 0;        // >0 while inside the TOC <nav>
    bool found_toc = false;
    int list_depth = 0;
    bool in_link = false;
    std::string link_href;
    std::string link_text;

    // Two passes at most: first look for <nav epub:type="toc">; if the document has no
    // typed nav, use its first <nav>.
    for (int pass = 0; pass < 2 && toc_.empty(); ++pass) {
        tokens = XmlTokenizer(document);
        nav_depth = 0;
        list_depth = 0;
        in_link = false;
        while (tokens.Next() != XmlTokenizer::Type::kEnd && toc_.size() < limits_.max_toc_entries) {
            const std::string& name = tokens.local_name();
            if (tokens.type() == XmlTokenizer::Type::kStartTag) {
                if (name == "nav") {
                    if (nav_depth > 0) {
                        ++nav_depth;
                    } else if (!found_toc) {
                        const std::string* type = tokens.Attribute("type");
                        if (pass == 1 || (type != nullptr && HasToken(*type, "toc"))) {
                            nav_depth = 1;
                            found_toc = true;
                        }
                    }
                } else if (nav_depth > 0 && name == "ol") {
                    ++list_depth;
                } else if (nav_depth > 0 && name == "a" && !tokens.self_closing()) {
                    const std::string* href = tokens.Attribute("href");
                    in_link = true;
                    link_href = href != nullptr ? *href : std::string();
                    link_text.clear();
                }
            } else if (tokens.type() == XmlTokenizer::Type::kText) {
                if (in_link && link_text.size() < kMaxTitleBytes * 2) {
                    link_text += tokens.text();
                }
            } else if (tokens.type() == XmlTokenizer::Type::kEndTag && nav_depth > 0) {
                if (name == "nav") {
                    if (--nav_depth == 0) {
                        break;
                    }
                } else if (name == "ol") {
                    list_depth = std::max(0, list_depth - 1);
                } else if (name == "a" && in_link) {
                    in_link = false;
                    TocEntry entry;
                    entry.title = CollapseWhitespace(link_text);
                    entry.depth = std::max(0, list_depth - 1);
                    const std::string path = link_href.empty()
                                                 ? std::string()
                                                 : ResolveHref(base, link_href, &entry.fragment);
                    entry.spine_index = path.empty() ? -1 : SpineIndexOf(path);
                    if (!entry.title.empty()) {
                        toc_.push_back(std::move(entry));
                    }
                }
            }
        }
    }
}

void EpubBook::ParseNcx(const std::string& ncx_path, const std::string& document)
{
    const std::string base = DirectoryOf(ncx_path);
    struct Point {
        std::string label;
        bool in_text = false;
        bool emitted = false;
    };
    std::vector<Point> points;
    bool in_nav_map = false;

    XmlTokenizer tokens(document);
    while (tokens.Next() != XmlTokenizer::Type::kEnd && toc_.size() < limits_.max_toc_entries) {
        const std::string& name = tokens.local_name();
        switch (tokens.type()) {
            case XmlTokenizer::Type::kStartTag:
                if (name == "navmap") {
                    in_nav_map = true;
                } else if (in_nav_map && name == "navpoint" && !tokens.self_closing()) {
                    if (points.size() < 64) {
                        points.push_back(Point{});
                    }
                } else if (in_nav_map && name == "text" && !points.empty()) {
                    points.back().in_text = true;
                } else if (in_nav_map && name == "content" && !points.empty() &&
                           !points.back().emitted) {
                    const std::string* source = tokens.Attribute("src");
                    TocEntry entry;
                    entry.title = CollapseWhitespace(points.back().label);
                    entry.depth = static_cast<int>(points.size()) - 1;
                    const std::string path =
                        source == nullptr ? std::string() : ResolveHref(base, *source, &entry.fragment);
                    entry.spine_index = path.empty() ? -1 : SpineIndexOf(path);
                    points.back().emitted = true;
                    if (!entry.title.empty()) {
                        toc_.push_back(std::move(entry));
                    }
                }
                break;
            case XmlTokenizer::Type::kText:
                if (!points.empty() && points.back().in_text &&
                    points.back().label.size() < kMaxTitleBytes * 2) {
                    points.back().label += tokens.text();
                }
                break;
            case XmlTokenizer::Type::kEndTag:
                if (name == "navmap") {
                    in_nav_map = false;
                } else if (name == "text" && !points.empty()) {
                    points.back().in_text = false;
                } else if (name == "navpoint" && !points.empty()) {
                    points.pop_back();
                }
                break;
            case XmlTokenizer::Type::kEnd:
                break;
        }
    }
}

bool EpubBook::LoadChapter(size_t index, Chapter* chapter, const XhtmlLimits& limits) const
{
    if (chapter == nullptr || index >= spine_.size()) {
        return false;
    }
    *chapter = Chapter{};
    const SpineItem& item = spine_[index];
    if (item.media_type.rfind("image/", 0) == 0) {
        // Some picture books put images straight into the reading order.
        Block image;
        image.type = BlockType::kImage;
        image.image_href = item.path;
        chapter->blocks.push_back(std::move(image));
        return true;
    }
    std::string document;
    if (zip_.Read(item.path, limits_.max_document_bytes, &document) != ZipStatus::kOk) {
        return false;
    }
    *chapter = ParseXhtml(document, item.path, limits);
    return true;
}

bool EpubBook::ReadFile(const std::string& path, size_t max_size, std::string* out) const
{
    return zip_.Read(path, max_size, out) == ZipStatus::kOk;
}

}  // namespace epub

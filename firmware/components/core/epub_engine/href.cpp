#include "epub_engine/href.h"

#include <cctype>
#include <vector>

namespace epub {
namespace {

int HexValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string PercentDecode(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size()) {
            const int high = HexValue(text[i + 1]);
            const int low = HexValue(text[i + 2]);
            if (high >= 0 && low >= 0) {
                out.push_back(static_cast<char>((high << 4) | low));
                i += 2;
                continue;
            }
        }
        out.push_back(text[i]);
    }
    return out;
}

bool HasScheme(std::string_view href)
{
    // "http:", "mailto:", "data:" ... A scheme is letters/digits/+-. before the first ':',
    // and must come before any '/'.
    for (size_t i = 0; i < href.size(); ++i) {
        const char c = href[i];
        if (c == ':') {
            return i > 0;
        }
        if (c == '/' || c == '#' || c == '?') {
            return false;
        }
        if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '+' && c != '-' && c != '.') {
            return false;
        }
    }
    return false;
}

}  // namespace

std::string DirectoryOf(std::string_view path)
{
    const size_t slash = path.rfind('/');
    return slash == std::string_view::npos ? std::string() : std::string(path.substr(0, slash + 1));
}

std::string ResolveHref(std::string_view base_directory, std::string_view href,
                        std::string* fragment)
{
    if (fragment != nullptr) {
        fragment->clear();
    }
    if (HasScheme(href)) {
        return {};
    }
    const size_t hash = href.find('#');
    if (hash != std::string_view::npos) {
        if (fragment != nullptr) {
            *fragment = PercentDecode(href.substr(hash + 1));
        }
        href = href.substr(0, hash);
    }
    const size_t query = href.find('?');
    if (query != std::string_view::npos) {
        href = href.substr(0, query);
    }

    // A bare "#fragment" (empty href) resolves to the base itself: callers that need "this
    // document" pass the document's own path as the base.
    std::string combined;
    if (!href.empty() && href.front() == '/') {
        combined = PercentDecode(href.substr(1));
    } else {
        combined = std::string(base_directory) + PercentDecode(href);
    }

    std::vector<std::string> segments;
    size_t start = 0;
    while (start <= combined.size()) {
        size_t end = combined.find('/', start);
        if (end == std::string::npos) {
            end = combined.size();
        }
        const std::string segment = combined.substr(start, end - start);
        if (segment == "..") {
            if (!segments.empty()) {
                segments.pop_back();
            }
        } else if (!segment.empty() && segment != ".") {
            segments.push_back(segment);
        }
        start = end + 1;
    }

    std::string out;
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) {
            out.push_back('/');
        }
        out += segments[i];
    }
    return out;
}

}  // namespace epub

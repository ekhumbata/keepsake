#include "library/library.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace library {
namespace {

constexpr std::string_view kIndexHeader = "keepsake-library 1";

std::string Lower(std::string_view text)
{
    std::string out(text);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string Escape(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::string Unescape(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\\' || i + 1 >= text.size()) {
            out.push_back(text[i]);
            continue;
        }
        const char next = text[++i];
        out.push_back(next == 't' ? '\t' : next == 'n' ? '\n' : next == 'r' ? '\r' : next);
    }
    return out;
}

bool ParseInt(std::string_view text, int64_t* out)
{
    if (text.empty() || text.size() > 20) {
        return false;
    }
    const std::string copy(text);
    char* end = nullptr;
    const long long value = std::strtoll(copy.c_str(), &end, 10);
    if (end == copy.c_str() || *end != '\0') {
        return false;
    }
    *out = value;
    return true;
}

}  // namespace

std::string Extension(std::string_view path)
{
    const size_t slash = path.find_last_of('/');
    const size_t dot = path.rfind('.');
    if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash) ||
        dot + 1 >= path.size()) {
        return {};
    }
    return Lower(path.substr(dot + 1));
}

std::string TitleFromPath(std::string_view path)
{
    const size_t slash = path.find_last_of('/');
    std::string_view name = slash == std::string_view::npos ? path : path.substr(slash + 1);
    const size_t dot = name.rfind('.');
    if (dot != std::string_view::npos && dot > 0) {
        name = name.substr(0, dot);
    }
    std::string title;
    bool space = false;
    for (const char c : name) {
        if (c == '_' || c == '-' || c == ' ' || c == '.') {
            space = !title.empty();
            continue;
        }
        if (space) {
            title.push_back(' ');
            space = false;
        }
        title.push_back(c);
    }
    if (title.empty()) {
        return std::string(name.empty() ? path : name);
    }
    title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(title[0])));
    return title;
}

std::string SerializeIndex(const std::vector<BookRecord>& records)
{
    std::string out(kIndexHeader);
    out.push_back('\n');
    for (const BookRecord& r : records) {
        out += Escape(r.path) + '\t' + Escape(r.title) + '\t' + Escape(r.author) + '\t' +
               std::to_string(r.size_bytes) + '\t' + std::to_string(r.modified_time) + '\t' +
               std::to_string(r.last_opened) + '\t' + std::to_string(r.progress_percent) + '\n';
    }
    return out;
}

std::vector<BookRecord> ParseIndex(std::string_view text)
{
    std::vector<BookRecord> records;
    size_t start = 0;
    bool first = true;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        std::string_view line = text.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (first) {
            first = false;
            if (line != kIndexHeader) {
                return {};  // unknown version: rebuild from the books
            }
            continue;
        }
        std::vector<std::string_view> fields;
        size_t field_start = 0;
        while (fields.size() < 8) {
            const size_t tab = line.find('\t', field_start);
            fields.push_back(line.substr(field_start, tab == std::string_view::npos
                                                          ? std::string_view::npos
                                                          : tab - field_start));
            if (tab == std::string_view::npos) {
                break;
            }
            field_start = tab + 1;
        }
        if (fields.size() != 7 || fields[0].empty()) {
            continue;
        }
        BookRecord record;
        int64_t size = 0;
        int64_t modified = 0;
        int64_t opened = 0;
        int64_t progress = 0;
        if (!ParseInt(fields[3], &size) || !ParseInt(fields[4], &modified) ||
            !ParseInt(fields[5], &opened) || !ParseInt(fields[6], &progress) || size < 0) {
            continue;
        }
        record.path = Unescape(fields[0]);
        record.title = Unescape(fields[1]);
        record.author = Unescape(fields[2]);
        record.size_bytes = static_cast<uint64_t>(size);
        record.modified_time = modified;
        record.last_opened = opened;
        record.progress_percent = static_cast<int>(std::clamp<int64_t>(progress, -1, 100));
        records.push_back(std::move(record));
    }
    return records;
}

void SortForShelf(std::vector<BookRecord>* records)
{
    std::stable_sort(records->begin(), records->end(), [](const BookRecord& a, const BookRecord& b) {
        if ((a.last_opened > 0) != (b.last_opened > 0)) {
            return a.last_opened > 0;
        }
        if (a.last_opened != b.last_opened) {
            return a.last_opened > b.last_opened;
        }
        const std::string ta = Lower(a.title);
        const std::string tb = Lower(b.title);
        if (ta != tb) {
            return ta < tb;
        }
        return a.path < b.path;
    });
}

std::string ShelfLabel(const BookRecord& record)
{
    const std::string title = record.title.empty() ? TitleFromPath(record.path) : record.title;
    if (record.progress_percent >= 100) {
        return title + " - Done";
    }
    if (record.progress_percent > 0) {
        return title + " - " + std::to_string(record.progress_percent) + "%";
    }
    return title;
}

}  // namespace library

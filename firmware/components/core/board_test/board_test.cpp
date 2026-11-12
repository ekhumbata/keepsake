#include "board_test/board_test.h"

#include <cmath>
#include <cstdio>

namespace board_test {

bool ParseCommandLine(std::string_view line, std::vector<std::string>* words)
{
    words->clear();
    std::string current;
    bool in_word = false;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (quoted) {
            if (c == '\\' && i + 1 < line.size() && (line[i + 1] == '"' || line[i + 1] == '\\')) {
                current.push_back(line[++i]);
            } else if (c == '"') {
                quoted = false;
            } else {
                current.push_back(c);
            }
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (in_word) {
                words->push_back(current);
                current.clear();
                in_word = false;
            }
            continue;
        }
        in_word = true;
        if (c == '"') {
            quoted = true;
        } else {
            current.push_back(c);
        }
    }
    if (quoted) {
        words->clear();
        return false;
    }
    if (in_word) {
        words->push_back(current);
    }
    if (words->empty() || (*words)[0] != kCommandPrefix) {
        words->clear();
        return false;
    }
    words->erase(words->begin());
    return true;
}

std::string JsonEscape(std::string_view text)
{
    std::string out;
    out.reserve(text.size() + 2);
    for (const char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char hex[8];
                    std::snprintf(hex, sizeof(hex), "\\u%04x", static_cast<unsigned char>(c));
                    out += hex;
                } else {
                    out.push_back(c);
                }
        }
    }
    return out;
}

void JsonWriter::Key(std::string_view key)
{
    if (!first_) {
        body_ += ",";
    }
    first_ = false;
    body_ += "\"" + JsonEscape(key) + "\":";
}

JsonWriter& JsonWriter::Add(std::string_view key, std::string_view value)
{
    Key(key);
    body_ += "\"" + JsonEscape(value) + "\"";
    return *this;
}

JsonWriter& JsonWriter::Add(std::string_view key, bool value)
{
    Key(key);
    body_ += value ? "true" : "false";
    return *this;
}

JsonWriter& JsonWriter::Add(std::string_view key, int64_t value)
{
    Key(key);
    body_ += std::to_string(value);
    return *this;
}

JsonWriter& JsonWriter::Add(std::string_view key, double value)
{
    Key(key);
    if (!std::isfinite(value)) {
        body_ += "null";
        return *this;
    }
    char text[32];
    std::snprintf(text, sizeof(text), "%.4g", value);
    body_ += text;
    return *this;
}

JsonWriter& JsonWriter::AddRaw(std::string_view key, std::string_view json)
{
    Key(key);
    body_ += std::string(json);
    return *this;
}

namespace {

constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int Base64Value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

}  // namespace

std::string Base64Encode(const uint8_t* data, size_t size)
{
    std::string out;
    out.reserve((size + 2) / 3 * 4);
    for (size_t i = 0; i < size; i += 3) {
        const uint32_t b0 = data[i];
        const uint32_t b1 = i + 1 < size ? data[i + 1] : 0;
        const uint32_t b2 = i + 2 < size ? data[i + 2] : 0;
        const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;
        out.push_back(kAlphabet[(triple >> 18) & 0x3F]);
        out.push_back(kAlphabet[(triple >> 12) & 0x3F]);
        out.push_back(i + 1 < size ? kAlphabet[(triple >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < size ? kAlphabet[triple & 0x3F] : '=');
    }
    return out;
}

bool Base64Decode(std::string_view text, std::vector<uint8_t>* out)
{
    out->clear();
    if (text.size() % 4 != 0) {
        return false;
    }
    for (size_t i = 0; i < text.size(); i += 4) {
        int values[4];
        int padding = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = text[i + static_cast<size_t>(k)];
            if (c == '=' && i + 4 == text.size() && k >= 2) {
                values[k] = 0;
                ++padding;
                continue;
            }
            if (padding > 0) {
                return false;  // data after padding
            }
            values[k] = Base64Value(c);
            if (values[k] < 0) {
                return false;
            }
        }
        const uint32_t triple = (static_cast<uint32_t>(values[0]) << 18) |
                                (static_cast<uint32_t>(values[1]) << 12) |
                                (static_cast<uint32_t>(values[2]) << 6) |
                                static_cast<uint32_t>(values[3]);
        out->push_back(static_cast<uint8_t>(triple >> 16));
        if (padding < 2) {
            out->push_back(static_cast<uint8_t>((triple >> 8) & 0xFF));
        }
        if (padding < 1) {
            out->push_back(static_cast<uint8_t>(triple & 0xFF));
        }
    }
    return true;
}

bool IsSafeCardPath(std::string_view path)
{
    if (path.empty() || path.size() > 128 || path[0] != '/') {
        return false;
    }
    if (path.find("//") != std::string_view::npos) {
        return false;
    }
    for (const char c : path) {
        if (c < 0x20 || c > 0x7E || c == '\\') {
            return false;
        }
    }
    // No ".." path component.
    size_t start = 1;
    while (start <= path.size()) {
        size_t end = path.find('/', start);
        if (end == std::string_view::npos) {
            end = path.size();
        }
        if (path.substr(start, end - start) == "..") {
            return false;
        }
        start = end + 1;
    }
    return true;
}

}  // namespace board_test

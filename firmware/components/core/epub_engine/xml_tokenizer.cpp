#include "epub_engine/xml_tokenizer.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>

namespace epub {
namespace {

bool IsNameStart(char c)
{
    const auto u = static_cast<unsigned char>(c);
    return std::isalpha(u) != 0 || c == '_' || c == ':' || u >= 0x80;
}

bool IsNameChar(char c)
{
    const auto u = static_cast<unsigned char>(c);
    return IsNameStart(c) || std::isdigit(u) != 0 || c == '-' || c == '.';
}

bool IsSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

void AppendUtf8(uint32_t codepoint, std::string* out)
{
    if (codepoint == 0 || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
        codepoint = 0xFFFD;
    }
    if (codepoint < 0x80) {
        out->push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800) {
        out->push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        out->push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint < 0x10000) {
        out->push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        out->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out->push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        out->push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        out->push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out->push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

struct NamedEntity {
    const char* name;
    uint32_t codepoint;
};

// The XML five plus the HTML entities that show up in real EPUB text.
constexpr NamedEntity kNamedEntities[] = {
    {"amp", '&'},       {"lt", '<'},         {"gt", '>'},         {"quot", '"'},
    {"apos", '\''},     {"nbsp", 0x00A0},    {"shy", 0x00AD},     {"copy", 0x00A9},
    {"reg", 0x00AE},    {"trade", 0x2122},   {"deg", 0x00B0},     {"middot", 0x00B7},
    {"para", 0x00B6},   {"sect", 0x00A7},    {"laquo", 0x00AB},   {"raquo", 0x00BB},
    {"lsquo", 0x2018},  {"rsquo", 0x2019},   {"sbquo", 0x201A},   {"ldquo", 0x201C},
    {"rdquo", 0x201D},  {"bdquo", 0x201E},   {"ndash", 0x2013},   {"mdash", 0x2014},
    {"hellip", 0x2026}, {"bull", 0x2022},    {"prime", 0x2032},   {"Prime", 0x2033},
    {"times", 0x00D7},  {"divide", 0x00F7},  {"frac12", 0x00BD},  {"frac14", 0x00BC},
    {"frac34", 0x00BE}, {"iexcl", 0x00A1},   {"iquest", 0x00BF},  {"pound", 0x00A3},
    {"euro", 0x20AC},   {"cent", 0x00A2},    {"yen", 0x00A5},     {"dagger", 0x2020},
    {"Dagger", 0x2021}, {"aacute", 0x00E1},  {"agrave", 0x00E0},  {"acirc", 0x00E2},
    {"auml", 0x00E4},   {"atilde", 0x00E3},  {"aring", 0x00E5},   {"aelig", 0x00E6},
    {"ccedil", 0x00E7}, {"eacute", 0x00E9},  {"egrave", 0x00E8},  {"ecirc", 0x00EA},
    {"euml", 0x00EB},   {"iacute", 0x00ED},  {"igrave", 0x00EC},  {"icirc", 0x00EE},
    {"iuml", 0x00EF},   {"ntilde", 0x00F1},  {"oacute", 0x00F3},  {"ograve", 0x00F2},
    {"ocirc", 0x00F4},  {"ouml", 0x00F6},    {"otilde", 0x00F5},  {"oslash", 0x00F8},
    {"szlig", 0x00DF},  {"uacute", 0x00FA},  {"ugrave", 0x00F9},  {"ucirc", 0x00FB},
    {"uuml", 0x00FC},   {"yacute", 0x00FD},  {"yuml", 0x00FF},    {"Aacute", 0x00C1},
    {"Agrave", 0x00C0}, {"Auml", 0x00C4},    {"Ccedil", 0x00C7},  {"Eacute", 0x00C9},
    {"Egrave", 0x00C8}, {"Ntilde", 0x00D1},  {"Oacute", 0x00D3},  {"Ouml", 0x00D6},
    {"Uacute", 0x00DA}, {"Uuml", 0x00DC},    {"oelig", 0x0153},   {"OElig", 0x0152},
    {"thinsp", 0x2009}, {"ensp", 0x2002},    {"emsp", 0x2003},    {"zwnj", 0x200C},
    {"zwj", 0x200D},
};

bool LookupEntity(std::string_view name, uint32_t* codepoint)
{
    for (const NamedEntity& entity : kNamedEntities) {
        if (name == entity.name) {
            *codepoint = entity.codepoint;
            return true;
        }
    }
    return false;
}

}  // namespace

std::string DecodeEntities(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size());
    size_t i = 0;
    while (i < raw.size()) {
        const char c = raw[i];
        if (c != '&') {
            out.push_back(c);
            ++i;
            continue;
        }
        // Entity names are short; look for ';' within a small window only.
        const size_t limit = std::min(raw.size(), i + 12);
        size_t semicolon = std::string_view::npos;
        for (size_t j = i + 1; j < limit; ++j) {
            if (raw[j] == ';') {
                semicolon = j;
                break;
            }
            if (raw[j] == '&' || IsSpace(raw[j]) || raw[j] == '<') {
                break;
            }
        }
        if (semicolon == std::string_view::npos) {
            out.push_back('&');
            ++i;
            continue;
        }
        const std::string_view body = raw.substr(i + 1, semicolon - i - 1);
        uint32_t codepoint = 0;
        bool decoded = false;
        if (body.size() >= 2 && body[0] == '#') {
            const bool hex = body[1] == 'x' || body[1] == 'X';
            const std::string_view digits = body.substr(hex ? 2 : 1);
            if (!digits.empty() && digits.size() <= 8) {
                decoded = true;
                for (const char d : digits) {
                    const auto u = static_cast<unsigned char>(d);
                    int value = -1;
                    if (std::isdigit(u) != 0) {
                        value = d - '0';
                    } else if (hex && std::isxdigit(u) != 0) {
                        value = std::tolower(u) - 'a' + 10;
                    }
                    if (value < 0) {
                        decoded = false;
                        break;
                    }
                    codepoint = codepoint * (hex ? 16u : 10u) + static_cast<uint32_t>(value);
                    if (codepoint > 0x10FFFF) {
                        codepoint = 0x110000;  // clamp; becomes U+FFFD
                    }
                }
            }
        } else {
            decoded = LookupEntity(body, &codepoint);
        }
        if (decoded) {
            AppendUtf8(codepoint, &out);
        } else {
            out.append(raw.substr(i, semicolon - i + 1));
        }
        i = semicolon + 1;
    }
    return out;
}

std::string LocalName(std::string_view qualified_name)
{
    const size_t colon = qualified_name.rfind(':');
    const std::string_view local =
        colon == std::string_view::npos ? qualified_name : qualified_name.substr(colon + 1);
    std::string lower(local);
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower;
}

const std::string* XmlTokenizer::Attribute(std::string_view local_name) const
{
    for (const auto& [key, value] : attributes_) {
        if (LocalName(key) == local_name) {
            return &value;
        }
    }
    return nullptr;
}

void XmlTokenizer::SkipPast(std::string_view terminator)
{
    const size_t found = input_.find(terminator, position_);
    position_ = found == std::string_view::npos ? input_.size() : found + terminator.size();
}

XmlTokenizer::Type XmlTokenizer::Next()
{
    while (position_ < input_.size()) {
        if (input_[position_] != '<') {
            const size_t next = input_.find('<', position_);
            const size_t end = next == std::string_view::npos ? input_.size() : next;
            text_ = DecodeEntities(input_.substr(position_, end - position_));
            position_ = end;
            return type_ = Type::kText;
        }

        const std::string_view rest = input_.substr(position_);
        if (rest.substr(0, 4) == "<!--") {
            position_ += 4;
            SkipPast("-->");
            continue;
        }
        if (rest.substr(0, 9) == "<![CDATA[") {
            const size_t start = position_ + 9;
            const size_t end = input_.find("]]>", start);
            const size_t stop = end == std::string_view::npos ? input_.size() : end;
            text_.assign(input_.substr(start, stop - start));
            position_ = end == std::string_view::npos ? input_.size() : end + 3;
            return type_ = Type::kText;
        }
        if (rest.substr(0, 2) == "<?") {
            position_ += 2;
            SkipPast("?>");
            continue;
        }
        if (rest.substr(0, 2) == "<!") {
            // DOCTYPE (possibly with an internal subset in [...]) or another declaration.
            size_t i = position_ + 2;
            int bracket_depth = 0;
            while (i < input_.size()) {
                const char c = input_[i];
                if (c == '[') {
                    ++bracket_depth;
                } else if (c == ']' && bracket_depth > 0) {
                    --bracket_depth;
                } else if (c == '>' && bracket_depth == 0) {
                    break;
                }
                ++i;
            }
            position_ = i < input_.size() ? i + 1 : input_.size();
            continue;
        }
        if (ParseTag()) {
            return type_;
        }
        // Not a tag after all (e.g. "a < b" in text): emit the '<' as text.
        text_ = "<";
        ++position_;
        return type_ = Type::kText;
    }
    return type_ = Type::kEnd;
}

bool XmlTokenizer::ParseTag()
{
    size_t i = position_ + 1;
    bool closing = false;
    if (i < input_.size() && input_[i] == '/') {
        closing = true;
        ++i;
    }
    if (i >= input_.size() || !IsNameStart(input_[i])) {
        return false;
    }
    const size_t name_start = i;
    while (i < input_.size() && IsNameChar(input_[i])) {
        ++i;
    }
    name_.assign(input_.substr(name_start, i - name_start));
    local_name_ = LocalName(name_);
    attributes_.clear();
    self_closing_ = false;

    if (closing) {
        const size_t end = input_.find('>', i);
        position_ = end == std::string_view::npos ? input_.size() : end + 1;
        type_ = Type::kEndTag;
        return true;
    }

    while (i < input_.size()) {
        while (i < input_.size() && IsSpace(input_[i])) {
            ++i;
        }
        if (i >= input_.size()) {
            break;
        }
        const char c = input_[i];
        if (c == '>') {
            ++i;
            break;
        }
        if (c == '/') {
            if (i + 1 < input_.size() && input_[i + 1] == '>') {
                self_closing_ = true;
                i += 2;
                break;
            }
            ++i;
            continue;
        }
        if (!IsNameChar(c)) {
            ++i;  // junk inside a tag: skip it rather than give up on the tag
            continue;
        }
        const size_t key_start = i;
        while (i < input_.size() && IsNameChar(input_[i])) {
            ++i;
        }
        std::string key(input_.substr(key_start, i - key_start));
        while (i < input_.size() && IsSpace(input_[i])) {
            ++i;
        }
        std::string value;
        if (i < input_.size() && input_[i] == '=') {
            ++i;
            while (i < input_.size() && IsSpace(input_[i])) {
                ++i;
            }
            if (i < input_.size() && (input_[i] == '"' || input_[i] == '\'')) {
                const char quote = input_[i++];
                const size_t end = input_.find(quote, i);
                const size_t stop = end == std::string_view::npos ? input_.size() : end;
                value = DecodeEntities(input_.substr(i, stop - i));
                i = end == std::string_view::npos ? input_.size() : end + 1;
            } else {
                const size_t value_start = i;
                while (i < input_.size() && !IsSpace(input_[i]) && input_[i] != '>') {
                    ++i;
                }
                value = DecodeEntities(input_.substr(value_start, i - value_start));
            }
        }
        if (attributes_.size() < 64) {
            attributes_.emplace_back(std::move(key), std::move(value));
        }
    }
    position_ = i;
    type_ = Type::kStartTag;
    return true;
}

}  // namespace epub

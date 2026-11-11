#include "provisioning/mini_json.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace mini_json {
namespace {

class Parser {
public:
    Parser(std::string_view text, const Limits& limits) : text_(text), limits_(limits) {}

    bool ParseDocument(Value* out, std::string* error)
    {
        if (text_.size() > limits_.max_bytes) {
            return Fail("the file is too big", error);
        }
        if (text_.substr(0, 3) == "\xEF\xBB\xBF") {
            pos_ = 3;
        }
        SkipSpace();
        if (!ParseValue(out, 0)) {
            return Fail(message_, error);
        }
        SkipSpace();
        if (pos_ != text_.size()) {
            return Fail("unexpected text after the end", error);
        }
        return true;
    }

private:
    bool Fail(const std::string& what, std::string* error)
    {
        int line = 1;
        for (size_t i = 0; i < pos_ && i < text_.size(); ++i) {
            line += text_[i] == '\n' ? 1 : 0;
        }
        if (error != nullptr) {
            *error = "line " + std::to_string(line) + ": " + what;
        }
        return false;
    }

    bool Error(const char* what)
    {
        message_ = what;
        return false;
    }

    void SkipSpace()
    {
        while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t' ||
                                       text_[pos_] == '\n' || text_[pos_] == '\r')) {
            ++pos_;
        }
    }

    bool Consume(std::string_view word)
    {
        if (text_.substr(pos_, word.size()) == word) {
            pos_ += word.size();
            return true;
        }
        return false;
    }

    bool ParseValue(Value* out, int depth)
    {
        if (depth > limits_.max_depth) {
            return Error("nested too deeply");
        }
        if (pos_ >= text_.size()) {
            return Error("the file ends too early");
        }
        const char c = text_[pos_];
        if (c == '{') {
            return ParseObject(out, depth);
        }
        if (c == '[') {
            return ParseArray(out, depth);
        }
        if (c == '"') {
            out->type = Value::Type::kString;
            return ParseString(&out->string);
        }
        if (Consume("true")) {
            out->type = Value::Type::kBool;
            out->boolean = true;
            return true;
        }
        if (Consume("false")) {
            out->type = Value::Type::kBool;
            out->boolean = false;
            return true;
        }
        if (Consume("null")) {
            out->type = Value::Type::kNull;
            return true;
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            return ParseNumber(out);
        }
        return Error("expected a value (a \"string\", number, true, false, null, [ or {)");
    }

    bool ParseObject(Value* out, int depth)
    {
        out->type = Value::Type::kObject;
        ++pos_;  // '{'
        SkipSpace();
        if (pos_ < text_.size() && text_[pos_] == '}') {
            ++pos_;
            return true;
        }
        while (true) {
            SkipSpace();
            if (pos_ >= text_.size() || text_[pos_] != '"') {
                return Error("expected a \"name\" in quotes");
            }
            std::string key;
            if (!ParseString(&key)) {
                return false;
            }
            SkipSpace();
            if (pos_ >= text_.size() || text_[pos_] != ':') {
                return Error("expected ':' after a name");
            }
            ++pos_;
            SkipSpace();
            Value value;
            if (!ParseValue(&value, depth + 1)) {
                return false;
            }
            out->object.push_back(Member{std::move(key), std::move(value)});
            SkipSpace();
            if (pos_ < text_.size() && text_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < text_.size() && text_[pos_] == '}') {
                ++pos_;
                return true;
            }
            return Error("expected ',' or '}'");
        }
    }

    bool ParseArray(Value* out, int depth)
    {
        out->type = Value::Type::kArray;
        ++pos_;  // '['
        SkipSpace();
        if (pos_ < text_.size() && text_[pos_] == ']') {
            ++pos_;
            return true;
        }
        while (true) {
            SkipSpace();
            Value value;
            if (!ParseValue(&value, depth + 1)) {
                return false;
            }
            out->array.push_back(std::move(value));
            SkipSpace();
            if (pos_ < text_.size() && text_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < text_.size() && text_[pos_] == ']') {
                ++pos_;
                return true;
            }
            return Error("expected ',' or ']'");
        }
    }

    static void AppendUtf8(uint32_t cp, std::string* out)
    {
        if (cp < 0x80) {
            out->push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out->push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out->push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out->push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out->push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool ParseHex4(uint32_t* out)
    {
        if (pos_ + 4 > text_.size()) {
            return Error("a \\u escape needs four hex digits");
        }
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            const char h = text_[pos_ + static_cast<size_t>(i)];
            value <<= 4;
            if (h >= '0' && h <= '9') {
                value |= static_cast<uint32_t>(h - '0');
            } else if (h >= 'a' && h <= 'f') {
                value |= static_cast<uint32_t>(h - 'a' + 10);
            } else if (h >= 'A' && h <= 'F') {
                value |= static_cast<uint32_t>(h - 'A' + 10);
            } else {
                return Error("a \\u escape needs four hex digits");
            }
        }
        pos_ += 4;
        *out = value;
        return true;
    }

    bool ParseString(std::string* out)
    {
        ++pos_;  // opening quote
        out->clear();
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"') {
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                return Error("a line break or control character inside \"quotes\" (missing a \"?)");
            }
            if (c != '\\') {
                out->push_back(c);
                continue;
            }
            if (pos_ >= text_.size()) {
                break;
            }
            const char e = text_[pos_++];
            switch (e) {
                case '"': out->push_back('"'); break;
                case '\\': out->push_back('\\'); break;
                case '/': out->push_back('/'); break;
                case 'b': out->push_back('\b'); break;
                case 'f': out->push_back('\f'); break;
                case 'n': out->push_back('\n'); break;
                case 'r': out->push_back('\r'); break;
                case 't': out->push_back('\t'); break;
                case 'u': {
                    uint32_t cp = 0;
                    if (!ParseHex4(&cp)) {
                        return false;
                    }
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        uint32_t low = 0;
                        if (!Consume("\\u") || !ParseHex4(&low) || low < 0xDC00 || low > 0xDFFF) {
                            return Error("a broken \\u surrogate pair");
                        }
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return Error("a broken \\u surrogate pair");
                    }
                    AppendUtf8(cp, out);
                    break;
                }
                default:
                    return Error("an unknown \\ escape inside \"quotes\"");
            }
        }
        return Error("a \"string\" that never ends (missing a closing \")");
    }

    bool ParseNumber(Value* out)
    {
        const size_t start = pos_;
        if (text_[pos_] == '-') {
            ++pos_;
        }
        const auto digits = [this]() {
            const size_t from = pos_;
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
                ++pos_;
            }
            return pos_ > from;
        };
        if (pos_ < text_.size() && text_[pos_] == '0') {
            ++pos_;
        } else if (!digits()) {
            return Error("a number with no digits");
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            if (!digits()) {
                return Error("a number with nothing after the '.'");
            }
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
                ++pos_;
            }
            if (!digits()) {
                return Error("a number with a broken exponent");
            }
        }
        const std::string number(text_.substr(start, pos_ - start));
        out->type = Value::Type::kNumber;
        out->number = std::strtod(number.c_str(), nullptr);
        if (!std::isfinite(out->number)) {
            return Error("a number that's too big");
        }
        return true;
    }

    std::string_view text_;
    Limits limits_;
    size_t pos_ = 0;
    std::string message_;
};

}  // namespace

const Value* Value::Find(std::string_view key) const
{
    const Value* found = nullptr;
    for (const auto& member : object) {
        if (member.key == key) {
            found = &member.value;
        }
    }
    return found;
}

bool Parse(std::string_view text, Value* out, std::string* error, const Limits& limits)
{
    *out = Value{};
    Parser parser(text, limits);
    if (!parser.ParseDocument(out, error)) {
        *out = Value{};
        return false;
    }
    return true;
}

}  // namespace mini_json

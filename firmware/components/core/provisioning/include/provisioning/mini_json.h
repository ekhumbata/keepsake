#ifndef PROVISIONING_MINI_JSON_H_
#define PROVISIONING_MINI_JSON_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// A small, strict JSON reader (RFC 8259) for files a person writes by hand on a computer: the
// provisioning file. Builds a tree; refuses anything malformed, too deep or too big, with a
// message that says where. Not for large documents.
namespace mini_json {

struct Member;

struct Value {
    enum class Type { kNull, kBool, kNumber, kString, kArray, kObject };

    Type type = Type::kNull;
    bool boolean = false;
    double number = 0.0;
    std::string string;                                  // kString: UTF-8
    std::vector<Value> array;                            // kArray
    std::vector<Member> object;                          // kObject, in file order

    // The member called `key` of an object (the last one, if repeated), or nullptr.
    const Value* Find(std::string_view key) const;
};

struct Member {
    std::string key;
    Value value;
};

struct Limits {
    size_t max_bytes = 64u * 1024u;
    int max_depth = 16;
};

// Parses all of `text` (surrounding whitespace allowed; a UTF-8 BOM is skipped). On failure,
// returns false and sets *error to e.g. "line 3: expected ',' or '}'".
bool Parse(std::string_view text, Value* out, std::string* error, const Limits& limits = {});

}  // namespace mini_json

#endif  // PROVISIONING_MINI_JSON_H_

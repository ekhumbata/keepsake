#ifndef EPUB_ENGINE_XML_TOKENIZER_H_
#define EPUB_ENGINE_XML_TOKENIZER_H_

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace epub {

// A forgiving pull tokenizer for the XML and XHTML found inside EPUBs. It is not a
// validating parser: real-world books contain HTML named entities without a DTD, stray '<'
// and unclosed tags, and a reader should still show the text. Comments, processing
// instructions and DOCTYPEs are skipped; CDATA becomes text; character and common named
// entities are decoded to UTF-8 (unknown ones are kept as written).
class XmlTokenizer {
public:
    enum class Type {
        kStartTag,  // name(), attributes(), self_closing()
        kEndTag,    // name()
        kText,      // text(), entities decoded
        kEnd,
    };

    explicit XmlTokenizer(std::string_view input) : input_(input) {}

    Type Next();

    Type type() const { return type_; }
    // Tag name as written, including any namespace prefix ("dc:title").
    const std::string& name() const { return name_; }
    // Lower-case name without its namespace prefix ("title"), for matching.
    const std::string& local_name() const { return local_name_; }
    bool self_closing() const { return self_closing_; }
    const std::vector<std::pair<std::string, std::string>>& attributes() const
    {
        return attributes_;
    }
    // Attribute value by local name (prefix-insensitive, ASCII case-insensitive), or nullptr.
    const std::string* Attribute(std::string_view local_name) const;
    const std::string& text() const { return text_; }

private:
    bool ParseTag();
    void SkipPast(std::string_view terminator);

    std::string_view input_;
    size_t position_ = 0;
    Type type_ = Type::kEnd;
    std::string name_;
    std::string local_name_;
    bool self_closing_ = false;
    std::vector<std::pair<std::string, std::string>> attributes_;
    std::string text_;
};

// Decodes character references (&#169; &#xA9;) and common named entities in `raw`.
std::string DecodeEntities(std::string_view raw);

// Lower-case local part of a qualified name: "dc:Title" -> "title".
std::string LocalName(std::string_view qualified_name);

}  // namespace epub

#endif  // EPUB_ENGINE_XML_TOKENIZER_H_

#include "epub_engine/xhtml_parser.h"

#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>

#include "epub_engine/href.h"
#include "epub_engine/xml_tokenizer.h"

namespace epub {

size_t Chapter::AnchorBlock(const std::string& id) const
{
    for (const auto& [anchor, index] : anchors) {
        if (anchor == id) {
            return index;
        }
    }
    return 0;
}

std::string BlockText(const Block& block)
{
    std::string text;
    for (const TextRun& run : block.runs) {
        text += run.text;
    }
    return text;
}

namespace {

constexpr size_t kMaxStackDepth = 256;

bool IsOneOf(const std::string& name, std::initializer_list<const char*> names)
{
    for (const char* candidate : names) {
        if (name == candidate) {
            return true;
        }
    }
    return false;
}

bool IsVoid(const std::string& name)
{
    return IsOneOf(name, {"area", "base", "br", "col", "embed", "hr", "img", "input", "link",
                          "meta", "param", "source", "track", "wbr", "image"});
}

bool IsBlockElement(const std::string& name)
{
    return IsOneOf(name, {"p",       "div",    "h1",      "h2",     "h3",       "h4",
                          "h5",      "h6",     "li",      "ul",     "ol",       "dl",
                          "dt",      "dd",     "blockquote", "pre", "section",  "article",
                          "aside",   "header", "footer",  "nav",    "figure",   "figcaption",
                          "table",   "tr",     "address", "center", "main",     "body",
                          "hgroup",  "caption", "details", "summary", "fieldset", "legend",
                          "tbody",   "thead",  "tfoot"});
}

bool IsSkipped(const std::string& name)
{
    return IsOneOf(name, {"head", "script", "style", "title", "template"});
}

bool IsSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

uint8_t InlineStyleFor(const XmlTokenizer& token)
{
    const std::string& name = token.local_name();
    uint8_t style = kStyleNone;
    if (IsOneOf(name, {"b", "strong"})) {
        style |= kStyleBold;
    }
    if (IsOneOf(name, {"i", "em", "cite", "dfn", "var"})) {
        style |= kStyleItalic;
    }
    // Inline CSS only; class-based styling needs the CSS work in #28's follow-ups.
    if (const std::string* css = token.Attribute("style")) {
        if (css->find("italic") != std::string::npos) {
            style |= kStyleItalic;
        }
        if (css->find("bold") != std::string::npos || css->find("font-weight:7") != std::string::npos ||
            css->find("font-weight: 7") != std::string::npos ||
            css->find("font-weight:6") != std::string::npos ||
            css->find("font-weight: 6") != std::string::npos) {
            style |= kStyleBold;
        }
    }
    return style;
}

class Builder {
public:
    Builder(std::string_view document_path, const XhtmlLimits& limits)
        : document_directory_(DirectoryOf(document_path)), limits_(limits)
    {
    }

    Chapter Build(std::string_view xhtml)
    {
        XmlTokenizer tokens(xhtml);
        while (!done_) {
            switch (tokens.Next()) {
                case XmlTokenizer::Type::kStartTag:
                    OnStart(tokens);
                    break;
                case XmlTokenizer::Type::kEndTag:
                    OnEnd(tokens.local_name());
                    break;
                case XmlTokenizer::Type::kText:
                    OnText(tokens.text());
                    break;
                case XmlTokenizer::Type::kEnd:
                    done_ = true;
                    break;
            }
        }
        while (!stack_.empty()) {
            Pop();
        }
        Flush();
        return std::move(chapter_);
    }

private:
    struct Frame {
        std::string name;
        bool block = false;
        bool skip = false;
        bool pre = false;
        uint8_t style = kStyleNone;
    };

    void OnStart(const XmlTokenizer& token)
    {
        const std::string& name = token.local_name();
        if (skip_depth_ == 0) {
            RecordAnchor(token);
        }

        if (name == "br") {
            if (skip_depth_ == 0) {
                LineBreak();
            }
            return;
        }
        if (name == "hr") {
            if (skip_depth_ == 0) {
                Flush();
                Block rule;
                rule.type = BlockType::kRule;
                Emit(std::move(rule));
            }
            return;
        }
        if (name == "img" || name == "image") {
            // Void (a stray </image> later finds no frame and is ignored).
            if (skip_depth_ == 0) {
                AddImage(token);
            }
            return;
        }
        if (token.self_closing() || IsVoid(name)) {
            return;
        }

        Frame frame;
        frame.name = name;
        frame.skip = IsSkipped(name);
        frame.block = IsBlockElement(name);
        frame.pre = name == "pre";
        frame.style = InlineStyleFor(token);
        if (stack_.size() >= kMaxStackDepth) {
            return;  // too deep: treat as a transparent inline element
        }
        if (frame.block) {
            Flush();
        }
        if (name == "td" || name == "th") {
            pending_space_style_ = CurrentStyle();
            pending_space_ = true;  // keep table cells apart
        }
        if (frame.skip) {
            ++skip_depth_;
        }
        if (frame.pre) {
            ++pre_depth_;
            skip_pre_newline_ = true;
        }
        if (frame.style & kStyleBold) {
            ++bold_depth_;
        }
        if (frame.style & kStyleItalic) {
            ++italic_depth_;
        }
        stack_.push_back(std::move(frame));
    }

    void OnEnd(const std::string& name)
    {
        for (size_t i = stack_.size(); i-- > 0;) {
            if (stack_[i].name == name) {
                while (stack_.size() > i) {
                    Pop();
                }
                return;
            }
        }
        // An end tag with no matching start: ignore it.
    }

    void Pop()
    {
        const Frame frame = std::move(stack_.back());
        stack_.pop_back();
        if (frame.block) {
            Flush();
        }
        if (frame.skip) {
            --skip_depth_;
        }
        if (frame.pre) {
            --pre_depth_;
        }
        if (frame.style & kStyleBold) {
            --bold_depth_;
        }
        if (frame.style & kStyleItalic) {
            --italic_depth_;
        }
    }

    void OnText(const std::string& text)
    {
        if (skip_depth_ > 0 || done_) {
            return;
        }
        text_bytes_ += text.size();
        if (text_bytes_ > limits_.max_text_bytes) {
            chapter_.truncated = true;
            done_ = true;
            return;
        }
        if (pre_depth_ > 0) {
            AppendPreformatted(text);
            return;
        }
        for (const char c : text) {
            if (IsSpace(c)) {
                if (!pending_space_) {
                    pending_space_style_ = CurrentStyle();
                }
                pending_space_ = true;
                continue;
            }
            if (pending_space_ && has_content_) {
                AppendSpace();
            }
            pending_space_ = false;
            AppendChar(c);
        }
    }

    void AppendPreformatted(const std::string& text)
    {
        for (const char c : text) {
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                if (skip_pre_newline_ && !has_content_) {
                    skip_pre_newline_ = false;  // a newline right after <pre> is not content
                    continue;
                }
                LineBreak(/*allow_empty=*/true);
                continue;
            }
            skip_pre_newline_ = false;
            AppendChar(c == '\t' ? ' ' : c);
        }
    }

    void AppendChar(char c)
    {
        if (!has_content_) {
            StartBlock();
        }
        const uint8_t style = CurrentStyle();
        std::vector<TextRun>& runs = current_.runs;
        if (runs.empty() || runs.back().style != style || runs.back().text == "\n") {
            runs.push_back(TextRun{std::string(1, c), style});
        } else {
            runs.back().text.push_back(c);
        }
    }

    // A collapsed space keeps the style in effect where the whitespace was written, so
    // "and <b>Mira</b> decided" gives "and " + bold "Mira" + " decided", never a bold space.
    void AppendSpace()
    {
        std::vector<TextRun>& runs = current_.runs;
        if (!runs.empty() && runs.back().style == pending_space_style_ && runs.back().text != "\n") {
            runs.back().text.push_back(' ');
        } else {
            runs.push_back(TextRun{" ", pending_space_style_});
        }
    }

    void LineBreak(bool allow_empty = false)
    {
        if (!has_content_) {
            if (!allow_empty) {
                return;
            }
            StartBlock();
        }
        current_.runs.push_back(TextRun{"\n", kStyleNone});
        pending_space_ = false;
    }

    void StartBlock()
    {
        current_ = Block{};
        has_content_ = true;
        int list_depth = 0;
        int quote_depth = 0;
        bool typed = false;
        for (size_t i = stack_.size(); i-- > 0;) {
            const std::string& name = stack_[i].name;
            if (name == "ul" || name == "ol") {
                ++list_depth;
            } else if (name == "blockquote") {
                ++quote_depth;
            }
            if (typed) {
                continue;
            }
            if (name.size() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6') {
                current_.type = BlockType::kHeading;
                current_.level = static_cast<uint8_t>(name[1] - '0');
                typed = true;
            } else if (name == "pre") {
                current_.type = BlockType::kPreformatted;
                typed = true;
            } else if (name == "li") {
                current_.type = BlockType::kListItem;
                typed = true;
            }
        }
        if (current_.type == BlockType::kListItem) {
            current_.level = static_cast<uint8_t>(list_depth > 0 ? std::min(list_depth, 9) : 1);
        } else if (!typed && quote_depth > 0) {
            current_.type = BlockType::kBlockquote;
            current_.level = static_cast<uint8_t>(std::min(quote_depth, 9));
        }
    }

    uint8_t CurrentStyle() const
    {
        uint8_t style = kStyleNone;
        if (bold_depth_ > 0) style |= kStyleBold;
        if (italic_depth_ > 0) style |= kStyleItalic;
        return style;
    }

    void Flush()
    {
        if (has_content_) {
            // Trailing line breaks carry no content.
            while (!current_.runs.empty() && current_.runs.back().text == "\n") {
                current_.runs.pop_back();
            }
            if (!current_.runs.empty()) {
                Emit(std::move(current_));
            }
        }
        current_ = Block{};
        has_content_ = false;
        pending_space_ = false;
    }

    void Emit(Block block)
    {
        if (chapter_.blocks.size() >= limits_.max_blocks) {
            chapter_.truncated = true;
            done_ = true;
            return;
        }
        chapter_.blocks.push_back(std::move(block));
    }

    void AddImage(const XmlTokenizer& token)
    {
        const std::string* source = token.Attribute("src");
        if (source == nullptr) {
            source = token.Attribute("href");  // SVG <image xlink:href="...">
        }
        if (source == nullptr || source->empty()) {
            return;
        }
        Flush();
        Block image;
        image.type = BlockType::kImage;
        image.image_href = ResolveHref(document_directory_, *source);
        if (const std::string* alt = token.Attribute("alt"); alt != nullptr && !alt->empty()) {
            image.runs.push_back(TextRun{*alt, kStyleNone});
        }
        Emit(std::move(image));
    }

    void RecordAnchor(const XmlTokenizer& token)
    {
        const std::string* id = token.Attribute("id");
        if (id == nullptr && token.local_name() == "a") {
            id = token.Attribute("name");
        }
        if (id != nullptr && !id->empty() && chapter_.anchors.size() < limits_.max_blocks) {
            chapter_.anchors.emplace_back(*id, chapter_.blocks.size());
        }
    }

    std::string document_directory_;
    XhtmlLimits limits_;
    Chapter chapter_;
    std::vector<Frame> stack_;
    Block current_;
    bool has_content_ = false;
    bool pending_space_ = false;
    uint8_t pending_space_style_ = kStyleNone;
    bool skip_pre_newline_ = false;
    bool done_ = false;
    int skip_depth_ = 0;
    int pre_depth_ = 0;
    int bold_depth_ = 0;
    int italic_depth_ = 0;
    size_t text_bytes_ = 0;
};

}  // namespace

Chapter ParseXhtml(std::string_view xhtml, std::string_view document_path,
                   const XhtmlLimits& limits)
{
    Builder builder(document_path, limits);
    return builder.Build(xhtml);
}

}  // namespace epub

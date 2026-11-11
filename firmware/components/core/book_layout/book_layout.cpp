#include "book_layout/book_layout.h"

#include <algorithm>

namespace book_layout {
namespace {

// A word is a run of non-space characters; it can cross style runs ("<b>bold</b>ness").
struct Piece {
    std::string text;
    uint8_t style = epub::kStyleNone;
};

struct Word {
    std::vector<Piece> pieces;
    size_t offset = 0;              // byte offset of the word's first character in the block
    bool space_before = false;      // collapsed whitespace before this word
    uint8_t space_style = epub::kStyleNone;
    bool break_before = false;      // a forced line break (<br>, or a newline in <pre>)
};

bool IsSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\f';
}

size_t Utf8Length(unsigned char lead)
{
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;  // stray continuation byte: treat as one character
}

TextRole RoleFor(const epub::Block& block)
{
    switch (block.type) {
        case epub::BlockType::kHeading:
            return block.level <= 1 ? TextRole::kHeading1
                                    : (block.level == 2 ? TextRole::kHeading2 : TextRole::kHeading3);
        case epub::BlockType::kPreformatted:
            return TextRole::kPreformatted;
        default:
            return TextRole::kBody;
    }
}

// Splits a block into words. In <pre> blocks only newlines separate "words", so spacing
// inside a line survives.
std::vector<Word> Tokenize(const epub::Block& block)
{
    const bool preformatted = block.type == epub::BlockType::kPreformatted;
    std::vector<Word> words;
    Word current;
    bool in_word = false;
    bool pending_space = false;
    uint8_t pending_space_style = epub::kStyleNone;
    bool pending_break = false;
    size_t offset = 0;

    auto finish = [&]() {
        if (in_word) {
            words.push_back(std::move(current));
            current = Word{};
            in_word = false;
        }
    };

    for (const epub::TextRun& run : block.runs) {
        if (run.text == "\n") {
            finish();
            if (pending_break && preformatted) {
                // Two breaks in a row: keep the empty line.
                Word empty;
                empty.offset = offset;
                empty.break_before = true;
                words.push_back(std::move(empty));
            }
            pending_break = true;
            pending_space = false;
            offset += run.text.size();
            continue;
        }
        for (size_t i = 0; i < run.text.size(); ++i) {
            const char c = run.text[i];
            if (c == '\n') {
                finish();
                pending_break = true;
                pending_space = false;
                ++offset;
                continue;
            }
            if (!preformatted && IsSpace(c)) {
                finish();
                if (!pending_space) {
                    pending_space_style = run.style;
                }
                pending_space = true;
                ++offset;
                continue;
            }
            if (!in_word) {
                in_word = true;
                current.offset = offset;
                current.space_before = pending_space;
                current.space_style = pending_space_style;
                current.break_before = pending_break;
                pending_space = false;
                pending_break = false;
            }
            if (current.pieces.empty() || current.pieces.back().style != run.style) {
                current.pieces.push_back(Piece{std::string(), run.style});
            }
            current.pieces.back().text.push_back(c);
            ++offset;
        }
    }
    finish();
    return words;
}

class LineBuilder {
public:
    LineBuilder(const TextMeasurer& measurer, TextRole role)
        : measurer_(measurer), role_(role)
    {
    }

    int WordWidth(const Word& word) const
    {
        int width = 0;
        for (const Piece& piece : word.pieces) {
            width += measurer_.Width(piece.text, piece.style, role_);
        }
        return width;
    }

    int SpaceWidth(uint8_t style) const { return measurer_.Width(" ", style, role_); }

    void AppendText(Line* line, int* x, std::string_view text, uint8_t style) const
    {
        if (text.empty()) {
            return;
        }
        if (!line->spans.empty() && line->spans.back().style == style) {
            line->spans.back().text.append(text);
        } else {
            line->spans.push_back(Span{std::string(text), style, *x});
        }
        *x += measurer_.Width(text, style, role_);
    }

private:
    const TextMeasurer& measurer_;
    TextRole role_;
};

// Breaks an overlong word into chunks that each fit `available` (at least one character per
// chunk, so a 1-px-wide line still makes progress).
std::vector<Word> SplitWord(const Word& word, const TextMeasurer& measurer, TextRole role,
                            int available)
{
    std::vector<Word> chunks;
    Word chunk;
    chunk.offset = word.offset;
    chunk.space_before = word.space_before;
    chunk.space_style = word.space_style;
    chunk.break_before = word.break_before;
    int width = 0;
    size_t offset = word.offset;
    for (const Piece& piece : word.pieces) {
        size_t i = 0;
        while (i < piece.text.size()) {
            const size_t length =
                std::min(Utf8Length(static_cast<unsigned char>(piece.text[i])), piece.text.size() - i);
            const std::string_view character(piece.text.data() + i, length);
            const int character_width = measurer.Width(character, piece.style, role);
            const bool chunk_empty = chunk.pieces.empty();
            if (!chunk_empty && width + character_width > available) {
                chunks.push_back(std::move(chunk));
                chunk = Word{};
                chunk.offset = offset;
                width = 0;
            }
            if (chunk.pieces.empty() || chunk.pieces.back().style != piece.style) {
                chunk.pieces.push_back(Piece{std::string(), piece.style});
            }
            chunk.pieces.back().text.append(character);
            width += character_width;
            offset += length;
            i += length;
        }
    }
    if (!chunk.pieces.empty()) {
        chunks.push_back(std::move(chunk));
    }
    return chunks;
}

// Wraps one text block into lines (y not yet assigned).
std::vector<Line> WrapBlock(const epub::Block& block, size_t block_index,
                            const TextMeasurer& measurer, const LayoutOptions& options)
{
    const TextRole role = RoleFor(block);
    int indent = 0;
    if (block.type == epub::BlockType::kListItem) {
        indent = options.list_indent * std::max<int>(1, block.level);
    } else if (block.type == epub::BlockType::kBlockquote) {
        indent = options.quote_indent * std::max<int>(1, block.level);
    }
    indent = std::min(indent, options.width / 2);  // deep nesting still leaves room for text
    const int first_indent =
        block.type == epub::BlockType::kParagraph ? std::min(options.first_line_indent, options.width / 4) : 0;

    LineBuilder builder(measurer, role);
    std::vector<Line> lines;
    Line line;
    int x = 0;
    bool first_line = true;

    auto start_line = [&](size_t offset) {
        line = Line{};
        line.role = role;
        line.block = block_index;
        line.offset = offset;
        line.height = measurer.LineHeight(role);
        line.x_indent = indent + (first_line ? first_indent : 0);
        line.kind = (first_line && block.type == epub::BlockType::kListItem) ? LineKind::kBullet
                                                                              : LineKind::kText;
        x = line.x_indent;
    };
    auto finish_line = [&]() {
        lines.push_back(std::move(line));
        first_line = false;
    };

    bool line_open = false;
    for (const Word& source : Tokenize(block)) {
        if (source.break_before && line_open) {
            finish_line();
            line_open = false;
        }
        const bool empty_word = source.pieces.empty();
        if (empty_word) {
            // An empty line inside <pre>.
            start_line(source.offset);
            finish_line();
            continue;
        }

        const int word_width = builder.WordWidth(source);
        const int space = (line_open && source.space_before) ? builder.SpaceWidth(source.space_style) : 0;
        const int right_edge = options.width;
        if (line_open && x + space + word_width <= right_edge) {
            if (space > 0) {
                builder.AppendText(&line, &x, " ", source.space_style);
            }
            for (const Piece& piece : source.pieces) {
                builder.AppendText(&line, &x, piece.text, piece.style);
            }
            continue;
        }
        if (line_open) {
            finish_line();
            line_open = false;
        }

        start_line(source.offset);
        line_open = true;
        const int available = std::max(1, right_edge - x);
        if (word_width <= available) {
            for (const Piece& piece : source.pieces) {
                builder.AppendText(&line, &x, piece.text, piece.style);
            }
            continue;
        }
        // Too wide for an empty line: break it between characters.
        const std::vector<Word> chunks = SplitWord(source, measurer, role, available);
        for (size_t c = 0; c < chunks.size(); ++c) {
            if (c > 0) {
                finish_line();
                start_line(chunks[c].offset);
            }
            for (const Piece& piece : chunks[c].pieces) {
                builder.AppendText(&line, &x, piece.text, piece.style);
            }
        }
    }
    if (line_open) {
        finish_line();
    }
    return lines;
}

}  // namespace

std::vector<Page> PaginateChapter(const epub::Chapter& chapter, const TextMeasurer& measurer,
                                  const LayoutOptions& options)
{
    std::vector<Page> pages(1);
    const int page_height = std::max(1, options.height);
    int y = 0;

    auto place = [&](Line line, int gap_before) {
        line.height = std::max(1, std::min(line.height, page_height));
        const bool page_empty = pages.back().lines.empty();
        int top = page_empty ? 0 : y + gap_before;
        if (!page_empty && top + line.height > page_height) {
            pages.emplace_back();
            top = 0;
        }
        line.y = top;
        y = top + line.height;
        pages.back().lines.push_back(std::move(line));
    };

    for (size_t b = 0; b < chapter.blocks.size(); ++b) {
        const epub::Block& block = chapter.blocks[b];
        int gap = options.paragraph_gap;
        if (block.type == epub::BlockType::kHeading) {
            gap += options.heading_gap;
        }

        if (block.type == epub::BlockType::kRule || block.type == epub::BlockType::kImage) {
            Line line;
            line.kind = block.type == epub::BlockType::kRule ? LineKind::kRule : LineKind::kImage;
            line.block = b;
            line.height = block.type == epub::BlockType::kRule ? options.rule_height : options.image_height;
            const std::string alt = epub::BlockText(block);
            if (!alt.empty() && line.kind == LineKind::kImage) {
                line.spans.push_back(Span{alt, epub::kStyleItalic, 0});
            }
            place(std::move(line), gap);
            continue;
        }

        std::vector<Line> lines = WrapBlock(block, b, measurer, options);
        for (size_t i = 0; i < lines.size(); ++i) {
            place(std::move(lines[i]), i == 0 ? gap : 0);
        }
    }
    return pages;
}

Position PageStart(const Page& page)
{
    if (page.lines.empty()) {
        return {};
    }
    return Position{page.lines.front().block, page.lines.front().offset};
}

size_t PageForPosition(const std::vector<Page>& pages, Position position)
{
    size_t found = 0;
    for (size_t i = 0; i < pages.size(); ++i) {
        if (pages[i].lines.empty()) {
            continue;
        }
        if (position < PageStart(pages[i])) {
            break;
        }
        found = i;
    }
    return found;
}

}  // namespace book_layout

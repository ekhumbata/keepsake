#include "epaper_ui/book_reader_page.h"

#include "epaper_ui/book_page.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <vector>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr auto kTitleRole = design::TypographyRole::kHeadingH2;
constexpr auto kBodyRole = design::TypographyRole::kBookBody;  // Literata, at the reader's text size
constexpr auto kPageIndicatorRole = design::TypographyRole::kLabelSmallBlack;
constexpr int kSideInset = design::spacing::k16;
constexpr int kTopGap = design::spacing::k24;
constexpr int kTitleContentGap = design::spacing::k16;
constexpr int kContentIndicatorGap = design::spacing::k8;
constexpr int kIndicatorFooterGap = design::spacing::k16;
// Bookmark ribbon (#28): hangs from under the status bar at the right edge, with a notch.
constexpr int kRibbonWidth = 18;
constexpr int kRibbonHeight = 34;
constexpr int kRibbonNotch = 8;
constexpr int kRibbonTitleGap = design::spacing::k8;

int FooterTop(int portrait_height)
{
    return portrait_height - design::global_footer::kBottomPadding -
           design::global_footer::kButtonSize;
}

struct Layout {
    UiRect title = {};
    UiRect content = {};
    UiRect page_indicator = {};
};

Layout BuildLayout(int portrait_width, int portrait_height)
{
    const int page_x = kSideInset;
    const int page_width = std::max(0, portrait_width - (2 * kSideInset));
    const int title_y = StatusBarHeight() + kTopGap;

    Layout layout = {};
    layout.title = {page_x, title_y, page_width, LineHeight(kTitleRole)};

    const int content_top = layout.title.bottom() + kTitleContentGap;
    const int footer_top = FooterTop(portrait_height);
    const int indicator_height = LineHeight(kPageIndicatorRole);
    const int indicator_y = footer_top - kIndicatorFooterGap - indicator_height;
    const int content_height = std::max(0, indicator_y - kContentIndicatorGap - content_top);
    layout.content = {page_x, content_top, page_width, content_height};
    layout.page_indicator = {page_x, indicator_y, page_width, indicator_height};
    return layout;
}

}  // namespace

namespace {

bool IsWrapBreakChar(char c)
{
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

size_t FindNextBreak(const std::string& text, size_t from)
{
    for (size_t index = from; index < text.size(); ++index) {
        if (IsWrapBreakChar(text[index])) {
            return index;
        }
    }
    return std::string::npos;
}

// Collapses every run of '\r'/'\n' to spaces, except a run with 2+ '\n's (a blank line in the
// source -- a paragraph break), which keeps exactly one '\n' as a marker and blanks the rest.
// Byte-length preserving (in-place, no insert/erase) so byte offsets tracked against the
// original text stay valid. A lone '\n' remains just a normal word-break afterward (same as a
// space); a surviving '\n' unambiguously means "paragraph break" to the line-wrap loop below.
void NormalizeLineBreaks(std::string& text)
{
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] != '\r' && text[i] != '\n') {
            ++i;
            continue;
        }
        const size_t run_start = i;
        int newline_count = 0;
        while (i < text.size() && (text[i] == '\r' || text[i] == '\n')) {
            if (text[i] == '\n') {
                ++newline_count;
            }
            ++i;
        }
        for (size_t j = run_start; j < i; ++j) {
            text[j] = ' ';
        }
        if (newline_count >= 2) {
            text[run_start] = '\n';
        }
    }
}

// Raw file bytes -> what the fonts can draw (ASCII stand-ins for accented letters, curly
// quotes, dashes ...), remembering for each output byte which raw byte it came from, so
// pagination can report how much of the file it used. Stops before a character cut off at the
// end of `raw`; `*decoded_end` is where it stopped.
std::string DecodeForDisplay(const std::string& raw, book_formats::TextEncoding encoding,
                             std::vector<size_t>* source, size_t* decoded_end)
{
    std::string text;
    text.reserve(raw.size());
    source->clear();
    source->reserve(raw.size() + 1);
    size_t pos = 0;
    while (pos < raw.size()) {
        uint32_t codepoint = 0;
        size_t length = 0;
        if (!book_formats::DecodeCharacter(raw, pos, encoding, &codepoint, &length)) {
            break;
        }
        if (codepoint < 0x20 && codepoint != '\n' && codepoint != '\r' && codepoint != '\t') {
            codepoint = ' ';  // stray control characters (and UTF-16's NULs, if misdetected)
        }
        AppendDisplayCodepoint(codepoint, &text);
        source->resize(text.size(), pos);
        pos += length;
    }
    *decoded_end = pos;
    source->push_back(pos);  // text.size() maps to the end of what was decoded
    return text;
}

}  // namespace

BookReaderPageLayout PaginateBookText(int portrait_width, int portrait_height,
                                      const std::string& raw_text,
                                      book_formats::TextEncoding encoding)
{
    BookReaderPageLayout result = {};
    if (raw_text.empty()) {
        result.text_exhausted = true;
        return result;
    }

    const UiRect content = BuildLayout(portrait_width, portrait_height).content;
    const int line_height = std::max(1, LineHeight(kBodyRole));
    const int max_lines = content.height > 0 ? std::max(1, content.height / line_height) : 0;
    if (max_lines <= 0 || content.width <= 0) {
        result.text_exhausted = true;
        return result;
    }

    // This is a reflowing reader -- original line breaks are not preserved verbatim -- so
    // hard-wrapped source newlines (every .txt file exported from Project Gutenberg et al.
    // hard-wraps at ~70-80 columns) are normalized to spaces up front, while an actual blank
    // line (a paragraph break) survives as a single '\n' marker the loop below reacts to.
    // The text is first decoded to what the fonts draw, with a map back to raw byte offsets
    // (`source`), so consumed_bytes still lines up with offsets in the file; normalizing is
    // length-preserving on top of that. Without the newline normalization, epaper_font::MeasureText (which
    // deliberately stops at the first '\r'/'\n', a safety behavior other single-line UI
    // callers rely on) silently under-reports the width of any multi-word span that has a
    // source newline in it -- which is nearly every span in a real book -- so the "is this
    // line too wide yet" check never trips and the algorithm swallows the entire input buffer
    // into one unbreakable "line".
    std::vector<size_t> source;
    size_t decoded_end = 0;
    std::string text = DecodeForDisplay(raw_text, encoding, &source, &decoded_end);
    NormalizeLineBreaks(text);

    size_t start = 0;
    while (start < text.size() && static_cast<int>(result.lines.size()) < max_lines) {
        while (start < text.size() && IsWrapBreakChar(text[start])) {
            ++start;
        }
        if (start >= text.size()) {
            break;
        }

        size_t best_end = start;
        size_t pos = start;
        while (pos < text.size()) {
            size_t word_end = FindNextBreak(text, pos);
            if (word_end == std::string::npos) {
                word_end = text.size();
            }
            if (MeasureText(kBodyRole, text.substr(start, word_end - start)) > content.width) {
                if (best_end == start) {
                    best_end = word_end;  // a single word longer than the line: keep it whole
                }
                break;
            }
            best_end = word_end;
            if (word_end >= text.size()) {
                break;
            }
            if (text[word_end] == '\n') {
                break;  // paragraph break: don't pull the next paragraph onto this line
            }
            pos = word_end + 1;
        }

        // NormalizeLineBreaks leaves '\n' nowhere except as this marker, so its presence right
        // at the line's end boundary is unambiguous, however the loop above stopped.
        const bool paragraph_break_follows = best_end < text.size() && text[best_end] == '\n';
        result.lines.push_back(text.substr(start, best_end - start));
        start = best_end + (best_end < text.size() ? 1 : 0);
        if (paragraph_break_follows && static_cast<int>(result.lines.size()) < max_lines) {
            result.lines.push_back({});  // blank line rendered for the paragraph gap
        }
    }

    result.consumed_bytes = source[std::min(start, text.size())];
    result.text_exhausted = start >= text.size();
    return result;
}

UiRect BookReaderContentBounds(int portrait_width, int portrait_height)
{
    return BuildLayout(portrait_width, portrait_height).content;
}

void DrawBookReaderPage(uint8_t* framebuffer,
                        int raw_width,
                        int raw_height,
                        int portrait_width,
                        int portrait_height,
                        const BookReaderPageState& state,
                        const StatusBarState& status_bar_state,
                        const GlobalFooterState& footer_state)
{
    if (framebuffer == nullptr) {
        return;
    }

    FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     {0, 0, portrait_width, portrait_height}, design::color::kWhite);
    DrawStatusBar(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                  status_bar_state);

    const Layout layout = BuildLayout(portrait_width, portrait_height);

    int title_width = layout.title.width;
    if (state.bookmarked) {
        const int ribbon_x = layout.title.right() - kRibbonWidth;
        const int ribbon_top = StatusBarHeight();
        for (int row = 0; row < kRibbonHeight; ++row) {
            // Rows above the notch are solid; below, a V opens up from the middle.
            const int gap = std::max(0, row - (kRibbonHeight - kRibbonNotch)) * kRibbonWidth /
                            kRibbonNotch;
            const int side = (kRibbonWidth - gap) / 2;
            FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                             {ribbon_x, ribbon_top + row, side, 1}, design::color::kBlack);
            FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                             {ribbon_x + kRibbonWidth - side, ribbon_top + row, side, 1},
                             design::color::kBlack);
        }
        title_width = std::max(0, title_width - kRibbonWidth - kRibbonTitleGap);
    }
    const std::string title = FitLabelText(kTitleRole, state.title_text, title_width);
    if (!title.empty()) {
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           layout.title.x, layout.title.y, title, kTitleRole, design::color::kBlack);
    }

    const int line_height = std::max(1, LineHeight(kBodyRole));
    int line_y = layout.content.y;
    if (state.use_layout_page) {
        DrawBookLayoutPage(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           layout.content.x, layout.content.y, state.layout_page);
    }
    for (const std::string& line : state.use_layout_page ? std::vector<std::string>{}
                                                         : state.page_lines) {
        if (line_y + line_height > layout.content.bottom()) {
            break;
        }
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           layout.content.x, line_y, line, kBodyRole, design::color::kBlack);
        line_y += line_height;
    }

    const std::string page_label =
        !state.page_label.empty()
            ? FitLabelText(kPageIndicatorRole, state.page_label, layout.page_indicator.width)
            : "Page " + std::to_string(state.page_number);
    DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       layout.page_indicator.x, layout.page_indicator.y, page_label,
                       kPageIndicatorRole, design::color::kBlack);

    DrawGlobalFooter(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     footer_state);
}

}  // namespace epaper_ui

#include "epaper_ui/book_page.h"

#include <algorithm>

#include "epaper_ui/font_renderer.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kBulletSize = 6;
constexpr int kBulletGap = 10;
constexpr int kRuleDotSize = 4;
constexpr int kRuleDotGap = 16;
constexpr int kImageRadius = 8;

const char* AsciiFor(uint32_t codepoint);

// ASCII stand-ins for the non-ASCII characters books use most.
void AppendAscii(uint32_t codepoint, std::string* out)
{
    // Latin-1 letters with accents -> the base letter.
    if (codepoint >= 0x00C0 && codepoint <= 0x00FF && codepoint != 0x00C6 &&
        codepoint != 0x00E6 && codepoint != 0x00DF && codepoint != 0x00D7) {
        static constexpr char kBase[] =
            "AAAAAAACEEEEIIII"   // C0-CF
            "DNOOOOO*OUUUUYTs"   // D0-DF
            "aaaaaaaceeeeiiii"   // E0-EF
            "dnooooo/ouuuuyty";  // F0-FF
        out->push_back(kBase[codepoint - 0x00C0]);
        return;
    }
    out->append(AsciiFor(codepoint));
}

const char* AsciiFor(uint32_t codepoint)
{
    switch (codepoint) {
        case 0x00A0: case 0x2002: case 0x2003: case 0x2009: return " ";
        case 0x00AD: case 0x200B: case 0x200C: case 0x200D: case 0xFEFF: return "";
        case 0x2018: case 0x2019: case 0x201A: case 0x2032: return "'";
        case 0x201C: case 0x201D: case 0x201E: case 0x2033: case 0x00AB: case 0x00BB: return "\"";
        case 0x2010: case 0x2011: case 0x2012: case 0x2013: return "-";
        case 0x2014: case 0x2015: return "--";
        case 0x2026: return "...";
        case 0x2022: case 0x00B7: return "*";
        case 0x00D7: return "x";
        case 0x00A9: return "(c)";
        case 0x00AE: return "(R)";
        case 0x2122: return "(TM)";
        case 0x00BC: return "1/4";
        case 0x00BD: return "1/2";
        case 0x00BE: return "3/4";
        case 0x00DF: return "ss";
        case 0x00C6: return "AE";
        case 0x00E6: return "ae";
        case 0x0152: return "OE";
        case 0x0153: return "oe";
        default: break;
    }
    return "?";
}

}  // namespace

std::string ToDisplayText(std::string_view utf8)
{
    std::string out;
    out.reserve(utf8.size());
    size_t i = 0;
    while (i < utf8.size()) {
        const auto lead = static_cast<unsigned char>(utf8[i]);
        if (lead < 0x80) {
            out.push_back(static_cast<char>(lead));
            ++i;
            continue;
        }
        size_t length = 1;
        uint32_t codepoint = 0;
        if ((lead & 0xE0) == 0xC0) {
            length = 2;
            codepoint = lead & 0x1F;
        } else if ((lead & 0xF0) == 0xE0) {
            length = 3;
            codepoint = lead & 0x0F;
        } else if ((lead & 0xF8) == 0xF0) {
            length = 4;
            codepoint = lead & 0x07;
        } else {
            out.push_back('?');  // stray continuation byte
            ++i;
            continue;
        }
        bool valid = i + length <= utf8.size();
        for (size_t k = 1; valid && k < length; ++k) {
            const auto cont = static_cast<unsigned char>(utf8[i + k]);
            valid = (cont & 0xC0) == 0x80;
            codepoint = (codepoint << 6) | (cont & 0x3F);
        }
        if (!valid) {
            out.push_back('?');
            ++i;
            continue;
        }
        AppendAscii(codepoint, &out);
        i += length;
    }
    return out;
}

design::TypographyRole BookTypographyRole(book_layout::TextRole role, uint8_t style)
{
    switch (role) {
        case book_layout::TextRole::kHeading1:
            return design::TypographyRole::kHeadingH2;
        case book_layout::TextRole::kHeading2:
            return design::TypographyRole::kHeadingH3;
        case book_layout::TextRole::kHeading3:
            return design::TypographyRole::kLabelSmallBold;
        case book_layout::TextRole::kBody:
        case book_layout::TextRole::kPreformatted:
        default:
            return (style & epub::kStyleBold) ? design::TypographyRole::kLabelSmallBold
                                              : design::TypographyRole::kBody;
    }
}

int EpaperTextMeasurer::Width(std::string_view text, uint8_t style, book_layout::TextRole role) const
{
    return MeasureText(BookTypographyRole(role, style), ToDisplayText(text));
}

int EpaperTextMeasurer::LineHeight(book_layout::TextRole role) const
{
    return epaper_ui::LineHeight(BookTypographyRole(role, epub::kStyleNone));
}

void DrawBookLayoutPage(uint8_t* framebuffer, int raw_width, int raw_height, int portrait_width,
                        int portrait_height, int origin_x, int origin_y,
                        const book_layout::Page& page)
{
    if (framebuffer == nullptr) {
        return;
    }
    for (const book_layout::Line& line : page.lines) {
        const int top = origin_y + line.y;
        switch (line.kind) {
            case book_layout::LineKind::kRule: {
                // Three dots, centered: a quiet scene break.
                const int total = 3 * kRuleDotSize + 2 * kRuleDotGap;
                const int start_x = origin_x + (portrait_width - 2 * origin_x - total) / 2;
                const int dot_y = top + (line.height - kRuleDotSize) / 2;
                for (int i = 0; i < 3; ++i) {
                    FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width,
                                     portrait_height,
                                     {start_x + i * (kRuleDotSize + kRuleDotGap), dot_y,
                                      kRuleDotSize, kRuleDotSize},
                                     design::color::kBlack);
                }
                break;
            }
            case book_layout::LineKind::kImage: {
                const UiRect box{origin_x, top + 2, portrait_width - 2 * origin_x,
                                 std::max(4, line.height - 4)};
                DrawRoundedPortraitBorder(framebuffer, raw_width, raw_height, portrait_width,
                                          portrait_height, box, kImageRadius, 2,
                                          design::color::kBlack);
                const std::string label =
                    line.spans.empty() ? std::string("Picture") : ToDisplayText(line.spans[0].text);
                const auto role = design::TypographyRole::kLabelSmall;
                const std::string fitted = FitLabelText(role, label, box.width - 24);
                const int text_x = box.x + (box.width - MeasureText(role, fitted)) / 2;
                const int text_y = box.y + (box.height - epaper_ui::LineHeight(role)) / 2;
                DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width,
                                   portrait_height, text_x, text_y, fitted, role,
                                   design::color::kBlack);
                break;
            }
            case book_layout::LineKind::kBullet:
            case book_layout::LineKind::kText:
                if (line.kind == book_layout::LineKind::kBullet) {
                    const int bullet_x = origin_x + line.x_indent - kBulletGap - kBulletSize;
                    const int bullet_y = top + (line.height - kBulletSize) / 2;
                    FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width,
                                     portrait_height,
                                     {bullet_x, bullet_y, kBulletSize, kBulletSize},
                                     design::color::kBlack);
                }
                for (const book_layout::Span& span : line.spans) {
                    DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width,
                                       portrait_height, origin_x + span.x, top,
                                       ToDisplayText(span.text),
                                       BookTypographyRole(line.role, span.style),
                                       design::color::kBlack);
                }
                break;
        }
    }
}

}  // namespace epaper_ui

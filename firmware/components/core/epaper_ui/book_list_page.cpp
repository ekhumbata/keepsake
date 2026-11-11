#include "epaper_ui/book_list_page.h"

#include <algorithm>
#include <string>

#include "epaper_ui/progress_bar.h"

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr auto kTitleRole = design::TypographyRole::kHeadingH1;
constexpr int kSideInset = design::spacing::k16;
constexpr int kTopGap = design::spacing::k24;
constexpr int kHeadingBottomPadding = design::spacing::k24;
constexpr int kListFooterGap = design::spacing::k16;
// Continue-reading card: cover on the left, text to its right, then a gap before the list.
constexpr int kCardTextGap = design::spacing::k16;
constexpr int kCardListGap = design::spacing::k24;
constexpr int kCardLineGap = design::spacing::k4;
constexpr auto kCardLabelRole = design::TypographyRole::kLabelSmall;
constexpr auto kCardTitleRole = design::TypographyRole::kHeadingH3;
constexpr auto kCardAuthorRole = design::TypographyRole::kLabelSmall;

int FooterTop(int portrait_height)
{
    return portrait_height - design::global_footer::kBottomPadding -
           design::global_footer::kButtonSize;
}

SelectListStyle ListStyle(int width, int panel_height)
{
    SelectListStyle style = {};
    style.width = width;
    style.panel_height = panel_height;
    return style;
}

ProgressBarState CardProgress(const ContinueReadingState& state)
{
    ProgressBarState progress = {};
    const int percent = std::clamp(state.progress_percent, 0, 100);
    progress.status_text = std::to_string(percent) + "% read";
    progress.progress_percent = percent;
    return progress;
}

// Label, title, optional author, then the progress bar: the text column's height.
int CardTextHeight(const ContinueReadingState& state)
{
    int height = LineHeight(kCardLabelRole) + kCardLineGap + LineHeight(kCardTitleRole) +
                 kCardLineGap;
    if (!state.author_text.empty()) {
        height += LineHeight(kCardAuthorRole) + kCardLineGap;
    }
    ProgressBarStyle style = {};
    style.width = 100;
    return height + kCardLineGap + ProgressBarBounds(0, 0, CardProgress(state), style).height;
}

struct Layout {
    UiRect card = {};  // empty when there's no continue-reading card
    UiRect books_list = {};
};

Layout BuildLayout(int portrait_width, int portrait_height, const BookListPageState& state)
{
    const int page_x = kSideInset;
    const int page_width = std::max(0, portrait_width - (2 * kSideInset));
    const int title_y = StatusBarHeight() + kTopGap;
    const int title_bottom = title_y + LineHeight(kTitleRole);
    int list_y = title_bottom + kHeadingBottomPadding;
    Layout layout = {};
    if (state.continue_reading.visible) {
        layout.card = {page_x, list_y, page_width,
                       std::max(kContinueCoverHeight, CardTextHeight(state.continue_reading))};
        list_y = layout.card.bottom() + kCardListGap;
    }
    const int footer_top = FooterTop(portrait_height);
    const int panel_height = std::max(0, footer_top - kListFooterGap - list_y);

    layout.books_list = SelectListPanelBounds(page_x, list_y, ListStyle(page_width, panel_height));
    return layout;
}

void DrawCover(uint8_t* framebuffer, int raw_width, int raw_height, int portrait_width,
               int portrait_height, const UiRect& box, const cover_image::Thumbnail& cover)
{
    if (cover.width <= 0 || cover.height <= 0) {
        // No cover: a quiet gray "book" with a frame.
        FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height, box,
                         design::color::kGrayLight);
        DrawPortraitBorder(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           box, 2, design::color::kBlack);
        return;
    }
    // Centred in the box (covers that aren't 2:3 leave a margin), with a thin frame.
    const int x0 = box.x + (box.width - std::min(cover.width, box.width)) / 2;
    const int y0 = box.y + (box.height - std::min(cover.height, box.height)) / 2;
    for (int y = 0; y < std::min(cover.height, box.height); ++y) {
        for (int x = 0; x < std::min(cover.width, box.width); ++x) {
            DrawPortraitPixel(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                              x0 + x, y0 + y, cover.black(x, y));
        }
    }
    DrawPortraitBorder(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       {x0 - 1, y0 - 1, std::min(cover.width, box.width) + 2,
                        std::min(cover.height, box.height) + 2},
                       1, design::color::kBlack);
}

void DrawContinueReading(uint8_t* framebuffer, int raw_width, int raw_height, int portrait_width,
                         int portrait_height, const UiRect& card,
                         const ContinueReadingState& state)
{
    const UiRect cover_box = {card.x, card.y, kContinueCoverWidth, kContinueCoverHeight};
    DrawCover(framebuffer, raw_width, raw_height, portrait_width, portrait_height, cover_box,
              state.cover);

    const int text_x = cover_box.right() + kCardTextGap;
    const int text_width = std::max(0, card.right() - text_x);
    int y = card.y;
    const auto line = [&](design::TypographyRole role, const std::string& text) {
        const std::string fitted = FitLabelText(role, text, text_width);
        if (!fitted.empty()) {
            DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width,
                               portrait_height, text_x, y, fitted, role, design::color::kBlack);
        }
        y += LineHeight(role) + kCardLineGap;
    };
    line(kCardLabelRole, "Continue reading");
    line(kCardTitleRole, state.title_text);
    if (!state.author_text.empty()) {
        line(kCardAuthorRole, state.author_text);
    }

    ProgressBarStyle style = {};
    style.width = text_width;
    DrawProgressBar(framebuffer, raw_width, raw_height, portrait_width, portrait_height, text_x,
                    y + kCardLineGap, CardProgress(state), style);
}

}  // namespace

UiRect BookListListBounds(int portrait_width, int portrait_height, const BookListPageState& state)
{
    return BuildLayout(portrait_width, portrait_height, state).books_list;
}

void DrawBookListPage(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      const BookListPageState& state,
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

    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    const int title_x = kSideInset;
    const int title_y = StatusBarHeight() + kTopGap;
    DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       title_x, title_y, state.title_text, kTitleRole, design::color::kBlack);

    if (state.continue_reading.visible) {
        DrawContinueReading(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                            layout.card, state.continue_reading);
    }

    DrawSelectList(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                   layout.books_list.x, layout.books_list.y, state.books_list,
                   ListStyle(layout.books_list.width, layout.books_list.height));

    DrawGlobalFooter(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     footer_state);
}

}  // namespace epaper_ui

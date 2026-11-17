#include "epaper_ui/dashboard_page.h"

#include <algorithm>
#include <array>

#include "epaper_ui/menu_container.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kMargin = design::spacing::k16;
constexpr int kContentTopGap = design::spacing::k32;
constexpr int kWelcomeMiddleGap = design::spacing::k48;
constexpr int kMiddleMenuGap = design::spacing::k8;
constexpr auto kProgressLabelRole = design::TypographyRole::kLabelSmallBlack;

constexpr std::array<const char*, kDashboardMenuItemCount> kMenuLabels = {
    "Books", "Photos", "Follow up", "Todos", "Notes", "Topics",
};

int PageWidth(int portrait_width)
{
    return std::max(0, portrait_width - (2 * kMargin));
}

WelcomeMessageStyle WelcomeStyle(int width)
{
    WelcomeMessageStyle style = {};
    style.title_max_width = width;
    return style;
}

CompletionBannerStyle BannerStyle(int width)
{
    CompletionBannerStyle style = {};
    style.width = width;
    return style;
}

ProgressBarStyle ProgressStyle(int width)
{
    ProgressBarStyle style = {};
    style.label_role = kProgressLabelRole;
    style.width = width;
    return style;
}

MenuContainerState MenuState(const DashboardPageState& state)
{
    return {state.menu.selected_index, kDashboardMenuItemCount};
}

MenuContainerStyle MenuStyle(int width)
{
    MenuContainerStyle style = {};
    style.direction = MenuContainerDirection::kVertical;
    style.sizing = MenuContainerSizing::kFixedItemExtent;
    style.width = width;
    style.item_height = design::menu_item::kHeight;
    style.item_gap = 0;
    return style;
}

// Space kept between the last menu row and the footer's buttons.
constexpr int kMenuFooterGap = design::spacing::k8;
// The "more rows" arrows (#162): a small solid triangle at the right of the menu.
constexpr int kMoreArrowWidth = 14;
constexpr int kMoreArrowHeight = 7;

struct Layout {
    UiRect welcome = {};
    bool shows_banner = false;
    UiRect middle = {};
    int menu_y = 0;
    // The rows drawn: [first_row, first_row + visible_rows), the rest scrolled away.
    int first_row = 0;
    int visible_rows = kDashboardMenuItemCount;
};

Layout BuildLayout(int portrait_width, int portrait_height, const DashboardPageState& state)
{
    const int page_width = PageWidth(portrait_width);
    const int content_top = StatusBarHeight() + kContentTopGap;

    Layout layout = {};
    layout.welcome =
        WelcomeMessageBounds(kMargin, content_top, state.welcome_message, WelcomeStyle(page_width));
    const int middle_y = layout.welcome.bottom() + kWelcomeMiddleGap;
    layout.shows_banner = state.shows_completion_banner;
    if (layout.shows_banner) {
        layout.middle = CompletionBannerBounds(kMargin, middle_y, state.completion_banner,
                                               BannerStyle(page_width));
    } else {
        layout.middle =
            ProgressBarBounds(kMargin, middle_y, state.current_progress, ProgressStyle(page_width));
    }
    layout.menu_y = layout.middle.bottom() + kMiddleMenuGap;

    // As many rows as fit above the footer; the window follows the selection (#162).
    const int footer_top = portrait_height - design::global_footer::kBottomPadding -
                           design::global_footer::kButtonSize - kMenuFooterGap;
    const int fit = std::max(1, (footer_top - layout.menu_y) / design::menu_item::kHeight);
    layout.visible_rows = std::min(fit, kDashboardMenuItemCount);
    const int selected = state.menu.selected_index;
    if (selected >= layout.visible_rows) {
        layout.first_row = std::min(selected - layout.visible_rows + 1,
                                    kDashboardMenuItemCount - layout.visible_rows);
    }
    return layout;
}

bool RowVisible(const Layout& layout, int index)
{
    return index >= layout.first_row && index < layout.first_row + layout.visible_rows;
}

bool MenuItemShowsBadge(const DashboardPageMenuState& menu, int index)
{
    switch (static_cast<DashboardMenuItem>(index)) {
        case DashboardMenuItem::kFollowUp:
            return menu.shows_follow_up_badge;
        case DashboardMenuItem::kTodos:
            return menu.shows_todos_badge;
        case DashboardMenuItem::kNotes:
            return menu.shows_notes_badge;
        default:
            return false;
    }
}

BadgeState MenuItemBadge(const DashboardPageMenuState& menu, int index)
{
    switch (static_cast<DashboardMenuItem>(index)) {
        case DashboardMenuItem::kFollowUp:
            return {menu.follow_up_badge_text, false};
        case DashboardMenuItem::kTodos:
            return {menu.todos_badge_text, false};
        case DashboardMenuItem::kNotes:
            return {menu.notes_badge_text, false};
        default:
            return {};
    }
}

// A small solid triangle pointing up or down, its tip towards the hidden rows.
void DrawMoreArrow(uint8_t* framebuffer, int raw_width, int raw_height, int portrait_width,
                   int portrait_height, int center_x, int y, bool up)
{
    for (int row = 0; row < kMoreArrowHeight; ++row) {
        const int half = (kMoreArrowWidth / 2) * (row + 1) / kMoreArrowHeight;
        const int draw_y = up ? y + row : y + kMoreArrowHeight - 1 - row;
        FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                         {center_x - half, draw_y, 2 * half + 1, 1}, design::color::kBlack);
    }
}

}  // namespace

const char* DashboardMenuItemLabel(int index)
{
    if (index < 0 || index >= kDashboardMenuItemCount) {
        return "";
    }
    return kMenuLabels[static_cast<size_t>(index)];
}

UiRect DashboardMenuItemBounds(int portrait_width,
                               int portrait_height,
                               const DashboardPageState& state,
                               int index)
{
    if (index < 0 || index >= kDashboardMenuItemCount) {
        return {};
    }
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    if (!RowVisible(layout, index)) {
        return {};  // scrolled away
    }
    return MenuContainerItemBounds(kMargin, layout.menu_y, MenuState(state),
                                   MenuStyle(PageWidth(portrait_width)), index - layout.first_row);
}

bool HitTestDashboardMenuItem(int portrait_width,
                              int portrait_height,
                              const DashboardPageState& state,
                              int x,
                              int y,
                              int* index)
{
    if (index != nullptr) {
        *index = -1;
    }
    for (int candidate = 0; candidate < kDashboardMenuItemCount; ++candidate) {
        const UiRect bounds =
            DashboardMenuItemBounds(portrait_width, portrait_height, state, candidate);
        if (!bounds.IsEmpty() && bounds.Contains(x, y)) {
            if (index != nullptr) {
                *index = candidate;
            }
            return true;
        }
    }
    return false;
}

void DrawDashboardPage(uint8_t* framebuffer,
                       int raw_width,
                       int raw_height,
                       int portrait_width,
                       int portrait_height,
                       const DashboardPageState& state,
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

    const int page_width = PageWidth(portrait_width);
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);

    DrawWelcomeMessage(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       layout.welcome.x, layout.welcome.y, state.welcome_message,
                       WelcomeStyle(page_width));

    if (layout.shows_banner) {
        DrawCompletionBanner(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                             layout.middle.x, layout.middle.y, state.completion_banner,
                             BannerStyle(page_width));
    } else {
        DrawProgressBar(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                        layout.middle.x, layout.middle.y, state.current_progress,
                        ProgressStyle(page_width));
    }

    const int last_row = layout.first_row + layout.visible_rows - 1;
    for (int index = layout.first_row; index <= last_row; ++index) {
        const UiRect bounds =
            MenuContainerItemBounds(kMargin, layout.menu_y, MenuState(state),
                                    MenuStyle(page_width), index - layout.first_row);
        MenuItemState item = {};
        item.label_text = DashboardMenuItemLabel(index);
        item.selected = index == state.menu.selected_index;
        item.shows_badge = MenuItemShowsBadge(state.menu, index);
        item.badge = MenuItemBadge(state.menu, index);

        MenuItemStyle item_style = {};
        item_style.width = bounds.width;
        item_style.height = bounds.height;
        if (index == last_row) {
            item_style.bottom_border_thickness = 0;  // no separator under the last row
        }
        DrawMenuItem(framebuffer, raw_width, raw_height, portrait_width, portrait_height, bounds.x,
                     bounds.y, item, item_style);
    }

    // Arrows at the right edge say there are rows scrolled away above or below (#162).
    const int arrow_x = kMargin + page_width - design::spacing::k16;
    if (layout.first_row > 0) {
        DrawMoreArrow(framebuffer, raw_width, raw_height, portrait_width, portrait_height, arrow_x,
                      layout.menu_y - kMoreArrowHeight - 1, /*up=*/true);
    }
    if (last_row < kDashboardMenuItemCount - 1) {
        DrawMoreArrow(framebuffer, raw_width, raw_height, portrait_width, portrait_height, arrow_x,
                      layout.menu_y + layout.visible_rows * design::menu_item::kHeight + 1,
                      /*up=*/false);
    }

    DrawGlobalFooter(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     footer_state);
}

}  // namespace epaper_ui

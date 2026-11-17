#include "epaper_ui/settings_display_page.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr auto kTitleRole = design::TypographyRole::kHeadingH1;
constexpr int kSideInset = design::spacing::k16;
constexpr int kTopGap = design::spacing::k24;
constexpr int kHeadingBottomPadding = design::spacing::k24;
constexpr int kBackButtonGap = design::spacing::k24;
constexpr int kInputGap = design::spacing::k16;

struct Layout {
    UiRect lock_screen_input = {};
    UiRect button_sounds_input = {};
    UiRect back = {};
};

Layout BuildLayout(int portrait_width, int portrait_height, const SettingsDisplayPageState& state)
{
    (void)portrait_height;
    const int page_x = kSideInset;
    const int page_width = std::max(0, portrait_width - (2 * kSideInset));
    const int title_y = StatusBarHeight() + kTopGap;
    const int title_bottom = title_y + LineHeight(kTitleRole);

    TextInputStyle input_style = {};
    input_style.width = page_width;
    const UiRect lock_screen_input = SelectInputBounds(
        page_x, title_bottom + kHeadingBottomPadding, state.lock_screen_input,
        input_style);

    const UiRect button_sounds_input = SelectInputBounds(
        page_x, lock_screen_input.bottom() + kInputGap, state.button_sounds_input,
        input_style);

    ButtonStyle back_style = {};
    back_style.width = page_width;
    back_style.center_label = true;
    const UiRect back =
        ButtonBounds(page_x, button_sounds_input.bottom() + kBackButtonGap, state.back, back_style);

    return {
        .lock_screen_input = lock_screen_input,
        .button_sounds_input = button_sounds_input,
        .back = back,
    };
}

}  // namespace

UiRect SettingsDisplayPageItemBounds(int portrait_width,
                                   int portrait_height,
                                   const SettingsDisplayPageState& state,
                                   SettingsDisplayPageItemId item)
{
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    switch (item) {
        case SettingsDisplayPageItemId::kLockScreenInput:
            return layout.lock_screen_input;
        case SettingsDisplayPageItemId::kButtonSoundsInput:
            return layout.button_sounds_input;
        case SettingsDisplayPageItemId::kBack:
            return layout.back;
        case SettingsDisplayPageItemId::kNone:
        default:
            return {};
    }
}

bool HitTestSettingsDisplayPageItem(int portrait_width,
                                  int portrait_height,
                                  const SettingsDisplayPageState& state,
                                  int x,
                                  int y,
                                  SettingsDisplayPageItemId* item)
{
    if (item != nullptr) {
        *item = SettingsDisplayPageItemId::kNone;
    }

    constexpr SettingsDisplayPageItemId kItems[] = {
        SettingsDisplayPageItemId::kLockScreenInput,
        SettingsDisplayPageItemId::kButtonSoundsInput,
        SettingsDisplayPageItemId::kBack,
    };
    for (SettingsDisplayPageItemId candidate : kItems) {
        const UiRect bounds =
            SettingsDisplayPageItemBounds(portrait_width, portrait_height, state, candidate);
        if (!bounds.IsEmpty() && bounds.Contains(x, y)) {
            if (item != nullptr) {
                *item = candidate;
            }
            return true;
        }
    }
    return false;
}

void DrawSettingsDisplayPage(uint8_t* framebuffer,
                           int raw_width,
                           int raw_height,
                           int portrait_width,
                           int portrait_height,
                           const SettingsDisplayPageState& state,
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

    TextInputStyle input_style = {};
    input_style.width = layout.lock_screen_input.width;
    DrawSelectInput(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                    layout.lock_screen_input.x, layout.lock_screen_input.y,
                    state.lock_screen_input, input_style);
    DrawSelectInput(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                    layout.button_sounds_input.x, layout.button_sounds_input.y,
                    state.button_sounds_input, input_style);

    DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height, layout.back.x,
              layout.back.y, state.back,
              {.width = layout.back.width, .center_label = true});

    DrawGlobalFooter(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     footer_state);
}

}  // namespace epaper_ui

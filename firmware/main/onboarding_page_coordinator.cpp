#include "onboarding_page_coordinator.h"

#include "ai_features.h"

#include <algorithm>
#include <array>

#include "app_state_service.h"
#include "wifi_service.h"

namespace {

using page_navigation::NavigationItemRole;
using epaper_ui::OnboardingControl;

NavigationItemRole RoleForControl(OnboardingControl control)
{
    switch (control) {
        case OnboardingControl::kClose:
            return NavigationItemRole::kOnboardingPageClose;
        case OnboardingControl::kPrev:
            return NavigationItemRole::kOnboardingPagePrev;
        case OnboardingControl::kNext:
            return NavigationItemRole::kOnboardingPageNext;
        case OnboardingControl::kNone:
        default:
            return NavigationItemRole::kUnknown;
    }
}

}  // namespace

OnboardingPageCoordinator::OnboardingPageCoordinator() = default;

int OnboardingPageCoordinator::slide_count() const
{
    return static_cast<int>(slides_.size());
}

void OnboardingPageCoordinator::PrepareForShow()
{
    // What the slides say depends on the setup (#86): the friend's name, a Gemini key (#44),
    // and whether the provisioning file already brought a Wi-Fi network.
    const std::string friend_name = app_state_service::Personal().friend_name;
    epaper_ui::OnboardingContext context = {};
    context.friend_name = friend_name;
    context.ai_enabled = ai_features::Enabled();
    context.wifi_ready = !wifi_service::SavedNetworks().empty();
    slides_ = epaper_ui::OnboardingSlides(context);
    active_index_ = 0;
    focus_.Configure(navigation_model_.item_count, 0);
    FocusFirstSelectable();
}

epaper_ui::OnboardingControl OnboardingPageCoordinator::FocusedControl() const
{
    if (IsRoleFocused(NavigationItemRole::kOnboardingPageClose)) {
        return OnboardingControl::kClose;
    }
    if (IsRoleFocused(NavigationItemRole::kOnboardingPagePrev)) {
        return OnboardingControl::kPrev;
    }
    if (IsRoleFocused(NavigationItemRole::kOnboardingPageNext)) {
        return OnboardingControl::kNext;
    }
    return OnboardingControl::kNone;
}

bool OnboardingPageCoordinator::ControlSelectable(OnboardingControl control) const
{
    switch (control) {
        case OnboardingControl::kPrev:
            return !PrevDisabled();
        case OnboardingControl::kNext:
            return !NextDisabled();
        case OnboardingControl::kClose:
            return ShowClose();  // only on the last slide
        case OnboardingControl::kNone:
        default:
            return false;
    }
}

void OnboardingPageCoordinator::FocusRole(NavigationItemRole role)
{
    const int index = navigation_model_.IndexOfRole(role);
    if (index >= 0) {
        focus_.SetIndex(index);
    }
}

void OnboardingPageCoordinator::FocusFirstSelectable()
{
    // Nav order is [Close, Prev, Next]; pick the first control that can currently be focused.
    for (const OnboardingControl control :
         {OnboardingControl::kClose, OnboardingControl::kPrev, OnboardingControl::kNext}) {
        if (ControlSelectable(control)) {
            FocusRole(RoleForControl(control));
            return;
        }
    }
}

bool OnboardingPageCoordinator::IsRoleFocused(NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

bool OnboardingPageCoordinator::MoveFocus(int delta)
{
    if (delta == 0) {
        return false;
    }
    // UP/DOWN pages the carousel rather than roving the control row.
    //
    // Roving does not work here: the row is Close/Prev/Next, and on the first slide -- where
    // onboarding always opens -- Prev is disabled and Close is hidden, leaving Next as the
    // only selectable control. The old loop then wrapped a full circle back onto Next and
    // still reported success, so UP/DOWN played a cue and repainted without ever moving.
    // Paging is also what the keys mean everywhere else in the app, and it keeps every slide
    // reachable without touch. EnsureFocusEnabled (inside Next/PrevSlide) keeps the focused
    // control valid, so a click still activates Next, or Close on the last slide.
    return delta > 0 ? NextSlide() : PrevSlide();
}

bool OnboardingPageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

void OnboardingPageCoordinator::EnsureFocusEnabled()
{
    const OnboardingControl focused = FocusedControl();
    if (ControlSelectable(focused)) {
        return;
    }
    // Reaching the last slide: Next disables and Close appears -> land on Close (the primary CTA).
    if (focused == OnboardingControl::kNext && ShowClose()) {
        FocusRole(NavigationItemRole::kOnboardingPageClose);
        return;
    }
    // Reaching the first slide: Prev disables -> land on Next.
    if (focused == OnboardingControl::kPrev && !NextDisabled()) {
        FocusRole(NavigationItemRole::kOnboardingPageNext);
        return;
    }
    FocusFirstSelectable();
}

bool OnboardingPageCoordinator::NextSlide()
{
    if (NextDisabled()) {
        return false;
    }
    ++active_index_;
    EnsureFocusEnabled();
    return true;
}

bool OnboardingPageCoordinator::PrevSlide()
{
    if (PrevDisabled()) {
        return false;
    }
    --active_index_;
    EnsureFocusEnabled();
    return true;
}

bool OnboardingPageCoordinator::FocusControl(OnboardingControl control)
{
    const NavigationItemRole role = RoleForControl(control);
    if (role == NavigationItemRole::kUnknown || !ControlSelectable(control)) {
        return false;
    }
    const int index = navigation_model_.IndexOfRole(role);
    if (index < 0) {
        return false;
    }
    return focus_.SetIndex(index);
}

epaper_ui::OnboardingPageState OnboardingPageCoordinator::BuildState() const
{
    epaper_ui::OnboardingPageState state = {};
    state.navigation_focus_index = focus_.index();
    state.carousel.slide_count = slide_count();
    state.carousel.active_index = active_index_;
    state.carousel.show_close = ShowClose();
    state.carousel.close_selected = IsRoleFocused(NavigationItemRole::kOnboardingPageClose);
    state.carousel.prev_selected = IsRoleFocused(NavigationItemRole::kOnboardingPagePrev);
    state.carousel.next_selected = IsRoleFocused(NavigationItemRole::kOnboardingPageNext);

    if (slides_.empty()) {
        return state;
    }
    const size_t slide_index =
        std::min(static_cast<size_t>(std::max(active_index_, 0)), slides_.size() - 1);
    const epaper_ui::OnboardingSlide& slide = slides_[slide_index];
    state.slide_title = slide.title;
    state.slide_body = slide.body;
    state.slide_image = slide.image;
    return state;
}

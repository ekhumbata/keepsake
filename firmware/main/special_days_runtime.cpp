#include "special_days_runtime.h"

#include <cstdio>
#include <ctime>
#include <string>

#include "epaper_ui/book_page.h"
#include "epaper_ui/mascots.h"
#include "esp_log.h"
#include "overlay_runtime.h"

namespace special_days_runtime {
namespace {

constexpr const char* kTag = "SpecialDays";
constexpr int64_t kMinValidEpoch = 1704067200;  // 2024-01-01: before this the clock isn't set

bool LocalDate(greeting::Date* out)
{
    const time_t now = time(nullptr);
    if (static_cast<int64_t>(now) < kMinValidEpoch) {
        return false;
    }
    std::tm local = {};
    localtime_r(&now, &local);
    *out = {local.tm_year + 1900, local.tm_mon + 1, local.tm_mday};
    return true;
}

}  // namespace

greeting::People PeopleFrom(const app_state_service::PersonalSettings& personal)
{
    greeting::People people;
    people.name = personal.friend_name;
    people.birthday = personal.birthday;
    people.anniversary = personal.anniversary;
    people.first_date = personal.first_date;
    people.holidays = personal.holidays >= 0 ? static_cast<uint8_t>(personal.holidays)
                                             : greeting::kAllHolidays;
    people.custom_days = greeting::ParseCustomDays(personal.special_days);
    return people;
}

std::vector<greeting::Occasion> Today(const app_state_service::PersonalSettings& personal)
{
    greeting::Date today;
    if (!LocalDate(&today)) {
        return {};
    }
    return greeting::OccasionsOn(PeopleFrom(personal), today);
}

void ShowCardIfDue()
{
    greeting::Date today;
    if (!LocalDate(&today)) {
        return;
    }
    const std::vector<greeting::Occasion> occasions =
        greeting::OccasionsOn(PeopleFrom(app_state_service::Personal()), today);
    if (occasions.empty()) {
        return;
    }
    const greeting::Occasion& occasion = occasions.front();
    char stamp[48];
    std::snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d %s", today.year, today.month, today.day,
                  occasion.key.c_str());
    if (app_state_service::LastDayCard() == stamp) {
        return;  // already shown today
    }
    const esp_err_t err = overlay_runtime::ShowNoteCard(
        epaper_ui::ToDisplayText(occasion.title), epaper_ui::ToDisplayText(occasion.message),
        epaper_ui::OccasionMascot(occasion.mascot));
    if (err == ESP_OK) {
        app_state_service::SetLastDayCard(stamp);
        ESP_LOGI(kTag, "Showed today's card (%s)", occasion.key.c_str());
    }
}

}  // namespace special_days_runtime

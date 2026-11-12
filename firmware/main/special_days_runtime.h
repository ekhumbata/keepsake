#ifndef SPECIAL_DAYS_RUNTIME_H_
#define SPECIAL_DAYS_RUNTIME_H_

#include <vector>

#include "app_state_service.h"
#include "greeting/special_days.h"

// Special days on the device (#85): today's occasions from the dates the provisioning file
// saved, and the once-a-day card with its mascot.
namespace special_days_runtime {

greeting::People PeopleFrom(const app_state_service::PersonalSettings& personal);

// Today's occasions, most personal first. Empty when the clock isn't set yet.
std::vector<greeting::Occasion> Today(const app_state_service::PersonalSettings& personal);

// Shows today's first occasion as a card with its mascot, once per day. Call when Home shows.
void ShowCardIfDue();

}  // namespace special_days_runtime

#endif  // SPECIAL_DAYS_RUNTIME_H_

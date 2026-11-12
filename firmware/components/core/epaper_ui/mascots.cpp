#include "epaper_ui/mascots.h"

#include "project_assets.h"

namespace epaper_ui {

const EmbeddedImageAsset* OccasionMascot(greeting::Mascot mascot)
{
    switch (mascot) {
        case greeting::Mascot::kBirthday:
            return project_assets::GetImage(EmbeddedImageId::kMascotDayBirthday);
        case greeting::Mascot::kChristmas:
            return project_assets::GetImage(EmbeddedImageId::kMascotDayChristmas);
        case greeting::Mascot::kNewYear:
            return project_assets::GetImage(EmbeddedImageId::kMascotDayNewYear);
        case greeting::Mascot::kValentines:
            return project_assets::GetImage(EmbeddedImageId::kMascotDayValentines);
        case greeting::Mascot::kAnniversary:
            return project_assets::GetImage(EmbeddedImageId::kMascotDayAnniversary);
        case greeting::Mascot::kFirstDate:
            return project_assets::GetImage(EmbeddedImageId::kMascotDayFirstDate);
        case greeting::Mascot::kHalloween:
            return project_assets::GetImage(EmbeddedImageId::kMascotDayHalloween);
        case greeting::Mascot::kCelebrate:
        default:
            return project_assets::GetImage(EmbeddedImageId::kMascotDayCelebrate);
    }
}

}  // namespace epaper_ui

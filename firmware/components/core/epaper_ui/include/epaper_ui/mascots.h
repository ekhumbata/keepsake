#ifndef EPAPER_UI_MASCOTS_H_
#define EPAPER_UI_MASCOTS_H_

#include "asset_types.h"
#include "greeting/special_days.h"

// The mascot pictures (#20) for each special day (#85).
namespace epaper_ui {

const EmbeddedImageAsset* OccasionMascot(greeting::Mascot mascot);

}  // namespace epaper_ui

#endif  // EPAPER_UI_MASCOTS_H_

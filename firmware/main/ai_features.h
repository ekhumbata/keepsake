#ifndef AI_FEATURES_H_
#define AI_FEATURES_H_

#include "gemini_service.h"

// No-key mode (#44): Keepsake must feel complete without a Gemini key. Everything that
// mentions AI (transcription, summaries, "waiting to transcribe" badges, the Gemini onboarding
// slide) asks this first; without a key, recordings are simply playable voice notes.
namespace ai_features {

inline bool Enabled()
{
    return gemini_service::HasApiKey();
}

}  // namespace ai_features

#endif  // AI_FEATURES_H_

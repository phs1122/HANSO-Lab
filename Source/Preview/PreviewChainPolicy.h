#pragma once

#include <JuceHeader.h>

#include "Capture/CaptureType.h"
#include "Model/HansoCategory.h"

namespace hanso
{
// Tone Preview complement policy: which chain stages the preview should
// auto-attach around a loaded package so every capture type is audible in a
// sensible context.
//
//   Full rig    -> nothing (the package already contains amp + cab)
//   Amp head    -> complement cabinet
//   Pre amp     -> neutral power (unity, nothing to add) + complement cabinet
//   Pedal       -> neutral amp + FRFR (both unity, so no complement stage)
//   Cabinet     -> neutral DI front-end (the existing cabinet IR preview path)
//
// The neutral amp/power blocks are intentionally unity gain, so only the
// cabinet complement exists as an actual processor stage. These are preview
// defaults; the user keeps the manual override and nothing here is written
// into the package.

inline bool previewComplementCabDefaultForCaptureType(CaptureType type) noexcept
{
    switch (type)
    {
        case CaptureType::Amp:
        case CaptureType::PreAmp:
            return true;

        case CaptureType::FullRig:
        case CaptureType::Cabinet:
        case CaptureType::Pedal:
        case CaptureType::Effect:
            return false;
    }

    return true;
}

// Packages carry the specific capture kind in metadata.deviceType (the
// CaptureType string); category is the coarser fallback for packages written
// by other producers. Unknown metadata keeps the legacy default (cab on).
inline bool previewComplementCabDefaultForPackage(const juce::String& deviceType,
                                                  HansoCategory category) noexcept
{
    if (deviceType == "Amp" || deviceType == "PreAmp")
        return true;
    if (deviceType == "FullRig" || deviceType == "Cabinet"
        || deviceType == "Pedal" || deviceType == "Effect")
        return false;

    switch (category)
    {
        case HansoCategory::Amp: return true;
        case HansoCategory::Rig:
        case HansoCategory::Pedal:
        case HansoCategory::Cabinet:
        case HansoCategory::Speaker:
            return false;
        case HansoCategory::Pickup:
        case HansoCategory::Utility:
        case HansoCategory::Unknown:
            break;
    }

    return true;
}
}

#pragma once

#include <JuceHeader.h>

#include "Model/HansoCategory.h"

namespace hanso
{
enum class CaptureType
{
    Amp,
    Cabinet,
    Pedal,
    Effect,
    FullRig
};

inline juce::String toString(CaptureType type)
{
    switch (type)
    {
        case CaptureType::Amp: return "Amp";
        case CaptureType::Cabinet: return "Cabinet";
        case CaptureType::Pedal: return "Pedal";
        case CaptureType::Effect: return "Effect";
        case CaptureType::FullRig: return "FullRig";
    }

    return "Amp";
}

inline juce::String displayName(CaptureType type)
{
    switch (type)
    {
        case CaptureType::Amp: return "Amp Capture";
        case CaptureType::Cabinet: return "Cabinet Capture";
        case CaptureType::Pedal: return "Pedal Capture";
        case CaptureType::Effect: return "Effect Capture";
        case CaptureType::FullRig: return "Full Rig Capture";
    }

    return "Amp Capture";
}

inline CaptureType captureTypeFromString(const juce::String& text)
{
    if (text.equalsIgnoreCase("Cabinet") || text.equalsIgnoreCase("Cabinet Capture")) return CaptureType::Cabinet;
    if (text.equalsIgnoreCase("Pedal") || text.equalsIgnoreCase("Pedal Capture")) return CaptureType::Pedal;
    if (text.equalsIgnoreCase("Effect") || text.equalsIgnoreCase("Effect Capture")) return CaptureType::Effect;
    if (text.equalsIgnoreCase("FullRig") || text.equalsIgnoreCase("Full Rig") || text.equalsIgnoreCase("Full Rig Capture")) return CaptureType::FullRig;
    return CaptureType::Amp;
}

inline HansoCategory categoryForCaptureType(CaptureType type)
{
    switch (type)
    {
        case CaptureType::Amp: return HansoCategory::Amp;
        case CaptureType::Cabinet: return HansoCategory::Cabinet;
        case CaptureType::Pedal: return HansoCategory::Pedal;
        case CaptureType::Effect: return HansoCategory::Utility;
        case CaptureType::FullRig: return HansoCategory::Rig;
    }

    return HansoCategory::Amp;
}
}

#pragma once

#include <JuceHeader.h>

#include "App/Utf8.h"

namespace hanso
{
enum class CaptureMode
{
    Standard,
    Easy
};

enum class CableGuideType
{
    NormalTsCable,
    TrsToDualTsYCable
};

inline juce::String toString(CaptureMode mode)
{
    return mode == CaptureMode::Easy ? "Easy" : "Standard";
}

inline juce::String toKoreanString(CaptureMode mode)
{
    return mode == CaptureMode::Easy ? utf8("간편 캡쳐") : utf8("일반 캡쳐");
}

inline juce::String toString(CableGuideType type)
{
    return type == CableGuideType::TrsToDualTsYCable ? "TrsToDualTsYCable" : "NormalTsCable";
}
}

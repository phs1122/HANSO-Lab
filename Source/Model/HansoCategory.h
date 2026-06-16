#pragma once

#include <JuceHeader.h>

namespace hanso
{
enum class HansoCategory
{
    Amp,
    Pedal,
    Cabinet,
    Speaker,
    Pickup,
    Rig,
    Utility,
    Unknown
};

inline juce::String toString(HansoCategory category)
{
    switch (category)
    {
        case HansoCategory::Amp: return "Amp";
        case HansoCategory::Pedal: return "Pedal";
        case HansoCategory::Cabinet: return "Cabinet";
        case HansoCategory::Speaker: return "Speaker";
        case HansoCategory::Pickup: return "Pickup";
        case HansoCategory::Rig: return "Rig";
        case HansoCategory::Utility: return "Utility";
        case HansoCategory::Unknown: return "Unknown";
    }

    return "Unknown";
}

inline HansoCategory hansoCategoryFromString(const juce::String& text)
{
    if (text == "Amp") return HansoCategory::Amp;
    if (text == "Pedal") return HansoCategory::Pedal;
    if (text == "Cabinet") return HansoCategory::Cabinet;
    if (text == "Speaker") return HansoCategory::Speaker;
    if (text == "Pickup") return HansoCategory::Pickup;
    if (text == "Rig") return HansoCategory::Rig;
    if (text == "Utility") return HansoCategory::Utility;
    return HansoCategory::Unknown;
}

inline juce::String assetTypeForCategory(HansoCategory category)
{
    switch (category)
    {
        case HansoCategory::Amp: return "amp";
        case HansoCategory::Cabinet: return "cabinet";
        case HansoCategory::Pedal: return "pedal";
        case HansoCategory::Speaker: return "speaker";
        case HansoCategory::Pickup: return "pickup";
        case HansoCategory::Rig: return "full-rig";
        case HansoCategory::Utility: return "utility";
        case HansoCategory::Unknown: return "unknown";
    }

    return "unknown";
}

inline HansoCategory hansoCategoryFromAssetType(const juce::String& assetType)
{
    if (assetType.equalsIgnoreCase("amp")) return HansoCategory::Amp;
    if (assetType.equalsIgnoreCase("cabinet")) return HansoCategory::Cabinet;
    if (assetType.equalsIgnoreCase("pedal")) return HansoCategory::Pedal;
    if (assetType.equalsIgnoreCase("effect")) return HansoCategory::Utility;
    if (assetType.equalsIgnoreCase("speaker")) return HansoCategory::Speaker;
    if (assetType.equalsIgnoreCase("pickup")) return HansoCategory::Pickup;
    if (assetType.equalsIgnoreCase("full-rig") || assetType.equalsIgnoreCase("fullrig") || assetType.equalsIgnoreCase("rig"))
        return HansoCategory::Rig;
    if (assetType.equalsIgnoreCase("utility")) return HansoCategory::Utility;
    return HansoCategory::Unknown;
}

inline juce::StringArray allHansoCategoryNames()
{
    return { "Amp", "Pedal", "Cabinet", "Speaker", "Pickup", "Rig", "Utility", "Unknown" };
}
}

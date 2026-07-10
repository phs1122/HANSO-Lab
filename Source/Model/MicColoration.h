#pragma once

#include <JuceHeader.h>

namespace hanso
{
// Broad microphone family used for cabinet IR coloration analysis. Slot-level
// metadata stores the class (plus the free-text model name) so the mic matrix
// estimator can de-embed the known coloration from a reference IR.
enum class CabinetMicClass
{
    Unknown = 0,
    Dynamic,
    Ribbon,
    Condenser
};

inline juce::String toString(CabinetMicClass micClass)
{
    switch (micClass)
    {
        case CabinetMicClass::Unknown: return "unknown";
        case CabinetMicClass::Dynamic: return "dynamic";
        case CabinetMicClass::Ribbon: return "ribbon";
        case CabinetMicClass::Condenser: return "condenser";
    }

    return "unknown";
}

inline CabinetMicClass cabinetMicClassFromString(const juce::String& text)
{
    if (text == "dynamic") return CabinetMicClass::Dynamic;
    if (text == "ribbon") return CabinetMicClass::Ribbon;
    if (text == "condenser") return CabinetMicClass::Condenser;
    return CabinetMicClass::Unknown;
}

inline juce::String cabinetMicClassLabel(CabinetMicClass micClass)
{
    switch (micClass)
    {
        case CabinetMicClass::Unknown: return "Unknown";
        case CabinetMicClass::Dynamic: return "Dynamic";
        case CabinetMicClass::Ribbon: return "Ribbon";
        case CabinetMicClass::Condenser: return "Condenser";
    }

    return "Unknown";
}

// Suggests a mic class from a free-text model name ("Shure SM57", "R-121", ...).
// Matching is case-insensitive and ignores spaces/dashes so common spelling
// variants resolve to the same entry. Unrecognised names return Unknown.
inline CabinetMicClass suggestMicClassForModelName(const juce::String& modelName)
{
    const auto normalized = modelName.toLowerCase().removeCharacters(" -_.");
    if (normalized.isEmpty())
        return CabinetMicClass::Unknown;

    static const char* const dynamicModels[] = {
        "sm57", "sm58", "sm7", "e606", "e609", "e906", "md421", "md441",
        "re20", "re320", "m88", "pr30", "i5", "d112", "unidyne", "pga57",
    };
    static const char* const ribbonModels[] = {
        "r121", "r122", "r10", "r92", "r84", "m160", "m130", "4038",
        "fathead", "sf12", "sf24", "roswellite",
    };
    static const char* const condenserModels[] = {
        "c414", "c214", "c451", "km184", "km84", "u87", "u67", "u47",
        "tlm102", "tlm103", "tlm49", "sm81", "nt1", "nt2", "c1000", "mk4",
    };

    for (const auto* model : dynamicModels)
        if (normalized.contains(model))
            return CabinetMicClass::Dynamic;
    for (const auto* model : ribbonModels)
        if (normalized.contains(model))
            return CabinetMicClass::Ribbon;
    for (const auto* model : condenserModels)
        if (normalized.contains(model))
            return CabinetMicClass::Condenser;

    return CabinetMicClass::Unknown;
}
}

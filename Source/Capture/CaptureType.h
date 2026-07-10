#pragma once

#include <JuceHeader.h>

#include "Model/HansoCategory.h"

namespace hanso
{
enum class CaptureType
{
    Amp,        // Amp Head (pre + power, no speaker)
    PreAmp,     // Pre Amp Only (preamp stage, no power amp / no cab)
    FullRig,    // Full Rig (combo or head+cab stack, amp+cab sound)
    Cabinet,    // speaker IR
    Pedal,      // Pedal / Effect
    Effect      // legacy alias, folded into Pedal in the UI
};

inline juce::String toString(CaptureType type)
{
    switch (type)
    {
        case CaptureType::Amp: return "Amp";
        case CaptureType::PreAmp: return "PreAmp";
        case CaptureType::FullRig: return "FullRig";
        case CaptureType::Cabinet: return "Cabinet";
        case CaptureType::Pedal: return "Pedal";
        case CaptureType::Effect: return "Effect";
    }

    return "Amp";
}

inline juce::String displayName(CaptureType type)
{
    switch (type)
    {
        case CaptureType::Amp: return "Amp Head Capture";
        case CaptureType::PreAmp: return "Pre Amp Capture";
        case CaptureType::FullRig: return "Full Rig Capture";
        case CaptureType::Cabinet: return "Cabinet Capture";
        case CaptureType::Pedal: return "Pedal / Effect Capture";
        case CaptureType::Effect: return "Pedal / Effect Capture";
    }

    return "Amp Head Capture";
}

inline CaptureType captureTypeFromString(const juce::String& text)
{
    if (text.equalsIgnoreCase("PreAmp") || text.equalsIgnoreCase("Pre Amp") || text.equalsIgnoreCase("Pre Amp Capture")) return CaptureType::PreAmp;
    if (text.equalsIgnoreCase("Combo") || text.equalsIgnoreCase("Combo Capture")) return CaptureType::FullRig; // legacy
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
        case CaptureType::PreAmp: return HansoCategory::Amp;
        case CaptureType::FullRig: return HansoCategory::Rig;   // amp + cab (combo/stack)
        case CaptureType::Cabinet: return HansoCategory::Cabinet;
        case CaptureType::Pedal: return HansoCategory::Pedal;
        case CaptureType::Effect: return HansoCategory::Pedal;
    }

    return HansoCategory::Amp;
}
}

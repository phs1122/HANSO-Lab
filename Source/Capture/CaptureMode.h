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

// Physical signal-path descriptors recorded in capture metadata. The capture
// target says what the user modelled; these say which physical chain the data
// actually passed through (CAPTURE_CONNECTION_POLICY §6): every capture measures
// the whole excitation + return chain, never the bare device.
enum class ExcitationPath
{
    Reamp,           // interface out -> reamp box -> device
    DirectLine,      // interface line out -> device (no reamp box)
    DirectHeadphone  // interface phones out -> device (no reamp box)
};

enum class ReturnPath
{
    Unknown,
    LineOrDi,        // device line/DI/FX-send out -> interface line input
    Loadbox,         // speaker out -> loadbox line out -> interface
    Microphone,      // speaker -> mic (-> preamp) -> interface
    InstrumentInput  // pedal out -> interface instrument/line input
};

inline juce::String toString(ExcitationPath path)
{
    switch (path)
    {
        case ExcitationPath::Reamp:           return "reamp";
        case ExcitationPath::DirectLine:      return "direct_line";
        case ExcitationPath::DirectHeadphone: return "direct_headphone";
    }
    return "direct_line";
}

inline juce::String toString(ReturnPath path)
{
    switch (path)
    {
        case ReturnPath::Unknown:         return "unknown";
        case ReturnPath::LineOrDi:        return "line_or_di";
        case ReturnPath::Loadbox:         return "loadbox_line";
        case ReturnPath::Microphone:      return "microphone";
        case ReturnPath::InstrumentInput: return "instrument_line";
    }
    return "unknown";
}

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

#pragma once

#include <JuceHeader.h>

namespace hanso
{
enum class CabinetSlotSource
{
    Empty,
    Capturing,
    CapturedIr,
    ImportedIr,
    EstimatedCompactCab,
    Error
};

inline juce::String toString(CabinetSlotSource source)
{
    switch (source)
    {
        case CabinetSlotSource::Empty: return "empty";
        case CabinetSlotSource::Capturing: return "capturing";
        case CabinetSlotSource::CapturedIr: return "captured-ir";
        case CabinetSlotSource::ImportedIr: return "imported-ir";
        case CabinetSlotSource::EstimatedCompactCab: return "estimated-compact-cab";
        case CabinetSlotSource::Error: return "error";
    }

    return "empty";
}

struct CabinetMicPositionSlot
{
    juce::String id;
    juce::String label;
    CabinetSlotSource source { CabinetSlotSource::Empty };
    juce::String impulseResponseChunkId;
    juce::String sourceFileName;
    juce::String estimatedFrom;
    juce::String errorMessage;

    bool hasRealSource() const noexcept
    {
        return source == CabinetSlotSource::CapturedIr || source == CabinetSlotSource::ImportedIr;
    }

    bool hasAnyData() const noexcept
    {
        return source != CabinetSlotSource::Empty;
    }
};
}

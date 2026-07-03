#pragma once

#include <JuceHeader.h>

#include "Model/CabinetToneProfile.h"

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

struct CabinetMicPositionSlot
{
    juce::String id;
    juce::String label;
    CabinetSlotSource source { CabinetSlotSource::Empty };
    juce::String impulseResponseChunkId;
    juce::String sourceFileName;
    juce::String estimatedFrom;
    juce::String errorMessage;
    CabinetToneProfile toneProfile;

    bool hasRealSource() const noexcept
    {
        return source == CabinetSlotSource::CapturedIr || source == CabinetSlotSource::ImportedIr;
    }

    bool hasAnyData() const noexcept
    {
        return source != CabinetSlotSource::Empty;
    }
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

inline CabinetSlotSource cabinetSlotSourceFromString(const juce::String& text)
{
    if (text == "capturing") return CabinetSlotSource::Capturing;
    if (text == "captured-ir") return CabinetSlotSource::CapturedIr;
    if (text == "imported-ir") return CabinetSlotSource::ImportedIr;
    if (text == "estimated-compact-cab") return CabinetSlotSource::EstimatedCompactCab;
    if (text == "error") return CabinetSlotSource::Error;
    return CabinetSlotSource::Empty;
}

inline juce::var cabinetSlotToVar(const CabinetMicPositionSlot& slot, bool includeErrorField)
{
    auto position = new juce::DynamicObject();
    position->setProperty("id", slot.id);
    position->setProperty("label", slot.label);
    position->setProperty("source", toString(slot.source));
    position->setProperty("impulseResponseChunkId",
                          slot.impulseResponseChunkId.isNotEmpty()
                              ? juce::var(slot.impulseResponseChunkId)
                              : juce::var());
    position->setProperty("sourceFileName",
                          slot.sourceFileName.isNotEmpty()
                              ? juce::var(slot.sourceFileName)
                              : juce::var());
    position->setProperty("estimatedFrom",
                          slot.estimatedFrom.isNotEmpty()
                              ? juce::var(slot.estimatedFrom)
                              : juce::var());
    position->setProperty("toneProfile", slot.toneProfile.toVar());
    if (includeErrorField)
    {
        position->setProperty("error",
                              slot.errorMessage.isNotEmpty()
                                  ? juce::var(slot.errorMessage)
                                  : juce::var());
    }

    return position;
}

inline juce::Array<juce::var> cabinetSlotsToVar(const std::vector<CabinetMicPositionSlot>& slots,
                                                bool includeErrorField)
{
    juce::Array<juce::var> positions;
    for (const auto& slot : slots)
        positions.add(cabinetSlotToVar(slot, includeErrorField));
    return positions;
}
}

#pragma once

#include <JuceHeader.h>

#include "Capture/CabinetMicPositions.h"
#include "Capture/CaptureStep.h"

namespace hanso
{
inline bool isCabinetPositionStep(const CaptureStep& step) noexcept
{
    return step.anchor.parameterKey == "cabinet-position";
}

inline juce::String dryChunkIdForStep(const CaptureStep& step)
{
    if (isCabinetPositionStep(step))
        return "capture/shared/cabinet-dry-reference.f32";

    return step.isAnchorCapture() ? step.anchor.chunkPathPrefix() + "/dry-reference.f32"
                                : "capture/" + step.stepId + "/dry-reference.f32";
}

inline juce::String legacySharedDryChunkIdForStep(const CaptureStep& step)
{
    return isCabinetPositionStep(step) ? "capture/shared/cabinet-dry-reference.f32"
                                      : "capture/shared/dry-reference.f32";
}

inline bool dryChunkIsSharedForStep(const CaptureStep& step)
{
    return dryChunkIdForStep(step) == legacySharedDryChunkIdForStep(step);
}

inline juce::String capturedChunkIdForStep(const CaptureStep& step)
{
    return step.isAnchorCapture() ? juce::String() : "capture/" + step.stepId + "/captured.f32";
}

inline juce::String alignedChunkIdForStep(const CaptureStep& step)
{
    if (isCabinetPositionStep(step))
        return impulseResponseChunkIdForCabinetPosition(step.stepId);

    return step.isAnchorCapture() ? step.anchor.chunkPathPrefix() + "/aligned-captured.f32"
                                : "capture/" + step.stepId + "/aligned-captured.f32";
}

// Sample-injection chunk ids are keyed by this sanitized form of the preview
// sample's file name; capture and Tone Preview must agree on it.
inline juce::String sanitizePreviewSampleId(const juce::String& name)
{
    juce::String id;
    for (const auto character : name.toLowerCase())
    {
        if (juce::CharacterFunctions::isLetterOrDigit(character))
            id += character;
        else if (! id.endsWith("-"))
            id += '-';
    }

    id = id.trimCharactersAtStart("-").trimCharactersAtEnd("-");
    return id.isNotEmpty() ? id : juce::String("sample");
}
}

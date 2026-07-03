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
        return "capture/shared/cabinet-dry-reference.pcm16";

    return step.isAnchorCapture() ? step.anchor.chunkPathPrefix() + "/dry-reference.pcm16"
                                : "capture/" + step.stepId + "/dry-reference.pcm16";
}

inline juce::String legacySharedDryChunkIdForStep(const CaptureStep& step)
{
    return isCabinetPositionStep(step) ? "capture/shared/cabinet-dry-reference.pcm16"
                                      : "capture/shared/dry-reference.pcm16";
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

    return step.isAnchorCapture() ? step.anchor.chunkPathPrefix() + "/aligned-captured.pcm16"
                                : "capture/" + step.stepId + "/aligned-captured.pcm16";
}
}

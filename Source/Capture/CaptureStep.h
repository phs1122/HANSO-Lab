#pragma once

#include <JuceHeader.h>

namespace hanso
{
enum class CaptureStepStatus
{
    NotStarted,
    Ready,
    InProgress,
    Passed,
    Warning,
    Failed
};

struct CaptureAnchor
{
    juce::String parameterKey;
    float normalizedValue { 0.0f };
    juce::String displayLabel;

    juce::String chunkPathPrefix() const
    {
        if (parameterKey == "gain")
            return "capture/gain-" + juce::String(static_cast<int>(std::round(normalizedValue * 100.0f))).paddedLeft('0', 3);

        return "capture/" + parameterKey + "-" + juce::String(static_cast<int>(std::round(normalizedValue * 100.0f))).paddedLeft('0', 3);
    }
};

struct CaptureStep
{
    juce::String stepId;
    juce::String title;
    juce::String instructionText;
    CaptureStepStatus status { CaptureStepStatus::NotStarted };
    CaptureAnchor anchor;
    bool required { true };

    bool isAnchorCapture() const noexcept { return ! anchor.parameterKey.isEmpty(); }
};

inline juce::String toString(CaptureStepStatus status)
{
    switch (status)
    {
        case CaptureStepStatus::NotStarted: return "NotStarted";
        case CaptureStepStatus::Ready: return "Ready";
        case CaptureStepStatus::InProgress: return "InProgress";
        case CaptureStepStatus::Passed: return "Passed";
        case CaptureStepStatus::Warning: return "Warning";
        case CaptureStepStatus::Failed: return "Failed";
    }

    return "Unknown";
}
}

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

enum class CaptureProbeVariant
{
    Default,
    HansoProbeA1Full,
    HansoProbeA1Delta
};

inline juce::String toString(CaptureProbeVariant variant)
{
    switch (variant)
    {
        case CaptureProbeVariant::Default: return "Default";
        case CaptureProbeVariant::HansoProbeA1Full: return "HansoProbeA1Full";
        case CaptureProbeVariant::HansoProbeA1Delta: return "HansoProbeA1Delta";
    }

    return "Default";
}

struct CaptureAnchor
{
    juce::String parameterKey;
    float normalizedValue { 0.0f };
    juce::String displayLabel;
    CaptureProbeVariant probeVariant { CaptureProbeVariant::Default };

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

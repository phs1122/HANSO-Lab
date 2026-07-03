#pragma once

#include <JuceHeader.h>

namespace hanso
{
enum class CaptureQualitySeverity
{
    Good,
    Warning,
    Error
};

struct CaptureQualityIssue
{
    CaptureQualitySeverity severity { CaptureQualitySeverity::Good };
    juce::String code;
    juce::String messageEnglish;
    juce::String messageKorean;
    juce::String suggestedActionEnglish;
    juce::String suggestedActionKorean;
};

struct CaptureQualityReport
{
    CaptureQualitySeverity overallSeverity { CaptureQualitySeverity::Good };
    float peakDbfs { -120.0f };
    float rmsDbfs { -120.0f };
    int clipSampleCount { 0 };
    float noiseFloorDbfs { -120.0f };
    float signalToNoiseDb { -120.0f };
    int latencySamples { 0 };
    double latencyMs { 0.0 };
    float alignmentConfidence { 0.0f };
    std::vector<CaptureQualityIssue> issues;

    bool hasWarnings() const noexcept { return overallSeverity == CaptureQualitySeverity::Warning; }
    bool hasErrors() const noexcept { return overallSeverity == CaptureQualitySeverity::Error; }
};

inline juce::String toString(CaptureQualitySeverity severity)
{
    switch (severity)
    {
        case CaptureQualitySeverity::Good: return "Good";
        case CaptureQualitySeverity::Warning: return "Warning";
        case CaptureQualitySeverity::Error: return "Error";
    }

    return "Unknown";
}
}

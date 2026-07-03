#pragma once

#include <JuceHeader.h>

#include "Analysis/CaptureQualityReport.h"

namespace hanso
{
struct CaptureQualityTarget
{
    float minimumRmsDbfs { -36.0f };
    float maximumRmsDbfs { -8.0f };
    juce::String label { "Default" };
    float minimumSnrDb { 30.0f };
    bool lowGainAnchor { false };
};

class CaptureQualityAnalyzer final
{
public:
    CaptureQualityReport analyze(const juce::AudioBuffer<float>& captured,
                                 int expectedSamples,
                                 int latencySamples,
                                 double latencyMs,
                                 float alignmentConfidence,
                                 bool easyModeRightMuted,
                                 CaptureQualityTarget target = {}) const;

private:
    static float gainToDb(float gain) noexcept;
};
}

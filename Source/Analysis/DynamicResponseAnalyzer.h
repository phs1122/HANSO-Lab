#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct DynamicResponseSummary
{
    float dynamicRangeDb { 0.0f };
    float envelopeAttackMs { 0.0f };
    float envelopeReleaseMs { 0.0f };
};

class DynamicResponseAnalyzer final
{
public:
    DynamicResponseSummary analyze(const juce::AudioBuffer<float>& buffer, double sampleRate) const;
};
}

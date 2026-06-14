#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct FrequencyResponseSummary
{
    float lowBandDb { -120.0f };
    float midBandDb { -120.0f };
    float highBandDb { -120.0f };
};

class FrequencyResponseAnalyzer final
{
public:
    FrequencyResponseSummary analyze(const juce::AudioBuffer<float>& buffer, double sampleRate) const;
};
}

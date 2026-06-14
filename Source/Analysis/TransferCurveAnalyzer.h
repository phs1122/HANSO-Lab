#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct TransferCurveSummary
{
    float inputRmsDb { -120.0f };
    float outputRmsDb { -120.0f };
    float estimatedGainDb { 0.0f };
};

class TransferCurveAnalyzer final
{
public:
    TransferCurveSummary analyze(const juce::AudioBuffer<float>& input,
                                 const juce::AudioBuffer<float>& output) const;
};
}

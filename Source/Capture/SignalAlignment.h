#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct SignalAlignmentResult
{
    int estimatedOffsetSamples { 0 };
    double estimatedOffsetMs { 0.0 };
    float confidence { 0.0f };
    juce::AudioBuffer<float> alignedCaptured;
};

class SignalAlignment final
{
public:
    SignalAlignmentResult estimateOffset(const juce::AudioBuffer<float>& dry,
                                         const juce::AudioBuffer<float>& captured,
                                         double sampleRate) const;
};
}

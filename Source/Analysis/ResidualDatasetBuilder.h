#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct ResidualDataset
{
    juce::OwnedArray<juce::AudioBuffer<float>> inputSegments;
    juce::OwnedArray<juce::AudioBuffer<float>> targetSegments;
    double sampleRate { 48000.0 };
    float normalizationGain { 1.0f };
    int segmentLength { 2048 };
    int hopSize { 1024 };
};

class ResidualDatasetBuilder final
{
public:
    ResidualDataset build(const juce::AudioBuffer<float>& input,
                          const juce::AudioBuffer<float>& target,
                          double sampleRate,
                          int segmentLength,
                          int hopSize) const;
};
}

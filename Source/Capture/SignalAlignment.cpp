#include "Capture/SignalAlignment.h"

namespace hanso
{
SignalAlignmentResult SignalAlignment::estimateOffset(const juce::AudioBuffer<float>& dry,
                                                      const juce::AudioBuffer<float>& captured,
                                                      double sampleRate) const
{
    SignalAlignmentResult result;
    const auto samples = juce::jmin(dry.getNumSamples(), captured.getNumSamples());
    if (samples <= 0 || dry.getNumChannels() == 0 || captured.getNumChannels() == 0)
        return result;

    auto bestCorrelation = -1.0f;
    auto bestOffset = 0;
    const auto maxSearch = juce::jmin(samples / 2, static_cast<int>(sampleRate));

    for (int offset = 0; offset < maxSearch; ++offset)
    {
        auto correlation = 0.0f;
        auto dryEnergy = 0.0f;
        auto capturedEnergy = 0.0f;
        const auto compareSamples = samples - offset;
        for (int i = 0; i < compareSamples; i += 4)
        {
            const auto drySample = dry.getSample(0, i);
            const auto capturedSample = captured.getSample(0, i + offset);
            correlation += drySample * capturedSample;
            dryEnergy += drySample * drySample;
            capturedEnergy += capturedSample * capturedSample;
        }

        const auto normalized = correlation / (std::sqrt(dryEnergy * capturedEnergy) + 1.0e-9f);

        if (normalized > bestCorrelation)
        {
            bestCorrelation = normalized;
            bestOffset = offset;
        }
    }

    result.estimatedOffsetSamples = bestOffset;
    result.estimatedOffsetMs = sampleRate > 0.0 ? 1000.0 * static_cast<double>(bestOffset) / sampleRate : 0.0;
    result.confidence = juce::jlimit(0.0f, 1.0f, bestCorrelation);

    const auto alignedSamples = juce::jmax(0, captured.getNumSamples() - bestOffset);
    result.alignedCaptured.setSize(captured.getNumChannels(), alignedSamples);
    for (int channel = 0; channel < captured.getNumChannels(); ++channel)
        result.alignedCaptured.copyFrom(channel, 0, captured, channel, bestOffset, alignedSamples);

    return result;
}
}

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

    auto computeCorrelation = [&dry, &captured, samples](int offset, int stride, int capturedChannel, bool rectified)
    {
        auto correlation = 0.0f;
        auto dryEnergy = 0.0f;
        auto capturedEnergy = 0.0f;
        const auto compareSamples = samples - offset;
        for (int i = 0; i < compareSamples; i += stride)
        {
            auto drySample = dry.getSample(0, i);
            auto capturedSample = captured.getSample(capturedChannel, i + offset);
            if (rectified)
            {
                drySample = std::abs(drySample);
                capturedSample = std::abs(capturedSample);
            }

            correlation += drySample * capturedSample;
            dryEnergy += drySample * drySample;
            capturedEnergy += capturedSample * capturedSample;
        }

        return correlation / (std::sqrt(dryEnergy * capturedEnergy) + 1.0e-9f);
    };

    auto bestCorrelation = -1.0f;
    auto bestOffset = 0;
    auto bestChannel = 0;
    auto bestRectified = false;
    const auto maxSearch = juce::jmin(samples / 2, static_cast<int>(sampleRate * 0.25));
    constexpr auto coarseOffsetStep = 32;
    constexpr auto coarseSampleStride = 32;
    constexpr auto fineSampleStride = 4;

    for (int channel = 0; channel < captured.getNumChannels(); ++channel)
    {
        for (const auto rectified : { false, true })
        {
            for (int offset = 0; offset < maxSearch; offset += coarseOffsetStep)
            {
                const auto normalized = computeCorrelation(offset, coarseSampleStride, channel, rectified);
                if (normalized > bestCorrelation)
                {
                    bestCorrelation = normalized;
                    bestOffset = offset;
                    bestChannel = channel;
                    bestRectified = rectified;
                }
            }
        }
    }

    const auto fineStart = juce::jmax(0, bestOffset - coarseOffsetStep);
    const auto fineEnd = juce::jmin(maxSearch, bestOffset + coarseOffsetStep);
    for (int offset = fineStart; offset <= fineEnd; ++offset)
    {
        const auto normalized = computeCorrelation(offset, fineSampleStride, bestChannel, bestRectified);
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

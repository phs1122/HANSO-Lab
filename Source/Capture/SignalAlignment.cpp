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

    auto computeCorrelation = [&dry, &captured, samples](int offset, int stride,
                                                         int capturedChannel, bool rectified)
    {
        auto cross = 0.0;
        auto drySum = 0.0;
        auto capturedSum = 0.0;
        auto drySquare = 0.0;
        auto capturedSquare = 0.0;
        auto count = 0;
        const auto compareSamples = samples - offset;
        for (int i = 0; i < compareSamples; i += stride)
        {
            auto drySample = static_cast<double>(dry.getSample(0, i));
            auto capturedSample = static_cast<double>(captured.getSample(capturedChannel, i + offset));
            if (rectified)
            {
                drySample = std::abs(drySample);
                capturedSample = std::abs(capturedSample);
            }

            cross += drySample * capturedSample;
            drySum += drySample;
            capturedSum += capturedSample;
            drySquare += drySample * drySample;
            capturedSquare += capturedSample * capturedSample;
            ++count;
        }

        if (count < 2)
            return 0.0f;

        const auto covariance = cross - drySum * capturedSum / count;
        const auto dryVariance = drySquare - drySum * drySum / count;
        const auto capturedVariance = capturedSquare - capturedSum * capturedSum / count;
        const auto denominator = std::sqrt(juce::jmax(0.0, dryVariance * capturedVariance));
        return denominator > 1.0e-12
             ? static_cast<float>(juce::jlimit(-1.0, 1.0, covariance / denominator))
             : 0.0f;
    };

    auto bestScore = -1.0f;
    auto bestCorrelation = 0.0f;
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
                const auto score = std::abs(normalized);
                if (score > bestScore)
                {
                    bestScore = score;
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
        const auto score = std::abs(normalized);
        if (score > bestScore)
        {
            bestScore = score;
            bestCorrelation = normalized;
            bestOffset = offset;
        }
    }

    result.estimatedOffsetSamples = bestOffset;
    result.estimatedOffsetMs = sampleRate > 0.0 ? 1000.0 * static_cast<double>(bestOffset) / sampleRate : 0.0;
    result.confidence = juce::jlimit(0.0f, 1.0f, bestScore);
    result.mode = bestRectified ? SignalAlignmentMode::RectifiedCentered
                                : SignalAlignmentMode::Direct;
    result.polarity = bestCorrelation < 0.0f ? -1 : 1;

    const auto alignedSamples = juce::jmax(0, captured.getNumSamples() - bestOffset);
    result.alignedCaptured.setSize(captured.getNumChannels(), alignedSamples);
    for (int channel = 0; channel < captured.getNumChannels(); ++channel)
        result.alignedCaptured.copyFrom(channel, 0, captured, channel, bestOffset, alignedSamples);

    return result;
}
}

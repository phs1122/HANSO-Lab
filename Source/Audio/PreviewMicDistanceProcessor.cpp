#include "Audio/PreviewMicDistanceProcessor.h"

#include <cmath>

namespace hanso
{
namespace
{
constexpr float distanceEpsilonCm = 0.01f;
}

void PreviewMicDistanceProcessor::prepare(double newSampleRate, int outputChannels) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    preparedChannels = juce::jlimit(1, maxChannels, outputChannels);
    loadedDistanceCm = -1.0f;
    reset();
}

void PreviewMicDistanceProcessor::reset() noexcept
{
    for (auto& filter : filters)
        filter.reset();
    currentGainLinear = targetGainLinear;
}

void PreviewMicDistanceProcessor::setDistanceCm(float distanceCm) noexcept
{
    targetDistanceCm.store(juce::jlimit(minimumDistanceCm, maximumDistanceCm, distanceCm));
}

PreviewMicDistanceProcessor::LowPassCoefficients PreviewMicDistanceProcessor::makeLowPass(double rate,
                                                                                            double cutoffHz) noexcept
{
    const auto cutoff = juce::jlimit(20.0, rate * 0.45, cutoffHz);
    const auto w0 = juce::MathConstants<double>::twoPi * cutoff / rate;
    const auto cosW0 = std::cos(w0);
    const auto alpha = std::sin(w0) / (2.0 * 0.7071067811865476);

    const auto b0 = (1.0 - cosW0) * 0.5;
    const auto b1 = 1.0 - cosW0;
    const auto b2 = b0;
    const auto a0 = 1.0 + alpha;
    const auto a1 = -2.0 * cosW0;
    const auto a2 = 1.0 - alpha;

    return { static_cast<float>(b0 / a0), static_cast<float>(b1 / a0), static_cast<float>(b2 / a0),
             static_cast<float>(a1 / a0), static_cast<float>(a2 / a0) };
}

void PreviewMicDistanceProcessor::rebuildIfNeeded() noexcept
{
    const auto distanceCm = targetDistanceCm.load();
    if (std::abs(distanceCm - loadedDistanceCm) < distanceEpsilonCm)
        return;

    const auto excessDistanceCm = juce::jmax(0.0f, distanceCm - referenceDistanceCm);
    distanceEffectActive = excessDistanceCm > distanceEpsilonCm;
    if (! distanceEffectActive)
    {
        targetGainLinear = 1.0f;
        loadedDistanceCm = distanceCm;
        return;
    }

    // The close-mic reference is 2 cm. Farther placements lose level with
    // distance and progressively soften the upper spectrum through air loss.
    const auto gainDb = juce::jmax(-18.0f,
                                   -6.0f * std::log2(juce::jmax(1.0f, distanceCm / referenceDistanceCm)));
    const auto cutoffHz = 24000.0 * std::exp(-static_cast<double>(excessDistanceCm) / 20.0);
    coefficients = makeLowPass(sampleRate, cutoffHz);
    targetGainLinear = juce::Decibels::decibelsToGain(gainDb);
    loadedDistanceCm = distanceCm;
}

void PreviewMicDistanceProcessor::process(juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    rebuildIfNeeded();
    if (! distanceEffectActive)
        return;

    const auto channels = juce::jmin(buffer.getNumChannels(), preparedChannels);
    for (int channel = 0; channel < channels; ++channel)
    {
        auto* samples = buffer.getWritePointer(channel);
        auto& filter = filters[static_cast<size_t>(channel)];
        auto gain = currentGainLinear;
        for (int sample = 0; sample < numSamples; ++sample)
        {
            gain += (targetGainLinear - gain) * 0.002f;
            samples[sample] = filter.process(samples[sample], coefficients) * gain;
        }

        if (channel == channels - 1)
            currentGainLinear = gain;
    }
}
}

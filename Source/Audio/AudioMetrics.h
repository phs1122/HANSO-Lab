#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct AudioLevelMetrics
{
    float peak { 0.0f };
    float rms { 0.0f };
};

inline AudioLevelMetrics measureBufferLevels(const juce::AudioBuffer<float>& buffer) noexcept
{
    AudioLevelMetrics metrics;

    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return metrics;

    auto rmsSum = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        metrics.peak = juce::jmax(metrics.peak, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
        rmsSum += buffer.getRMSLevel(channel, 0, buffer.getNumSamples());
    }

    metrics.rms = rmsSum / static_cast<float>(juce::jmax(1, buffer.getNumChannels()));
    return metrics;
}

inline float peakDb(const juce::AudioBuffer<float>& buffer, float silenceDb = -120.0f) noexcept
{
    return juce::Decibels::gainToDecibels(measureBufferLevels(buffer).peak, silenceDb);
}

inline float rmsDb(const juce::AudioBuffer<float>& buffer, float silenceDb = -120.0f) noexcept
{
    return juce::Decibels::gainToDecibels(measureBufferLevels(buffer).rms, silenceDb);
}
}

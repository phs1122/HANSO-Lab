#pragma once

#include <JuceHeader.h>

namespace hanso
{
enum class SignalAlignmentMode
{
    None,
    Direct,
    RectifiedCentered
};

inline juce::String toString(SignalAlignmentMode mode)
{
    switch (mode)
    {
        case SignalAlignmentMode::None: return "none";
        case SignalAlignmentMode::Direct: return "direct";
        case SignalAlignmentMode::RectifiedCentered: return "rectified";
    }

    return "none";
}

struct SignalAlignmentResult
{
    int estimatedOffsetSamples { 0 };
    double estimatedOffsetMs { 0.0 };
    float confidence { 0.0f };
    SignalAlignmentMode mode { SignalAlignmentMode::None };
    int polarity { 1 };
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

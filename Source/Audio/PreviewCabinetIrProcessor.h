#pragma once

#include <JuceHeader.h>

#include "Capture/CabinetCaptureState.h"
#include "Model/HansoPackage.h"

namespace hanso
{
class PreviewCabinetIrProcessor final
{
public:
    void prepare(double sampleRate, int maximumBlockSize, int outputChannels) noexcept;
    void reset() noexcept;
    void clear() noexcept;
    bool loadFromPackage(const HansoPackage& package, juce::String& error);
    void setMicPositionNormalized(float normalizedPosition) noexcept;
    bool hasModel() const noexcept;
    juce::String summary() const;
    void process(const float* const* inputChannels,
                 int numInputChannels,
                 juce::AudioBuffer<float>& outputBuffer,
                 int numSamples) noexcept;

private:
    struct PositionIr
    {
        juce::String id;
        float normalizedPosition { 0.0f };
        juce::String label;
        CabinetSlotSource source { CabinetSlotSource::Empty };
        juce::AudioBuffer<float> impulseResponse;
    };

    static CabinetSlotSource sourceFromString(const juce::String& text) noexcept;
    static bool loadPositionIr(const HansoPackage& package,
                               const juce::String& chunkId,
                               PositionIr& destination,
                               juce::String& error);
    void rebuildConvolutionIfNeeded() noexcept;
    void buildBlendedImpulse(float normalizedPosition, juce::AudioBuffer<float>& blended) const;

    std::vector<PositionIr> positions;
    juce::dsp::Convolution convolution;
    juce::dsp::ProcessSpec processSpec;
    std::atomic<float> micPosition { 0.5f };
    float loadedMicPosition { -1.0f };
    bool modelLoaded { false };
    juce::String summaryText;
};
}

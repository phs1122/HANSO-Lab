#pragma once

#include <JuceHeader.h>

#include "Model/CompactHansoModel.h"

namespace hanso
{
class PreviewModelProcessor final
{
public:
    void prepare(double newSampleRate, int maximumBlockSize, int outputChannels) noexcept;
    void clear() noexcept;
    bool loadModel(const CompactHansoModel& model);
    void setPreviewGainPercent(float percent) noexcept;
    bool hasModel() const noexcept;
    juce::String summary() const;
    void reset() noexcept;

    void process(const float* const* inputChannels,
                 int numInputChannels,
                 juce::AudioBuffer<float>& outputBuffer) noexcept;

private:
    struct BiquadCoefficients
    {
        float b0 { 1.0f };
        float b1 { 0.0f };
        float b2 { 0.0f };
        float a1 { 0.0f };
        float a2 { 0.0f };
    };

    struct BiquadState
    {
        float z1 { 0.0f };
        float z2 { 0.0f };

        float process(float input, const BiquadCoefficients& coeffs) noexcept
        {
            const auto output = coeffs.b0 * input + z1;
            z1 = coeffs.b1 * input - coeffs.a1 * output + z2;
            z2 = coeffs.b2 * input - coeffs.a2 * output;
            return output;
        }

        void reset() noexcept
        {
            z1 = 0.0f;
            z2 = 0.0f;
        }
    };

    static BiquadCoefficients makeLowShelf(double sampleRate, double frequencyHz, float gainDb) noexcept;
    static BiquadCoefficients makePeak(double sampleRate, double frequencyHz, float q, float gainDb) noexcept;
    static BiquadCoefficients makeHighShelf(double sampleRate, double frequencyHz, float gainDb) noexcept;
    static BiquadCoefficients normalise(float b0, float b1, float b2, float a0, float a1, float a2) noexcept;

    static constexpr int maxChannels = 2;
    static constexpr int filterCount = 3;
    static constexpr int maxAnchors = 8;

    struct RuntimeAnchor
    {
        float parameterValue { 0.5f };
        float inputGainDb { 0.0f };
        float drive { 1.0f };
        float outputGainDb { 0.0f };
        float lowShelfDb { 0.0f };
        float midPeakDb { 0.0f };
        float highShelfDb { 0.0f };
    };

    RuntimeAnchor interpolateAnchor(float parameterValue) const noexcept;
    void updateToneCoefficients(const RuntimeAnchor& anchor) noexcept;

    std::array<std::array<BiquadState, filterCount>, maxChannels> states;
    std::array<BiquadCoefficients, filterCount> coefficients;
    std::array<RuntimeAnchor, maxAnchors> anchors;
    int anchorCount { 0 };
    double sampleRate { 48000.0 };
    int preparedChannels { 2 };
    float currentInputGain { 1.0f };
    float currentDrive { 1.0f };
    float currentDriveNormalizer { 1.0f };
    float currentOutputGain { 1.0f };
    std::atomic<float> targetParameter { 0.5f };
    bool modelLoaded { false };
    juce::String modelSummary;
};
}

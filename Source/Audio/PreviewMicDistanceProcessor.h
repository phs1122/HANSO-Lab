#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>

namespace hanso
{
// Preview-only close-mic distance approximation. Position is selected
// separately; this stage only models extra distance from the cabinet face.
class PreviewMicDistanceProcessor final
{
public:
    static constexpr float minimumDistanceCm = 0.5f;
    static constexpr float referenceDistanceCm = 2.0f;
    static constexpr float maximumDistanceCm = 30.0f;

    void prepare(double sampleRate, int outputChannels) noexcept;
    void reset() noexcept;
    void setDistanceCm(float distanceCm) noexcept;
    void process(juce::AudioBuffer<float>& buffer, int numSamples) noexcept;

private:
    struct LowPassCoefficients
    {
        float b0 { 1.0f };
        float b1 { 0.0f };
        float b2 { 0.0f };
        float a1 { 0.0f };
        float a2 { 0.0f };
    };

    struct LowPassState
    {
        float z1 { 0.0f };
        float z2 { 0.0f };

        float process(float input, const LowPassCoefficients& coeffs) noexcept
        {
            const auto output = coeffs.b0 * input + z1;
            z1 = coeffs.b1 * input - coeffs.a1 * output + z2;
            z2 = coeffs.b2 * input - coeffs.a2 * output;
            return output;
        }

        void reset() noexcept { z1 = z2 = 0.0f; }
    };

    static constexpr int maxChannels = 2;

    static LowPassCoefficients makeLowPass(double sampleRate, double cutoffHz) noexcept;
    void rebuildIfNeeded() noexcept;

    double sampleRate { 48000.0 };
    int preparedChannels { 2 };
    std::atomic<float> targetDistanceCm { referenceDistanceCm };
    float loadedDistanceCm { -1.0f };
    float targetGainLinear { 1.0f };
    float currentGainLinear { 1.0f };
    bool distanceEffectActive { false };
    LowPassCoefficients coefficients;
    std::array<LowPassState, maxChannels> filters {};
};
}

#pragma once

#include <JuceHeader.h>

namespace hanso
{
// Preview cabinet shaping based on Guitar Plugins grt_fx/CabinetModelFx
// (American Combo + Dynamic mic + Cap Edge defaults).
class PreviewCabinetProcessor final
{
public:
    void prepare(double sampleRate, int maximumBlockSize, int outputChannels) noexcept;
    void reset() noexcept;
    void process(juce::AudioBuffer<float>& buffer, int numSamples) noexcept;

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

        float process(float input, const BiquadCoefficients& coeffs) noexcept;
        void reset() noexcept;
    };

    struct ChannelFilters
    {
        BiquadState hpf;
        BiquadState lpf;
        BiquadState lowRes;
        BiquadState mid;
        BiquadState presence;
        BiquadState air;
        BiquadState micPresence;
        BiquadState micLowMid;
        BiquadState micHighRolloff;
        BiquadState micAir;
        BiquadState positionLowShelf;
        BiquadState positionMidPeak;
        BiquadState positionHighShelf;
        BiquadState positionHighCut;
    };

    struct EnvelopeFollower
    {
        float envelope { 0.0f };
        float attackCoeff { 0.01f };
        float releaseCoeff { 0.001f };

        void prepare(double sampleRate) noexcept;
        void reset() noexcept;
        float process(float input) noexcept;
    };

    static BiquadCoefficients makeHighPass(double sampleRate, double frequencyHz, float q) noexcept;
    static BiquadCoefficients makeLowPass(double sampleRate, double frequencyHz, float q) noexcept;
    static BiquadCoefficients makeLowShelf(double sampleRate, double frequencyHz, float q, float gainDb) noexcept;
    static BiquadCoefficients makeHighShelf(double sampleRate, double frequencyHz, float q, float gainDb) noexcept;
    static BiquadCoefficients makePeak(double sampleRate, double frequencyHz, float q, float gainDb) noexcept;
    static BiquadCoefficients normalise(float b0, float b1, float b2, float a0, float a1, float a2) noexcept;
    void updateCoefficients() noexcept;
    float processSample(float input, ChannelFilters& filters) noexcept;

    static constexpr int maxChannels = 2;
    static constexpr int coefficientCount = 14;

    double sampleRate { 48000.0 };
    int preparedChannels { 2 };
    float compressionAmount { 0.20f };
    float positionGainLinear { 1.0f };
    std::array<ChannelFilters, maxChannels> filters;
    std::array<EnvelopeFollower, maxChannels> compressors;
    std::array<float, maxChannels> compGain { 1.0f, 1.0f };
    std::array<BiquadCoefficients, coefficientCount> coefficients;
};
}

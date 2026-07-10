#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <vector>

#include "Model/CabinetToneProfile.h"
#include "Model/HansoPackage.h"
#include "Model/MicColoration.h"

namespace hanso
{
// Preview-only mic swap EQ for cabinet packages. Reads cabProfile.micMatrix
// and applies the band-gain difference between the selected target mic class
// and each position's reference mic on top of the IR preview output. With no
// matrix loaded or the target set to the original mic it is a strict bypass,
// so the protected IR preview path is unchanged by default.
class PreviewMicColorProcessor final
{
public:
    void prepare(double sampleRate, int outputChannels) noexcept;
    void reset() noexcept;
    void clear() noexcept;

    // Parses cabProfile.micMatrix and positions[].micClass. Returns false and
    // stays inactive when the package carries no matrix (older packages).
    bool loadFromPackage(const HansoPackage& package);

    // Unknown selects the original (per-position reference) mic = bypass.
    void setTargetMicClass(CabinetMicClass micClass) noexcept;
    void setMicPositionNormalized(float normalizedPosition) noexcept;
    bool hasMatrix() const noexcept;

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

        float process(float input, const BiquadCoefficients& coeffs) noexcept
        {
            const auto output = coeffs.b0 * input + z1;
            z1 = coeffs.b1 * input - coeffs.a1 * output + z2;
            z2 = coeffs.b2 * input - coeffs.a2 * output;
            return output;
        }

        void reset() noexcept { z1 = z2 = 0.0f; }
    };

    static constexpr int maxChannels = 2;
    static constexpr int classCount = 3; // Dynamic, Ribbon, Condenser

    struct PositionDeltas
    {
        float normalizedPosition { 0.0f };
        // [target class][band] band-gain delta vs the position's reference mic.
        std::array<std::array<float, static_cast<size_t>(cabinetToneBandCount)>, classCount> bandDeltasDb {};
        std::array<float, classCount> levelDeltasDb {};
    };

    static BiquadCoefficients makePeak(double sampleRate, double frequencyHz, float q, float gainDb) noexcept;
    void interpolateDeltas(float normalizedPosition,
                           int classIndex,
                           std::array<float, static_cast<size_t>(cabinetToneBandCount)>& bandsDb,
                           float& levelDb) const noexcept;
    void rebuildCoefficientsIfNeeded() noexcept;

    double sampleRate { 48000.0 };
    int preparedChannels { 2 };
    std::vector<PositionDeltas> positionDeltas;
    std::atomic<bool> matrixLoaded { false };
    std::atomic<int> targetClassIndex { -1 }; // -1 = original mic (bypass)
    std::atomic<float> micPositionNormalized { 0.0f };
    int loadedClassIndex { -2 };
    float loadedPosition { -1.0f };
    float targetLevelGainLinear { 1.0f };
    float currentLevelGainLinear { 1.0f };
    std::array<BiquadCoefficients, static_cast<size_t>(cabinetToneBandCount)> coefficients {};
    std::array<std::array<BiquadState, static_cast<size_t>(cabinetToneBandCount)>, maxChannels> filters {};
};
}

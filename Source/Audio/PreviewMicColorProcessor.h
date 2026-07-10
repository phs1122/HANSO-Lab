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
// Cabinet tone-profile renderer for the IR preview path. It calculates the
// delta from the real IR(s) selected for convolution to the requested mic
// position/mic-class profile. This lets an estimated slot remain audible even
// though it has no standalone IR chunk.
class PreviewMicColorProcessor final
{
public:
    void prepare(double sampleRate, int outputChannels) noexcept;
    void reset() noexcept;
    void clear() noexcept;

    // Parses cabProfile.positions and optional micMatrix. Older cabinet
    // packages without the matrix retain their real-IR behaviour.
    bool loadFromPackage(const HansoPackage& package);

    // Unknown keeps the source microphone class while still applying the
    // selected position's calculated profile.
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

    struct ProfileAnchor
    {
        float normalizedPosition { 0.0f };
        CabinetToneProfile profile;
        CabinetMicClass micClass { CabinetMicClass::Unknown };
    };

    static BiquadCoefficients makePeak(double sampleRate, double frequencyHz, float q, float gainDb) noexcept;
    static bool interpolateProfile(const std::vector<ProfileAnchor>& anchors,
                                   float normalizedPosition,
                                   CabinetToneProfile& destination,
                                   CabinetMicClass* nearestMicClass = nullptr) noexcept;
    bool targetProfileFor(float normalizedPosition,
                          CabinetMicClass sourceMicClass,
                          int requestedClassIndex,
                          CabinetToneProfile& destination) const noexcept;
    void rebuildCoefficientsIfNeeded() noexcept;

    double sampleRate { 48000.0 };
    int preparedChannels { 2 };
    std::vector<ProfileAnchor> sourceProfiles;
    std::vector<ProfileAnchor> positionProfiles;
    std::array<std::vector<ProfileAnchor>, classCount> matrixProfiles;
    std::atomic<bool> profileDataLoaded { false };
    std::atomic<bool> matrixLoaded { false };
    std::atomic<int> targetClassIndex { -1 }; // -1 = source/original mic
    std::atomic<float> micPositionNormalized { 0.0f };
    int loadedClassIndex { -2 };
    float loadedPosition { -1.0f };
    bool profileCorrectionActive { false }; // audio thread only
    float targetLevelGainLinear { 1.0f };
    float currentLevelGainLinear { 1.0f };
    std::array<BiquadCoefficients, static_cast<size_t>(cabinetToneBandCount)> coefficients {};
    std::array<std::array<BiquadState, static_cast<size_t>(cabinetToneBandCount)>, maxChannels> filters {};
};
}

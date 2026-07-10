#pragma once

#include <JuceHeader.h>

namespace hanso
{
enum class CalibrationValidationStatus
{
    MeasuringNoise,
    Passed,
    SignalTooLow,
    IdentityFailed,
    LoopbackSuspected
};

struct CalibrationValidationConfig
{
    float inputMinDbfs { -36.0f };
    float inputMaxDbfs { -8.0f };
    float outputMinDbfs { -42.0f };
    float outputMaxDbfs { -24.0f };
    float silentReturnLoopbackThresholdDbfs { -50.0f };
    float requiredSnrDb { 12.0f };
    float requiredToneDominanceDb { 6.0f };
};

struct CalibrationValidationResult
{
    CalibrationValidationStatus status { CalibrationValidationStatus::MeasuringNoise };
    bool inputLevelInRange { false };
    bool outputLevelInRange { false };
    bool snrOk { false };
    bool identityOk { false };
    bool muteDropRescuedIdentity { false };
    bool loopbackSuspected { false };
    float returnLevelDbfs { -120.0f };
    float returnRmsDbfs { -120.0f };
    float outputLevelDbfs { -120.0f };
    float noiseFloorDbfs { -120.0f };
    float signalToNoiseDb { -120.0f };
    float toneDominanceDb { -120.0f };
    float inBandDbfs { -120.0f };
    float outOfBandDbfs { -120.0f };
    juce::String code;
    juce::String messageEnglish;
    juce::String messageKorean;
};

class CalibrationValidator final
{
public:
    static float rmsDbfs(const juce::AudioBuffer<float>& buffer) noexcept;

    static CalibrationValidationResult evaluateSilentReturn(const juce::AudioBuffer<float>& recentReturn,
                                                            const CalibrationValidationConfig& config = {});

    static CalibrationValidationResult validateProbe(const juce::AudioBuffer<float>& recentReturn,
                                                     double sampleRate,
                                                     const double* probeFrequencies,
                                                     int numProbeFrequencies,
                                                     float returnLevelDbfs,
                                                     float outputLevelDbfs,
                                                     float noiseFloorDbfs,
                                                     const CalibrationValidationConfig& config = {},
                                                     bool muteDropIdentityConfirmed = false);

    // Distortion-robust identity check: heavy saturation spreads probe energy
    // into harmonics and can defeat the Goertzel tone-dominance test on an
    // honest signal. If muting the output makes the return drop by at least
    // requiredDropDb, the return demonstrably follows our output.
    static bool checkMuteDropIdentity(float preMuteReturnDbfs,
                                      float muteWindowReturnDbfs,
                                      float requiredDropDb = 6.0f) noexcept;

private:
    static double goertzelPower(const float* samples,
                                int numSamples,
                                double sampleRate,
                                double frequency) noexcept;
};
}

#pragma once

#include <array>
#include <vector>

#include <JuceHeader.h>

namespace hanso
{
enum class TestSignalType
{
    LogSineSweep,
    WhiteNoise,
    PinkNoise,
    ImpulseBurst,
    MultiSine,
    HansoProbeA1,
    CabinetProbeC1
};

enum class HansoProbeVariant
{
    Full,
    Delta
};

enum class HansoProbeSegmentPurpose
{
    Silence,
    Sync,
    Analysis,
    Training,
    Validation
};

struct HansoProbeSegment
{
    juce::String id;
    HansoProbeSegmentPurpose purpose { HansoProbeSegmentPurpose::Silence };
    int startSample { 0 };
    int numSamples { 0 };

    bool includeInModelFit() const noexcept
    {
        return purpose == HansoProbeSegmentPurpose::Analysis
            || purpose == HansoProbeSegmentPurpose::Training;
    }
};

struct TestSignalSpec
{
    TestSignalType type { TestSignalType::LogSineSweep };
    double sampleRate { 48000.0 };
    double durationSeconds { 5.0 };
    float amplitude { 0.2f };
    HansoProbeVariant probeVariant { HansoProbeVariant::Full };
};

class TestSignalGenerator final
{
public:
    juce::AudioBuffer<float> generate(const TestSignalSpec& spec) const;
    static juce::String toString(TestSignalType type);
    static juce::String toString(const TestSignalSpec& spec);
    static std::array<double, 10> multiSineFrequencies() noexcept;
    static double hansoProbeDurationSeconds(HansoProbeVariant variant) noexcept;
    static double cabinetProbeDurationSeconds() noexcept;
    static std::vector<HansoProbeSegment> hansoProbeSegments(HansoProbeVariant variant,
                                                              double sampleRate);

private:
    static void generateLogSineSweep(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec);
    static void generateWhiteNoise(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec);
    static void generatePinkNoise(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec);
    static void generateImpulseBurst(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec);
    static void generateMultiSine(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec);
    static void generateHansoProbeA1(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec);
    static void generateCabinetProbeC1(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec);
};
}

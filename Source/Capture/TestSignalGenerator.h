#pragma once

#include <JuceHeader.h>

namespace hanso
{
enum class TestSignalType
{
    LogSineSweep,
    WhiteNoise,
    PinkNoise,
    ImpulseBurst,
    MultiSine
};

struct TestSignalSpec
{
    TestSignalType type { TestSignalType::LogSineSweep };
    double sampleRate { 48000.0 };
    double durationSeconds { 5.0 };
    float amplitude { 0.2f };
};

class TestSignalGenerator final
{
public:
    juce::AudioBuffer<float> generate(const TestSignalSpec& spec) const;
    static juce::String toString(TestSignalType type);

private:
    static void generateLogSineSweep(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec);
    static void generateWhiteNoise(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec);
    static void generatePinkNoise(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec);
    static void generateImpulseBurst(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec);
    static void generateMultiSine(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec);
};
}

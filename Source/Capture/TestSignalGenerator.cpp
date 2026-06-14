#include "Capture/TestSignalGenerator.h"

namespace hanso
{
juce::AudioBuffer<float> TestSignalGenerator::generate(const TestSignalSpec& spec) const
{
    const auto numSamples = juce::jmax(1, static_cast<int>(spec.sampleRate * spec.durationSeconds));
    juce::AudioBuffer<float> buffer(1, numSamples);
    buffer.clear();

    switch (spec.type)
    {
        case TestSignalType::LogSineSweep: generateLogSineSweep(buffer, spec); break;
        case TestSignalType::WhiteNoise: generateWhiteNoise(buffer, spec); break;
        case TestSignalType::PinkNoise: generatePinkNoise(buffer, spec); break;
        case TestSignalType::ImpulseBurst: generateImpulseBurst(buffer, spec); break;
        case TestSignalType::MultiSine: generateMultiSine(buffer, spec); break;
    }

    return buffer;
}

juce::String TestSignalGenerator::toString(TestSignalType type)
{
    switch (type)
    {
        case TestSignalType::LogSineSweep: return "LogSineSweep";
        case TestSignalType::WhiteNoise: return "WhiteNoise";
        case TestSignalType::PinkNoise: return "PinkNoise";
        case TestSignalType::ImpulseBurst: return "ImpulseBurst";
        case TestSignalType::MultiSine: return "MultiSine";
    }

    return "Unknown";
}

void TestSignalGenerator::generateLogSineSweep(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec)
{
    constexpr auto startHz = 30.0;
    constexpr auto endHz = 20000.0;
    const auto duration = juce::jmax(0.001, spec.durationSeconds);
    const auto logRatio = std::log(endHz / startHz);
    auto phase = 0.0;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto t = static_cast<double>(i) / spec.sampleRate;
        const auto instantHz = startHz * std::exp(logRatio * t / duration);
        phase += juce::MathConstants<double>::twoPi * instantHz / spec.sampleRate;
        buffer.setSample(0, i, spec.amplitude * std::sin(static_cast<float>(phase)));
    }
}

void TestSignalGenerator::generateWhiteNoise(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec)
{
    juce::Random random(0x48414e53);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        buffer.setSample(0, i, spec.amplitude * random.nextFloat() * 2.0f - spec.amplitude);
}

void TestSignalGenerator::generatePinkNoise(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec)
{
    juce::Random random(0x4c4142);
    float b0 = 0.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto white = random.nextFloat() * 2.0f - 1.0f;
        b0 = 0.99765f * b0 + white * 0.0990460f;
        b1 = 0.96300f * b1 + white * 0.2965164f;
        b2 = 0.57000f * b2 + white * 1.0526913f;
        const auto pink = (b0 + b1 + b2 + white * 0.1848f) * 0.18f;
        buffer.setSample(0, i, spec.amplitude * pink);
    }
}

void TestSignalGenerator::generateImpulseBurst(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec)
{
    const auto spacing = juce::jmax(1, static_cast<int>(0.25 * spec.sampleRate));
    const auto burstSamples = juce::jmax(1, static_cast<int>(0.0025 * spec.sampleRate));

    for (int start = 0; start < buffer.getNumSamples(); start += spacing)
    {
        for (int i = 0; i < burstSamples && start + i < buffer.getNumSamples(); ++i)
        {
            const auto envelope = 1.0f - static_cast<float>(i) / static_cast<float>(burstSamples);
            buffer.setSample(0, start + i, spec.amplitude * envelope);
        }
    }
}

void TestSignalGenerator::generateMultiSine(juce::AudioBuffer<float>& buffer, const TestSignalSpec& spec)
{
    constexpr double frequencies[] = { 82.41, 110.0, 146.83, 196.0, 246.94, 329.63, 440.0, 880.0, 1760.0, 3520.0 };
    constexpr auto numFrequencies = static_cast<int>(sizeof(frequencies) / sizeof(frequencies[0]));
    double phases[numFrequencies] {};

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto value = 0.0;
        for (int i = 0; i < numFrequencies; ++i)
        {
            value += std::sin(phases[i]);
            phases[i] += juce::MathConstants<double>::twoPi * frequencies[i] / spec.sampleRate;
        }

        buffer.setSample(0, sample, static_cast<float>(value / numFrequencies) * spec.amplitude);
    }
}
}

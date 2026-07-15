#include "Capture/TestSignalGenerator.h"

namespace hanso
{
namespace
{
constexpr double fullProbeDurationSeconds = 24.5;
constexpr double deltaProbeDurationSeconds = 10.0;
constexpr double cabinetProbeTotalDurationSeconds = 8.0;
constexpr double cabinetSweepDurationSeconds = 6.0;

int toSamples(double seconds, double sampleRate) noexcept
{
    return juce::jmax(1, static_cast<int>(std::round(seconds * sampleRate)));
}

void normalizeRange(juce::AudioBuffer<float>& buffer, int start, int numSamples, float targetPeak)
{
    if (numSamples <= 0 || targetPeak <= 0.0f)
        return;

    const auto peak = buffer.getMagnitude(0, start, numSamples);
    if (peak > 1.0e-9f)
        buffer.applyGain(0, start, numSamples, targetPeak / peak);
}

void fadeRange(juce::AudioBuffer<float>& buffer,
               int start,
               int numSamples,
               double sampleRate,
               double fadeSeconds = 0.01)
{
    const auto fadeSamples = juce::jmin(numSamples / 4, toSamples(fadeSeconds, sampleRate));
    if (fadeSamples <= 1)
        return;

    buffer.applyGainRamp(0, start, fadeSamples, 0.0f, 1.0f);
    buffer.applyGainRamp(0, start + numSamples - fadeSamples, fadeSamples, 1.0f, 0.0f);
}

juce::AudioBuffer<float> makeOddGridMultisine(double sampleRate, double durationSeconds)
{
    struct ToneState
    {
        double sine { 0.0 };
        double cosine { 1.0 };
        double sineStep { 0.0 };
        double cosineStep { 1.0 };
        double weight { 1.0 };
    };

    std::vector<double> frequencies;
    const auto upperHz = juce::jmin(12000.0, sampleRate * 0.45);
    auto lineIndex = 0;
    for (auto oddHarmonic = 3; oddHarmonic * 20.0 <= upperHz; oddHarmonic += 2, ++lineIndex)
    {
        // One unexcited odd line in every four acts as a nonlinear detection bin.
        if (lineIndex % 4 != 2)
            frequencies.push_back(oddHarmonic * 20.0);
    }

    std::vector<ToneState> tones;
    tones.reserve(frequencies.size());
    for (size_t i = 0; i < frequencies.size(); ++i)
    {
        const auto frequency = frequencies[i];
        const auto phase = -juce::MathConstants<double>::pi
                         * static_cast<double>(i) * static_cast<double>(i + 1)
                         / static_cast<double>(frequencies.size());
        const auto step = juce::MathConstants<double>::twoPi * frequency / sampleRate;
        tones.push_back({ std::sin(phase), std::cos(phase), std::sin(step), std::cos(step),
                          std::sqrt(80.0 / frequency) });
    }

    juce::AudioBuffer<float> period(1, toSamples(durationSeconds, sampleRate));
    period.clear();
    for (int sample = 0; sample < period.getNumSamples(); ++sample)
    {
        auto value = 0.0;
        for (auto& tone : tones)
        {
            value += tone.sine * tone.weight;
            const auto nextSine = tone.sine * tone.cosineStep + tone.cosine * tone.sineStep;
            tone.cosine = tone.cosine * tone.cosineStep - tone.sine * tone.sineStep;
            tone.sine = nextSine;
        }
        period.setSample(0, sample, static_cast<float>(value));
    }

    normalizeRange(period, 0, period.getNumSamples(), 1.0f);
    return period;
}

void addPluck(juce::AudioBuffer<float>& destination,
              int startSample,
              double sampleRate,
              double frequency,
              double durationSeconds,
              float relativePeak,
              bool palmMuted,
              uint32_t seed)
{
    const auto available = destination.getNumSamples() - startSample;
    const auto numSamples = juce::jmin(available, toSamples(durationSeconds, sampleRate));
    if (startSample < 0 || numSamples <= 0)
        return;

    juce::AudioBuffer<float> pluck(1, numSamples);
    pluck.clear();
    auto randomState = seed | 1u;
    const auto decaySeconds = palmMuted ? 0.075 : 0.42;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto t = static_cast<double>(sample) / sampleRate;
        const auto attack = 1.0 - std::exp(-t / 0.0015);
        const auto envelope = attack * std::exp(-t / decaySeconds);
        auto harmonicSignal = 0.0;
        for (auto harmonic = 1; harmonic <= 14; ++harmonic)
        {
            const auto harmonicHz = frequency * harmonic;
            if (harmonicHz >= sampleRate * 0.45)
                break;
            const auto harmonicDecay = std::exp(-t * 0.7 * static_cast<double>(harmonic - 1));
            const auto phase = 0.37 * static_cast<double>(harmonic * harmonic);
            harmonicSignal += std::sin(juce::MathConstants<double>::twoPi * harmonicHz * t + phase)
                            * harmonicDecay / std::pow(static_cast<double>(harmonic), 1.12);
        }

        randomState = randomState * 1664525u + 1013904223u;
        const auto noise = (static_cast<double>((randomState >> 8) & 0x00ffffffu) / 8388607.5 - 1.0)
                         * std::exp(-t / 0.012) * 0.18;
        pluck.setSample(0, sample, static_cast<float>(envelope * (harmonicSignal + noise)));
    }

    normalizeRange(pluck, 0, pluck.getNumSamples(), relativePeak);
    destination.addFrom(0, startSample, pluck, 0, 0, pluck.getNumSamples());
}

juce::AudioBuffer<float> makeReferenceSignal(double sampleRate)
{
    juce::AudioBuffer<float> reference(1, toSamples(2.0, sampleRate));
    reference.clear();
    addPluck(reference, toSamples(0.04, sampleRate), sampleRate, 82.4069, 0.55, 0.75f, false, 0x48414e31u);
    addPluck(reference, toSamples(0.48, sampleRate), sampleRate, 110.0, 0.55, 0.55f, true, 0x48414e32u);
    addPluck(reference, toSamples(0.90, sampleRate), sampleRate, 146.832, 0.62, 0.72f, false, 0x48414e33u);
    const auto chordStart = toSamples(1.38, sampleRate);
    addPluck(reference, chordStart, sampleRate, 82.4069, 0.58, 0.62f, false, 0x48414e34u);
    addPluck(reference, chordStart, sampleRate, 123.471, 0.58, 0.48f, false, 0x48414e35u);
    addPluck(reference, chordStart, sampleRate, 164.814, 0.58, 0.42f, false, 0x48414e36u);
    normalizeRange(reference, 0, reference.getNumSamples(), 1.0f);
    fadeRange(reference, 0, reference.getNumSamples(), sampleRate, 0.005);
    return reference;
}

void writeSync(juce::AudioBuffer<float>& buffer,
               int start,
               int numSamples,
               double sampleRate,
               float targetPeak)
{
    constexpr auto chips = 31;
    const auto chipSamples = toSamples(0.012, sampleRate);
    const auto sequenceSamples = chips * chipSamples;
    auto cursor = start + juce::jmax(0, (numSamples - sequenceSamples) / 2);
    uint32_t lfsr = 0x1fu;
    constexpr double carriers[] { 731.0, 1171.0, 1871.0, 2879.0 };

    for (auto chip = 0; chip < chips; ++chip)
    {
        const auto feedback = ((lfsr >> 4u) ^ (lfsr >> 2u)) & 1u;
        lfsr = ((lfsr << 1u) | feedback) & 0x1fu;
        const auto polarity = (lfsr & 1u) != 0u ? 1.0 : -1.0;
        const auto frequency = carriers[chip % 4];
        for (int sample = 0; sample < chipSamples && cursor + sample < start + numSamples; ++sample)
        {
            const auto x = static_cast<double>(sample) / static_cast<double>(chipSamples - 1);
            const auto window = 0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi * x);
            const auto phase = juce::MathConstants<double>::twoPi * frequency
                             * static_cast<double>(sample) / sampleRate;
            buffer.setSample(0, cursor + sample, static_cast<float>(polarity * window * std::sin(phase)));
        }
        cursor += chipSamples;
    }
    normalizeRange(buffer, start, numSamples, targetPeak);
}

void writeLevelLadder(juce::AudioBuffer<float>& buffer,
                      int start,
                      int numSamples,
                      double sampleRate,
                      float ceiling)
{
    constexpr float levelsDb[] { -24.0f, -18.0f, -12.0f, -8.0f, -4.0f, 0.0f };
    const auto stepSamples = numSamples / 6;
    auto period = makeOddGridMultisine(sampleRate, static_cast<double>(stepSamples) / sampleRate);
    for (auto step = 0; step < 6; ++step)
    {
        const auto stepStart = start + step * stepSamples;
        const auto length = step == 5 ? start + numSamples - stepStart : stepSamples;
        const auto copyLength = juce::jmin(length, period.getNumSamples());
        const auto stepGain = ceiling * juce::Decibels::decibelsToGain(levelsDb[step]);
        buffer.copyFrom(0, stepStart, period.getReadPointer(0), copyLength, stepGain);
        if (copyLength < length)
            buffer.copyFrom(0, stepStart + copyLength, period.getReadPointer(0), length - copyLength, stepGain);
        fadeRange(buffer, stepStart, length, sampleRate, 0.008);
    }
}

void writeSynchronizedSweep(juce::AudioBuffer<float>& buffer,
                            int start,
                            int slotSamples,
                            double sampleRate,
                            float targetPeak)
{
    const auto startHz = 35.0;
    const auto endHz = juce::jmin(12000.0, sampleRate * 0.45);
    const auto ratio = endHz / startHz;
    const auto logRatio = std::log(ratio);
    const auto requestedDuration = static_cast<double>(slotSamples) / sampleRate;
    const auto integerCycles = std::floor(startHz * requestedDuration * (ratio - 1.0) / logRatio);
    const auto synchronizedDuration = integerCycles * logRatio / (startHz * (ratio - 1.0));
    const auto sweepSamples = juce::jmin(slotSamples, toSamples(synchronizedDuration, sampleRate));

    for (int sample = 0; sample < sweepSamples; ++sample)
    {
        const auto t = static_cast<double>(sample) / sampleRate;
        const auto phase = juce::MathConstants<double>::twoPi * startHz * synchronizedDuration / logRatio
                         * (std::exp(t * logRatio / synchronizedDuration) - 1.0);
        buffer.setSample(0, start + sample, static_cast<float>(std::sin(phase)));
    }
    normalizeRange(buffer, start, sweepSamples, targetPeak);
    fadeRange(buffer, start, sweepSamples, sampleRate);
}

void writeDualSweep(juce::AudioBuffer<float>& buffer,
                    int start,
                    int numSamples,
                    double sampleRate,
                    float ceiling)
{
    const auto sweepSamples = toSamples(2.0, sampleRate);
    const auto gapSamples = toSamples(0.25, sampleRate);
    writeSynchronizedSweep(buffer, start, sweepSamples, sampleRate,
                           ceiling * juce::Decibels::decibelsToGain(-12.0f));
    const auto secondStart = start + sweepSamples + gapSamples;
    writeSynchronizedSweep(buffer, secondStart,
                           juce::jmin(sweepSamples, start + numSamples - secondStart),
                           sampleRate, ceiling);
}

void writeOddGridPeriods(juce::AudioBuffer<float>& buffer,
                         int start,
                         int numSamples,
                         double sampleRate,
                         float ceiling)
{
    const auto periodSamples = toSamples(1.0, sampleRate);
    const auto periodCount = juce::jmax(1, numSamples / periodSamples);
    const auto lowPeriods = periodCount >= 4 ? 2 : 1;
    const auto lowGain = juce::Decibels::decibelsToGain(-12.0f);
    auto period = makeOddGridMultisine(sampleRate, 1.0);

    for (auto periodIndex = 0; periodIndex < periodCount; ++periodIndex)
    {
        const auto periodStart = start + periodIndex * periodSamples;
        const auto length = juce::jmin(periodSamples, start + numSamples - periodStart);
        if (length <= 0)
            break;
        buffer.copyFrom(0, periodStart, period.getReadPointer(0), length,
                        ceiling * (periodIndex < lowPeriods ? lowGain : 1.0f));
    }

    const auto transitionStart = start + lowPeriods * periodSamples;
    const auto rampSamples = toSamples(0.02, sampleRate);
    if (transitionStart < start + numSamples)
        buffer.applyGainRamp(0, transitionStart, juce::jmin(rampSamples, start + numSamples - transitionStart),
                             lowGain, 1.0f);
    fadeRange(buffer, start, numSamples, sampleRate);
}

void writeTransientBank(juce::AudioBuffer<float>& buffer,
                        int start,
                        int numSamples,
                        double sampleRate,
                        float ceiling)
{
    juce::AudioBuffer<float> bank(1, numSamples);
    bank.clear();
    constexpr double lowNotes[] { 82.4069, 110.0, 73.4162, 98.0 };
    for (auto note = 0; note < 8; ++note)
    {
        const auto noteStart = toSamples(0.08 + 0.26 * note, sampleRate);
        const auto level = note % 2 == 0 ? 0.92f : 0.32f;
        addPluck(bank, noteStart, sampleRate, lowNotes[note % 4], 0.22, level, true,
                 0x54524100u + static_cast<uint32_t>(note));
    }

    const auto sustainStart = toSamples(2.35, sampleRate);
    addPluck(bank, sustainStart, sampleRate, 82.4069, 1.25, 0.78f, false, 0x54524201u);
    addPluck(bank, sustainStart, sampleRate, 123.471, 1.25, 0.58f, false, 0x54524202u);
    addPluck(bank, sustainStart, sampleRate, 164.814, 1.25, 0.48f, false, 0x54524203u);

    addPluck(bank, toSamples(4.12, sampleRate), sampleRate, 146.832, 0.45, 0.18f, false, 0x54524301u);
    addPluck(bank, toSamples(4.58, sampleRate), sampleRate, 196.0, 0.36, 0.10f, true, 0x54524302u);
    normalizeRange(bank, 0, bank.getNumSamples(), ceiling);
    fadeRange(bank, 0, bank.getNumSamples(), sampleRate, 0.005);
    buffer.copyFrom(0, start, bank, 0, 0, numSamples);
}

void writeGuitarProgram(juce::AudioBuffer<float>& buffer,
                        int start,
                        int numSamples,
                        double sampleRate,
                        float ceiling)
{
    juce::AudioBuffer<float> program(1, numSamples);
    program.clear();
    constexpr double melody[] { 82.4069, 123.471, 146.832, 164.814, 220.0, 196.0, 146.832, 110.0 };
    for (auto note = 0; note < 8; ++note)
        addPluck(program, toSamples(0.03 + 0.235 * note, sampleRate), sampleRate,
                 melody[note], 0.34, 0.42f + 0.06f * static_cast<float>(note % 3), note % 3 == 0,
                 0x50524700u + static_cast<uint32_t>(note));
    normalizeRange(program, 0, program.getNumSamples(), ceiling * 0.9f);
    fadeRange(program, 0, program.getNumSamples(), sampleRate, 0.005);
    buffer.copyFrom(0, start, program, 0, 0, numSamples);
}
}

juce::AudioBuffer<float> TestSignalGenerator::generate(const TestSignalSpec& spec) const
{
    const auto duration = spec.type == TestSignalType::HansoProbeA1
                        ? hansoProbeDurationSeconds(spec.probeVariant)
                        : spec.type == TestSignalType::CabinetProbeC1
                        ? cabinetProbeTotalDurationSeconds
                        : spec.durationSeconds;
    const auto numSamples = toSamples(duration, spec.sampleRate);
    juce::AudioBuffer<float> buffer(1, numSamples);
    buffer.clear();

    switch (spec.type)
    {
        case TestSignalType::LogSineSweep: generateLogSineSweep(buffer, spec); break;
        case TestSignalType::WhiteNoise: generateWhiteNoise(buffer, spec); break;
        case TestSignalType::PinkNoise: generatePinkNoise(buffer, spec); break;
        case TestSignalType::ImpulseBurst: generateImpulseBurst(buffer, spec); break;
        case TestSignalType::MultiSine: generateMultiSine(buffer, spec); break;
        case TestSignalType::HansoProbeA1: generateHansoProbeA1(buffer, spec); break;
        case TestSignalType::CabinetProbeC1: generateCabinetProbeC1(buffer, spec); break;
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
        case TestSignalType::HansoProbeA1: return "HansoProbeA1";
        case TestSignalType::CabinetProbeC1: return "CabinetProbeC1";
    }

    return "Unknown";
}

juce::String TestSignalGenerator::toString(const TestSignalSpec& spec)
{
    if (spec.type != TestSignalType::HansoProbeA1)
        return toString(spec.type);

    return spec.probeVariant == HansoProbeVariant::Full ? "HansoProbeA1Full"
                                                       : "HansoProbeA1Delta";
}

double TestSignalGenerator::hansoProbeDurationSeconds(HansoProbeVariant variant) noexcept
{
    return variant == HansoProbeVariant::Full ? fullProbeDurationSeconds : deltaProbeDurationSeconds;
}

double TestSignalGenerator::cabinetProbeDurationSeconds() noexcept
{
    return cabinetProbeTotalDurationSeconds;
}

std::vector<HansoProbeSegment> TestSignalGenerator::hansoProbeSegments(HansoProbeVariant variant,
                                                                       double sampleRate)
{
    std::vector<HansoProbeSegment> segments;
    auto append = [&segments, sampleRate](const char* id,
                                          HansoProbeSegmentPurpose purpose,
                                          double startSeconds,
                                          double endSeconds)
    {
        const auto start = static_cast<int>(std::round(startSeconds * sampleRate));
        const auto end = static_cast<int>(std::round(endSeconds * sampleRate));
        segments.push_back({ id, purpose, start, juce::jmax(0, end - start) });
    };

    append("noise-floor", HansoProbeSegmentPurpose::Silence, 0.0, 0.5);
    append("coded-sync", HansoProbeSegmentPurpose::Sync, 0.5, 1.0);
    if (variant == HansoProbeVariant::Full)
    {
        append("reference-a-start", HansoProbeSegmentPurpose::Validation, 1.0, 3.0);
        append("level-ladder", HansoProbeSegmentPurpose::Analysis, 3.0, 6.0);
        append("dual-level-ess", HansoProbeSegmentPurpose::Analysis, 6.0, 10.5);
        append("odd-grid-multisine", HansoProbeSegmentPurpose::Analysis, 10.5, 14.5);
        append("transient-memory", HansoProbeSegmentPurpose::Analysis, 14.5, 19.5);
        append("guitar-program", HansoProbeSegmentPurpose::Training, 19.5, 21.5);
        append("reference-a-end", HansoProbeSegmentPurpose::Validation, 21.5, 23.5);
        append("tail-silence", HansoProbeSegmentPurpose::Silence, 23.5, 24.5);
    }
    else
    {
        append("level-ladder", HansoProbeSegmentPurpose::Analysis, 1.0, 4.0);
        append("odd-grid-multisine", HansoProbeSegmentPurpose::Analysis, 4.0, 7.0);
        append("reference-a", HansoProbeSegmentPurpose::Validation, 7.0, 9.0);
        append("tail-silence", HansoProbeSegmentPurpose::Silence, 9.0, 10.0);
    }
    return segments;
}

std::array<double, 10> TestSignalGenerator::multiSineFrequencies() noexcept
{
    // Integer Hz so each tone completes a whole number of cycles in the 1-second
    // probe buffer, letting it loop seamlessly (non-integer guitar-note values
    // left a non-zero sample at the wrap point, producing a click every second).
    // Generation and Goertzel detection share this array, so detection is exact.
    return { 82.0, 110.0, 147.0, 196.0, 247.0, 330.0, 440.0, 880.0, 1760.0, 3520.0 };
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

    // Short raised-cosine fades keep the sweep from starting and ending with a
    // click, which would otherwise leak wideband energy into the analysis.
    const auto fadeSamples = juce::jmin(buffer.getNumSamples() / 4,
                                        static_cast<int>(0.01 * spec.sampleRate));
    if (fadeSamples > 1)
    {
        buffer.applyGainRamp(0, 0, fadeSamples, 0.0f, 1.0f);
        buffer.applyGainRamp(0, buffer.getNumSamples() - fadeSamples, fadeSamples, 1.0f, 0.0f);
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
    const auto frequencies = multiSineFrequencies();
    constexpr auto numFrequencies = 10;
    double phases[numFrequencies] {};

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto value = 0.0;
        for (int i = 0; i < numFrequencies; ++i)
        {
            value += std::sin(phases[i]);
            phases[i] += juce::MathConstants<double>::twoPi * frequencies[static_cast<size_t>(i)] / spec.sampleRate;
        }

        buffer.setSample(0, sample, static_cast<float>(value / numFrequencies) * spec.amplitude);
    }
}

void TestSignalGenerator::generateHansoProbeA1(juce::AudioBuffer<float>& buffer,
                                                const TestSignalSpec& spec)
{
    const auto ceiling = juce::jlimit(0.0f, 1.0f, spec.amplitude);
    const auto segments = hansoProbeSegments(spec.probeVariant, spec.sampleRate);
    auto reference = makeReferenceSignal(spec.sampleRate);

    for (const auto& segment : segments)
    {
        if (segment.id == "coded-sync")
        {
            writeSync(buffer, segment.startSample, segment.numSamples, spec.sampleRate, ceiling * 0.7f);
        }
        else if (segment.id == "level-ladder")
        {
            writeLevelLadder(buffer, segment.startSample, segment.numSamples, spec.sampleRate, ceiling);
        }
        else if (segment.id == "dual-level-ess")
        {
            writeDualSweep(buffer, segment.startSample, segment.numSamples, spec.sampleRate, ceiling);
        }
        else if (segment.id == "odd-grid-multisine")
        {
            writeOddGridPeriods(buffer, segment.startSample, segment.numSamples, spec.sampleRate, ceiling);
        }
        else if (segment.id == "transient-memory")
        {
            writeTransientBank(buffer, segment.startSample, segment.numSamples, spec.sampleRate, ceiling);
        }
        else if (segment.id == "guitar-program")
        {
            writeGuitarProgram(buffer, segment.startSample, segment.numSamples, spec.sampleRate, ceiling);
        }
        else if (segment.purpose == HansoProbeSegmentPurpose::Validation)
        {
            const auto copyLength = juce::jmin(segment.numSamples, reference.getNumSamples());
            buffer.copyFrom(0, segment.startSample, reference.getReadPointer(0), copyLength, ceiling * 0.85f);
        }
    }

    // Every segment is individually peak-limited, but this final guard keeps
    // the public amplitude contract true if segment boundaries change later.
    const auto peak = buffer.getMagnitude(0, 0, buffer.getNumSamples());
    if (peak > ceiling && peak > 0.0f)
        buffer.applyGain(ceiling / peak);
}

void TestSignalGenerator::generateCabinetProbeC1(juce::AudioBuffer<float>& buffer,
                                                  const TestSignalSpec& spec)
{
    const auto sweepSamples = juce::jmin(buffer.getNumSamples(),
                                         toSamples(cabinetSweepDurationSeconds, spec.sampleRate));
    writeSynchronizedSweep(buffer, 0, sweepSamples, spec.sampleRate,
                           juce::jlimit(0.0f, 1.0f, spec.amplitude));
    // The remaining two seconds stay silent so the cabinet and room tail are
    // captured before regularized deconvolution.
}
}

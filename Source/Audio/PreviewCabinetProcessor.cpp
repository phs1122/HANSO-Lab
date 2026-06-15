#include "Audio/PreviewCabinetProcessor.h"

namespace hanso
{
namespace
{
constexpr float kAirBaseFreqHz = 12000.0f;
constexpr float kPositionHighCutHz = 8500.0f;
}

float PreviewCabinetProcessor::BiquadState::process(float input, const BiquadCoefficients& coeffs) noexcept
{
    const auto output = coeffs.b0 * input + z1;
    z1 = coeffs.b1 * input - coeffs.a1 * output + z2;
    z2 = coeffs.b2 * input - coeffs.a2 * output;
    return output;
}

void PreviewCabinetProcessor::BiquadState::reset() noexcept
{
    z1 = 0.0f;
    z2 = 0.0f;
}

void PreviewCabinetProcessor::EnvelopeFollower::prepare(double sr) noexcept
{
    const auto sampleRate = sr > 0.0 ? sr : 48000.0;
    attackCoeff = 1.0f - static_cast<float>(std::exp(-1.0 / (0.003 * sampleRate)));
    releaseCoeff = 1.0f - static_cast<float>(std::exp(-1.0 / (0.080 * sampleRate)));
}

void PreviewCabinetProcessor::EnvelopeFollower::reset() noexcept
{
    envelope = 0.0f;
}

float PreviewCabinetProcessor::EnvelopeFollower::process(float input) noexcept
{
    const auto target = std::abs(input);
    const auto coeff = target > envelope ? attackCoeff : releaseCoeff;
    envelope += (target - envelope) * coeff;
    return envelope;
}

void PreviewCabinetProcessor::prepare(double newSampleRate, int maximumBlockSize, int outputChannels) noexcept
{
    juce::ignoreUnused(maximumBlockSize);
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    preparedChannels = juce::jlimit(1, maxChannels, outputChannels);

    for (auto& compressor : compressors)
        compressor.prepare(sampleRate);

    updateCoefficients();
    reset();
}

void PreviewCabinetProcessor::reset() noexcept
{
    for (auto& channelFilters : filters)
    {
        channelFilters.hpf.reset();
        channelFilters.lpf.reset();
        channelFilters.lowRes.reset();
        channelFilters.mid.reset();
        channelFilters.presence.reset();
        channelFilters.air.reset();
        channelFilters.micPresence.reset();
        channelFilters.micLowMid.reset();
        channelFilters.micHighRolloff.reset();
        channelFilters.micAir.reset();
        channelFilters.positionLowShelf.reset();
        channelFilters.positionMidPeak.reset();
        channelFilters.positionHighShelf.reset();
        channelFilters.positionHighCut.reset();
    }

    for (auto& compressor : compressors)
        compressor.reset();

    compGain = { 1.0f, 1.0f };
}

float PreviewCabinetProcessor::processSample(float input, ChannelFilters& channelFilters) noexcept
{
    auto value = channelFilters.hpf.process(input, coefficients[0]);
    value = channelFilters.lpf.process(value, coefficients[1]);
    value = channelFilters.lowRes.process(value, coefficients[2]);
    value = channelFilters.mid.process(value, coefficients[3]);
    value = channelFilters.presence.process(value, coefficients[4]);
    value = channelFilters.air.process(value, coefficients[5]);
    return value;
}

void PreviewCabinetProcessor::process(juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    const auto channelsToProcess = juce::jmin(buffer.getNumChannels(), preparedChannels);
    for (int channel = 0; channel < channelsToProcess; ++channel)
    {
        auto* samples = buffer.getWritePointer(channel);
        auto& channelFilters = filters[static_cast<size_t>(channel)];
        auto& compressor = compressors[static_cast<size_t>(channel)];
        auto& channelCompGain = compGain[static_cast<size_t>(channel)];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            auto value = processSample(samples[sample], channelFilters);

            const auto env = compressor.process(value);
            const auto targetGain = 1.0f - compressionAmount * std::tanh(env * 3.5f);
            channelCompGain = 0.90f * channelCompGain + 0.10f * targetGain;
            value *= channelCompGain;

            value = channelFilters.micPresence.process(value, coefficients[6]);
            value = channelFilters.micLowMid.process(value, coefficients[7]);
            value = channelFilters.micHighRolloff.process(value, coefficients[8]);
            value = channelFilters.micAir.process(value, coefficients[9]);

            value = channelFilters.positionLowShelf.process(value, coefficients[10]);
            value = channelFilters.positionMidPeak.process(value, coefficients[11]);
            value = channelFilters.positionHighShelf.process(value, coefficients[12]);
            value = channelFilters.positionHighCut.process(value, coefficients[13]);
            samples[sample] = value * positionGainLinear;
        }
    }
}

void PreviewCabinetProcessor::updateCoefficients() noexcept
{
    const auto sr = sampleRate;
  // American Combo + Dynamic mic + Cap Edge (Guitar Plugins CabinetProfiles.h)
    coefficients[0] = makeHighPass(sr, 80.0, 0.707f);
    coefficients[1] = makeLowPass(sr, 5200.0, 0.707f);
    coefficients[2] = makePeak(sr, 110.0, 0.7f, 2.0f);
    coefficients[3] = makePeak(sr, 800.0, 0.9f, -2.5f);
    coefficients[4] = makePeak(sr, 3500.0, 0.8f, 3.0f);
    coefficients[5] = makePeak(sr, kAirBaseFreqHz, 0.6f, 0.35f * 6.0f);
    coefficients[6] = makePeak(sr, 4200.0, 0.9f, 3.0f);
    coefficients[7] = makePeak(sr, 500.0, 0.8f, 0.0f);
    coefficients[8] = makePeak(sr, 8000.0, 0.7f, -5.0f);
    coefficients[9] = makePeak(sr, 10000.0, 0.6f, 0.0f);
    coefficients[10] = makeLowShelf(sr, 130.0, 0.7f, 1.5f);
    coefficients[11] = makePeak(sr, 2000.0, 1.0f, 1.0f);
    coefficients[12] = makeHighShelf(sr, 6000.0, 0.8f, 2.0f);
    coefficients[13] = makeLowPass(sr, kPositionHighCutHz, 0.707f);
}

PreviewCabinetProcessor::BiquadCoefficients PreviewCabinetProcessor::makeHighPass(double sr,
                                                                                    double frequencyHz,
                                                                                    float q) noexcept
{
    const auto w0 = juce::MathConstants<double>::twoPi * frequencyHz / sr;
    const auto cosW0 = static_cast<float>(std::cos(w0));
    const auto sinW0 = static_cast<float>(std::sin(w0));
    const auto alpha = sinW0 / (2.0f * juce::jmax(0.1f, q));
    const auto b0 = (1.0f + cosW0) * 0.5f;
    const auto b1 = -(1.0f + cosW0);
    const auto b2 = (1.0f + cosW0) * 0.5f;
    const auto a0 = 1.0f + alpha;
    const auto a1 = -2.0f * cosW0;
    const auto a2 = 1.0f - alpha;
    return normalise(b0, b1, b2, a0, a1, a2);
}

PreviewCabinetProcessor::BiquadCoefficients PreviewCabinetProcessor::makeLowPass(double sr,
                                                                                   double frequencyHz,
                                                                                   float q) noexcept
{
    const auto w0 = juce::MathConstants<double>::twoPi * frequencyHz / sr;
    const auto cosW0 = static_cast<float>(std::cos(w0));
    const auto sinW0 = static_cast<float>(std::sin(w0));
    const auto alpha = sinW0 / (2.0f * juce::jmax(0.1f, q));
    const auto b0 = (1.0f - cosW0) * 0.5f;
    const auto b1 = 1.0f - cosW0;
    const auto b2 = (1.0f - cosW0) * 0.5f;
    const auto a0 = 1.0f + alpha;
    const auto a1 = -2.0f * cosW0;
    const auto a2 = 1.0f - alpha;
    return normalise(b0, b1, b2, a0, a1, a2);
}

PreviewCabinetProcessor::BiquadCoefficients PreviewCabinetProcessor::makeLowShelf(double sr,
                                                                                    double frequencyHz,
                                                                                    float q,
                                                                                    float gainDb) noexcept
{
    const auto a = std::sqrt(juce::Decibels::decibelsToGain(gainDb));
    const auto w0 = juce::MathConstants<double>::twoPi * frequencyHz / sr;
    const auto cosW0 = static_cast<float>(std::cos(w0));
    const auto sinW0 = static_cast<float>(std::sin(w0));
    const auto sqrtA = std::sqrt(a);
    const auto alpha = sinW0 / (2.0f * juce::jmax(0.1f, q));
    const auto b0 = a * ((a + 1.0f) - (a - 1.0f) * cosW0 + 2.0f * sqrtA * alpha);
    const auto b1 = 2.0f * a * ((a - 1.0f) - (a + 1.0f) * cosW0);
    const auto b2 = a * ((a + 1.0f) - (a - 1.0f) * cosW0 - 2.0f * sqrtA * alpha);
    const auto a0 = (a + 1.0f) + (a - 1.0f) * cosW0 + 2.0f * sqrtA * alpha;
    const auto a1 = -2.0f * ((a - 1.0f) + (a + 1.0f) * cosW0);
    const auto a2 = (a + 1.0f) + (a - 1.0f) * cosW0 - 2.0f * sqrtA * alpha;
    return normalise(b0, b1, b2, a0, a1, a2);
}

PreviewCabinetProcessor::BiquadCoefficients PreviewCabinetProcessor::makeHighShelf(double sr,
                                                                                     double frequencyHz,
                                                                                     float q,
                                                                                     float gainDb) noexcept
{
    const auto a = std::sqrt(juce::Decibels::decibelsToGain(gainDb));
    const auto w0 = juce::MathConstants<double>::twoPi * frequencyHz / sr;
    const auto cosW0 = static_cast<float>(std::cos(w0));
    const auto sinW0 = static_cast<float>(std::sin(w0));
    const auto sqrtA = std::sqrt(a);
    const auto alpha = sinW0 / (2.0f * juce::jmax(0.1f, q));
    const auto b0 = a * ((a + 1.0f) + (a - 1.0f) * cosW0 + 2.0f * sqrtA * alpha);
    const auto b1 = -2.0f * a * ((a - 1.0f) + (a + 1.0f) * cosW0);
    const auto b2 = a * ((a + 1.0f) + (a - 1.0f) * cosW0 - 2.0f * sqrtA * alpha);
    const auto a0 = (a + 1.0f) - (a - 1.0f) * cosW0 + 2.0f * sqrtA * alpha;
    const auto a1 = 2.0f * ((a - 1.0f) - (a + 1.0f) * cosW0);
    const auto a2 = (a + 1.0f) - (a - 1.0f) * cosW0 - 2.0f * sqrtA * alpha;
    return normalise(b0, b1, b2, a0, a1, a2);
}

PreviewCabinetProcessor::BiquadCoefficients PreviewCabinetProcessor::makePeak(double sr,
                                                                                double frequencyHz,
                                                                                float q,
                                                                                float gainDb) noexcept
{
    const auto a = std::sqrt(juce::Decibels::decibelsToGain(gainDb));
    const auto w0 = juce::MathConstants<double>::twoPi * frequencyHz / sr;
    const auto alpha = static_cast<float>(std::sin(w0)) / (2.0f * juce::jmax(0.1f, q));
    const auto cosW0 = static_cast<float>(std::cos(w0));
    const auto b0 = 1.0f + alpha * a;
    const auto b1 = -2.0f * cosW0;
    const auto b2 = 1.0f - alpha * a;
    const auto a0 = 1.0f + alpha / a;
    const auto a1 = -2.0f * cosW0;
    const auto a2 = 1.0f - alpha / a;
    return normalise(b0, b1, b2, a0, a1, a2);
}

PreviewCabinetProcessor::BiquadCoefficients PreviewCabinetProcessor::normalise(float b0,
                                                                               float b1,
                                                                               float b2,
                                                                               float a0,
                                                                               float a1,
                                                                               float a2) noexcept
{
    BiquadCoefficients coeffs;
    if (std::abs(a0) < 1.0e-8f)
        return coeffs;

    coeffs.b0 = b0 / a0;
    coeffs.b1 = b1 / a0;
    coeffs.b2 = b2 / a0;
    coeffs.a1 = a1 / a0;
    coeffs.a2 = a2 / a0;
    return coeffs;
}
}

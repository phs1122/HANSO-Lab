#include "Analysis/CabinetToneProfiler.h"

namespace hanso
{
namespace
{
// Impulse responses are transient signals: a single zero-padded FFT without a
// window preserves their spectrum exactly, whereas Hann-windowed frame
// averaging would suppress energy near the IR onset.
constexpr int irFftOrder = 15;
constexpr int irFftSize = 1 << irFftOrder;
}

CabinetToneProfile CabinetToneProfiler::fromImpulseResponse(const juce::AudioBuffer<float>& impulseResponse,
                                                            double sampleRate)
{
    CabinetToneProfile profile;
    if (impulseResponse.getNumChannels() == 0 || impulseResponse.getNumSamples() <= 8 || sampleRate <= 0.0)
        return profile;

    juce::dsp::FFT fft(irFftOrder);
    juce::HeapBlock<float> fftData(static_cast<size_t>(irFftSize * 2));
    juce::zeromem(fftData.getData(), sizeof(float) * static_cast<size_t>(irFftSize * 2));

    const auto usable = juce::jmin(irFftSize, impulseResponse.getNumSamples());
    const auto* irData = impulseResponse.getReadPointer(0);
    for (int i = 0; i < usable; ++i)
        fftData[i] = irData[i];

    fft.performFrequencyOnlyForwardTransform(fftData.getData());

    auto bandPowerDb = [&fftData, sampleRate](double lowHz, double highHz)
    {
        auto powerSum = 0.0;
        auto count = 0;
        for (int bin = 1; bin < irFftSize / 2; ++bin)
        {
            const auto hz = sampleRate * static_cast<double>(bin) / static_cast<double>(irFftSize);
            if (hz >= lowHz && hz < highHz)
            {
                const auto magnitude = static_cast<double>(fftData[bin]);
                powerSum += magnitude * magnitude;
                ++count;
            }
        }

        if (count <= 0 || powerSum <= 0.0)
            return -120.0f;

        return juce::jmax(-120.0f, static_cast<float>(10.0 * std::log10(powerSum / count)));
    };

    std::array<float, static_cast<size_t>(cabinetToneBandCount)> bandDb {};
    auto meanDb = 0.0f;
    auto anyBandAudible = false;
    for (int band = 0; band < cabinetToneBandCount; ++band)
    {
        bandDb[static_cast<size_t>(band)] = bandPowerDb(kCabinetToneBands[band].lowHz,
                                                        kCabinetToneBands[band].highHz);
        meanDb += bandDb[static_cast<size_t>(band)];
        if (bandDb[static_cast<size_t>(band)] > -110.0f)
            anyBandAudible = true;
    }

    if (! anyBandAudible)
        return profile;

    meanDb /= static_cast<float>(cabinetToneBandCount);

    auto energy = 0.0;
    const auto* data = impulseResponse.getReadPointer(0);
    for (int i = 0; i < impulseResponse.getNumSamples(); ++i)
        energy += static_cast<double>(data[i]) * static_cast<double>(data[i]);

    profile.valid = true;
    profile.estimated = false;
    profile.levelDb = energy > 0.0 ? static_cast<float>(10.0 * std::log10(energy)) : -120.0f;
    for (int band = 0; band < cabinetToneBandCount; ++band)
        profile.bandGainsDb[static_cast<size_t>(band)] =
            juce::jlimit(-36.0f, 36.0f, bandDb[static_cast<size_t>(band)] - meanDb);

    return profile;
}
}

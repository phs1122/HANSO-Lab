#pragma once

#include <JuceHeader.h>

namespace hanso
{
// Frame-averaged (Welch) power spectrum over the entire buffer.
// Earlier implementations analysed only the first FFT window, which for a
// 5-second log sweep meant looking at the lowest few tens of Hz only.
class AveragedPowerSpectrum final
{
public:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;

    AveragedPowerSpectrum(const juce::AudioBuffer<float>& buffer, double sampleRateIn)
        : sampleRate(sampleRateIn)
    {
        binPower.resize(static_cast<size_t>(fftSize / 2), 0.0);
        if (buffer.getNumChannels() == 0 || buffer.getNumSamples() <= 8 || sampleRate <= 0.0)
            return;

        juce::dsp::FFT fft(fftOrder);
        juce::HeapBlock<float> fftData(static_cast<size_t>(fftSize * 2));

        const auto numSamples = buffer.getNumSamples();
        const auto hop = fftSize / 2;
        const auto* data = buffer.getReadPointer(0);

        for (int start = 0; start < numSamples; start += hop)
        {
            const auto usable = juce::jmin(fftSize, numSamples - start);
            if (usable <= 8)
                break;

            juce::zeromem(fftData.getData(), sizeof(float) * static_cast<size_t>(fftSize * 2));
            for (int i = 0; i < usable; ++i)
            {
                const auto window = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi
                                                           * static_cast<float>(i)
                                                           / static_cast<float>(fftSize - 1));
                fftData[i] = data[start + i] * window;
            }

            fft.performFrequencyOnlyForwardTransform(fftData.getData());

            for (int bin = 0; bin < fftSize / 2; ++bin)
            {
                const auto magnitude = static_cast<double>(fftData[bin]);
                binPower[static_cast<size_t>(bin)] += magnitude * magnitude;
            }

            ++frameCount;
        }
    }

    bool isValid() const noexcept { return frameCount > 0; }

    // Mean power inside [lowHz, highHz), in dB. Returns -120 when empty.
    float bandPowerDb(double lowHz, double highHz) const noexcept
    {
        if (frameCount <= 0)
            return -120.0f;

        auto powerSum = 0.0;
        auto count = 0;
        for (int bin = 1; bin < fftSize / 2; ++bin)
        {
            const auto hz = sampleRate * static_cast<double>(bin) / static_cast<double>(fftSize);
            if (hz >= lowHz && hz < highHz)
            {
                powerSum += binPower[static_cast<size_t>(bin)] / static_cast<double>(frameCount);
                ++count;
            }
        }

        if (count <= 0 || powerSum <= 0.0)
            return -120.0f;

        const auto meanPower = powerSum / static_cast<double>(count);
        return juce::jmax(-120.0f, static_cast<float>(10.0 * std::log10(meanPower)));
    }

private:
    double sampleRate { 0.0 };
    int frameCount { 0 };
    std::vector<double> binPower;
};
}

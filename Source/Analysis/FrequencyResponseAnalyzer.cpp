#include "Analysis/FrequencyResponseAnalyzer.h"

namespace hanso
{
FrequencyResponseSummary FrequencyResponseAnalyzer::analyze(const juce::AudioBuffer<float>& buffer, double sampleRate) const
{
    FrequencyResponseSummary summary;
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return summary;

    const auto fftOrder = 12;
    const auto fftSize = 1 << fftOrder;
    const auto usableSamples = juce::jmin(fftSize, buffer.getNumSamples());
    juce::dsp::FFT fft(fftOrder);
    juce::HeapBlock<float> fftData(static_cast<size_t>(fftSize * 2));
    juce::zeromem(fftData.getData(), sizeof(float) * static_cast<size_t>(fftSize * 2));

    for (int i = 0; i < usableSamples; ++i)
    {
        const auto window = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * static_cast<float>(i)
                                                   / static_cast<float>(juce::jmax(1, usableSamples - 1)));
        fftData[i] = buffer.getSample(0, i) * window;
    }

    fft.performFrequencyOnlyForwardTransform(fftData.getData());

    auto bandAverage = [&](double lowHz, double highHz)
    {
        auto sum = 0.0f;
        auto count = 0;
        const auto maxBin = fftSize / 2;
        for (int bin = 1; bin < maxBin; ++bin)
        {
            const auto hz = sampleRate * static_cast<double>(bin) / static_cast<double>(fftSize);
            if (hz >= lowHz && hz < highHz)
            {
                sum += fftData[bin];
                ++count;
            }
        }

        return count > 0 ? juce::Decibels::gainToDecibels(sum / static_cast<float>(count), -120.0f) : -120.0f;
    };

    summary.lowBandDb = bandAverage(60.0, 250.0);
    summary.midBandDb = bandAverage(250.0, 2500.0);
    summary.highBandDb = bandAverage(2500.0, 10000.0);
    return summary;
}
}

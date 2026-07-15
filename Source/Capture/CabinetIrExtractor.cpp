#include "Capture/CabinetIrExtractor.h"

#include <complex>

namespace hanso
{
namespace
{
constexpr float targetPeak = 0.8912509f; // -1 dBFS
constexpr float regularizationRatio = 1.0e-7f;
constexpr float tailThresholdRatio = 0.00031623f; // -70 dB
constexpr double searchSeconds = 0.25;
constexpr double preRollSeconds = 0.001;
constexpr double minimumIrSeconds = 0.05;
constexpr double maximumIrSeconds = 1.0;
constexpr double tailFadeSeconds = 0.02;

int nextFftOrder(int requiredSamples)
{
    auto order = 1;
    auto size = 2;
    while (size < requiredSamples && order < 24)
    {
        ++order;
        size <<= 1;
    }
    return order;
}
}

CabinetIrExtractionResult CabinetIrExtractor::extract(const juce::AudioBuffer<float>& dryProbe,
                                                       const juce::AudioBuffer<float>& alignedReturn,
                                                       double sampleRate) const
{
    CabinetIrExtractionResult result;
    if (sampleRate <= 0.0 || dryProbe.getNumChannels() <= 0 || alignedReturn.getNumChannels() <= 0
        || dryProbe.getNumSamples() < 1024 || alignedReturn.getNumSamples() < 1024)
    {
        result.error = "Cabinet IR extraction requires non-empty dry and return probes.";
        return result;
    }

    const auto requiredSamples = dryProbe.getNumSamples() + alignedReturn.getNumSamples();
    const auto fftOrder = nextFftOrder(requiredSamples);
    const auto fftSize = 1 << fftOrder;
    if (fftSize < requiredSamples)
    {
        result.error = "Cabinet capture is too long for the offline IR extractor.";
        return result;
    }

    using Complex = std::complex<float>;
    std::vector<Complex> dryTime(static_cast<size_t>(fftSize));
    std::vector<Complex> returnTime(static_cast<size_t>(fftSize));
    std::vector<Complex> drySpectrum(static_cast<size_t>(fftSize));
    std::vector<Complex> returnSpectrum(static_cast<size_t>(fftSize));

    for (int sample = 0; sample < dryProbe.getNumSamples(); ++sample)
    {
        auto value = 0.0f;
        for (int channel = 0; channel < dryProbe.getNumChannels(); ++channel)
            value += dryProbe.getSample(channel, sample);
        dryTime[static_cast<size_t>(sample)] = Complex(value / static_cast<float>(dryProbe.getNumChannels()), 0.0f);
    }
    for (int sample = 0; sample < alignedReturn.getNumSamples(); ++sample)
    {
        auto value = 0.0f;
        for (int channel = 0; channel < alignedReturn.getNumChannels(); ++channel)
            value += alignedReturn.getSample(channel, sample);
        returnTime[static_cast<size_t>(sample)] = Complex(value / static_cast<float>(alignedReturn.getNumChannels()), 0.0f);
    }

    juce::dsp::FFT fft(fftOrder);
    fft.perform(dryTime.data(), drySpectrum.data(), false);
    fft.perform(returnTime.data(), returnSpectrum.data(), false);

    auto maximumDryPower = 0.0f;
    for (const auto& value : drySpectrum)
        maximumDryPower = juce::jmax(maximumDryPower, std::norm(value));
    if (maximumDryPower <= 1.0e-12f)
    {
        result.error = "Cabinet dry probe contains no usable energy.";
        return result;
    }

    const auto regularization = maximumDryPower * regularizationRatio;
    for (int bin = 0; bin < fftSize; ++bin)
    {
        const auto dry = drySpectrum[static_cast<size_t>(bin)];
        dryTime[static_cast<size_t>(bin)] = returnSpectrum[static_cast<size_t>(bin)] * std::conj(dry)
                                              / (std::norm(dry) + regularization);
    }
    fft.perform(dryTime.data(), returnTime.data(), true);

    const auto searchSamples = juce::jmin(fftSize, juce::jmax(1, static_cast<int>(sampleRate * searchSeconds)));
    auto directPeak = 0;
    auto peak = 0.0f;
    for (int sample = 0; sample < searchSamples; ++sample)
    {
        const auto magnitude = std::abs(returnTime[static_cast<size_t>(sample)].real());
        if (magnitude > peak)
        {
            peak = magnitude;
            directPeak = sample;
        }
    }
    if (peak <= 1.0e-8f || ! std::isfinite(peak))
    {
        result.error = "Cabinet deconvolution produced no stable direct response.";
        return result;
    }

    const auto preRoll = static_cast<int>(sampleRate * preRollSeconds);
    const auto start = juce::jmax(0, directPeak - preRoll);
    const auto maximumLength = juce::jmin(fftSize - start, static_cast<int>(sampleRate * maximumIrSeconds));
    const auto minimumLength = juce::jmin(maximumLength, static_cast<int>(sampleRate * minimumIrSeconds));
    const auto threshold = peak * tailThresholdRatio;
    auto lastUseful = minimumLength - 1;
    for (int sample = minimumLength; sample < maximumLength; ++sample)
        if (std::abs(returnTime[static_cast<size_t>(start + sample)].real()) >= threshold)
            lastUseful = sample;

    const auto fadeSamples = juce::jmax(1, static_cast<int>(sampleRate * tailFadeSeconds));
    const auto outputSamples = juce::jlimit(minimumLength, maximumLength, lastUseful + fadeSamples + 1);
    result.impulseResponse.setSize(1, outputSamples);
    for (int sample = 0; sample < outputSamples; ++sample)
        result.impulseResponse.setSample(0, sample, returnTime[static_cast<size_t>(start + sample)].real());

    const auto appliedFade = juce::jmin(fadeSamples, outputSamples);
    if (appliedFade > 1)
        result.impulseResponse.applyGainRamp(0, outputSamples - appliedFade, appliedFade, 1.0f, 0.0f);

    const auto outputPeak = result.impulseResponse.getMagnitude(0, 0, outputSamples);
    if (outputPeak <= 1.0e-8f || ! std::isfinite(outputPeak))
    {
        result.impulseResponse.setSize(0, 0);
        result.error = "Cabinet IR normalization failed because the response is silent or invalid.";
        return result;
    }
    result.impulseResponse.applyGain(targetPeak / outputPeak);
    result.directPeakSample = directPeak - start;
    result.success = true;
    return result;
}
}

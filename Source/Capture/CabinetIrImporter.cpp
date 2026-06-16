#include "Capture/CabinetIrImporter.h"

namespace hanso
{
namespace
{
constexpr int maxIrSeconds = 2;
constexpr float trimThreshold = 0.00003f;
constexpr float targetPeak = 0.8912509f; // -1 dBFS

float maxAbsSample(const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = juce::jmax(peak, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
    return peak;
}
}

CabinetIrImportResult CabinetIrImporter::importFile(const juce::File& file) const
{
    CabinetIrImportResult result;
    if (! file.existsAsFile())
    {
        result.error = "IR file does not exist.";
        return result;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr)
    {
        result.error = "Unsupported IR audio file. Use WAV for this phase.";
        return result;
    }

    const auto sampleRate = reader->sampleRate;
    const auto maxSamples = static_cast<juce::int64>(sampleRate * maxIrSeconds);
    const auto samplesToRead = static_cast<int>(juce::jmin<juce::int64>(reader->lengthInSamples, maxSamples));
    if (samplesToRead <= 0)
    {
        result.error = "IR file is empty.";
        return result;
    }

    const auto channelsToRead = juce::jlimit(1, 2, static_cast<int>(reader->numChannels));
    result.impulseResponse.setSize(channelsToRead, samplesToRead);
    reader->read(&result.impulseResponse, 0, samplesToRead, 0, true, channelsToRead > 1);

    trimTrailingSilence(result.impulseResponse);
    normalisePeak(result.impulseResponse);

    result.sampleRate = sampleRate;
    result.success = result.impulseResponse.getNumSamples() > 0;
    if (! result.success)
        result.error = "IR file only contains silence.";

    return result;
}

void CabinetIrImporter::trimTrailingSilence(juce::AudioBuffer<float>& buffer)
{
    int lastUsefulSample = buffer.getNumSamples() - 1;
    for (; lastUsefulSample > 0; --lastUsefulSample)
    {
        bool aboveThreshold = false;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            if (std::abs(buffer.getSample(ch, lastUsefulSample)) > trimThreshold)
            {
                aboveThreshold = true;
                break;
            }
        }

        if (aboveThreshold)
            break;
    }

    const auto keepSamples = juce::jlimit(1, buffer.getNumSamples(), lastUsefulSample + 1);
    if (keepSamples < buffer.getNumSamples())
        buffer.setSize(buffer.getNumChannels(), keepSamples, true, true, true);
}

void CabinetIrImporter::normalisePeak(juce::AudioBuffer<float>& buffer)
{
    const auto peak = maxAbsSample(buffer);
    if (peak <= 0.0f)
        return;

    buffer.applyGain(targetPeak / peak);
}
}

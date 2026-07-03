#include "Analysis/FrequencyResponseAnalyzer.h"

#include "Analysis/SpectrumUtils.h"

namespace hanso
{
FrequencyResponseSummary FrequencyResponseAnalyzer::analyze(const juce::AudioBuffer<float>& buffer, double sampleRate) const
{
    FrequencyResponseSummary summary;
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return summary;

    const AveragedPowerSpectrum spectrum(buffer, sampleRate);
    if (! spectrum.isValid())
        return summary;

    summary.lowBandDb = spectrum.bandPowerDb(60.0, 250.0);
    summary.midBandDb = spectrum.bandPowerDb(250.0, 2500.0);
    summary.highBandDb = spectrum.bandPowerDb(2500.0, 10000.0);
    return summary;
}
}

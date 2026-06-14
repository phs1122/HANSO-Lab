#include "Analysis/DynamicResponseAnalyzer.h"

namespace hanso
{
DynamicResponseSummary DynamicResponseAnalyzer::analyze(const juce::AudioBuffer<float>& buffer, double sampleRate) const
{
    DynamicResponseSummary summary;
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return summary;

    auto peak = buffer.getMagnitude(0, 0, buffer.getNumSamples());
    auto rms = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
    summary.dynamicRangeDb = juce::Decibels::gainToDecibels(peak + 1.0e-6f)
                           - juce::Decibels::gainToDecibels(rms + 1.0e-6f);

    const auto attackThreshold = peak * 0.9f;
    const auto releaseThreshold = peak * 0.1f;
    auto attackSample = 0;
    auto releaseSample = buffer.getNumSamples() - 1;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        if (std::abs(buffer.getSample(0, i)) >= attackThreshold)
        {
            attackSample = i;
            break;
        }
    }

    for (int i = buffer.getNumSamples() - 1; i >= 0; --i)
    {
        if (std::abs(buffer.getSample(0, i)) >= releaseThreshold)
        {
            releaseSample = i;
            break;
        }
    }

    summary.envelopeAttackMs = sampleRate > 0.0 ? static_cast<float>(1000.0 * attackSample / sampleRate) : 0.0f;
    summary.envelopeReleaseMs = sampleRate > 0.0 ? static_cast<float>(1000.0 * releaseSample / sampleRate) : 0.0f;
    return summary;
}
}

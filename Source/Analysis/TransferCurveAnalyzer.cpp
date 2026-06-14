#include "Analysis/TransferCurveAnalyzer.h"

namespace hanso
{
TransferCurveSummary TransferCurveAnalyzer::analyze(const juce::AudioBuffer<float>& input,
                                                    const juce::AudioBuffer<float>& output) const
{
    TransferCurveSummary summary;
    if (input.getNumChannels() == 0 || output.getNumChannels() == 0)
        return summary;

    const auto inputRms = input.getRMSLevel(0, 0, input.getNumSamples());
    const auto outputRms = output.getRMSLevel(0, 0, output.getNumSamples());
    summary.inputRmsDb = juce::Decibels::gainToDecibels(inputRms, -120.0f);
    summary.outputRmsDb = juce::Decibels::gainToDecibels(outputRms, -120.0f);
    summary.estimatedGainDb = summary.outputRmsDb - summary.inputRmsDb;
    return summary;
}
}

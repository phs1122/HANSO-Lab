#include "Analysis/ResidualDatasetBuilder.h"

namespace hanso
{
ResidualDataset ResidualDatasetBuilder::build(const juce::AudioBuffer<float>& input,
                                              const juce::AudioBuffer<float>& target,
                                              double sampleRate,
                                              int segmentLength,
                                              int hopSize) const
{
    ResidualDataset dataset;
    dataset.sampleRate = sampleRate;
    dataset.segmentLength = juce::jmax(1, segmentLength);
    dataset.hopSize = juce::jmax(1, hopSize);

    const auto samples = juce::jmin(input.getNumSamples(), target.getNumSamples());
    if (input.getNumChannels() == 0 || target.getNumChannels() == 0 || samples < dataset.segmentLength)
        return dataset;

    const auto inputPeak = input.getMagnitude(0, 0, samples);
    const auto targetPeak = target.getMagnitude(0, 0, samples);
    const auto peak = juce::jmax(inputPeak, targetPeak);
    dataset.normalizationGain = peak > 0.0f ? 1.0f / peak : 1.0f;

    for (int start = 0; start + dataset.segmentLength <= samples; start += dataset.hopSize)
    {
        auto* inputSegment = new juce::AudioBuffer<float>(1, dataset.segmentLength);
        auto* targetSegment = new juce::AudioBuffer<float>(1, dataset.segmentLength);
        inputSegment->copyFrom(0, 0, input, 0, start, dataset.segmentLength);
        targetSegment->copyFrom(0, 0, target, 0, start, dataset.segmentLength);
        inputSegment->applyGain(dataset.normalizationGain);
        targetSegment->applyGain(dataset.normalizationGain);
        dataset.inputSegments.add(inputSegment);
        dataset.targetSegments.add(targetSegment);
    }

    return dataset;
}
}

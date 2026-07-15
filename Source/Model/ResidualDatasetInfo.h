#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct ResidualDatasetInfo
{
    bool prepared { false };
    juce::String inputChunkId { "capture/shared/dry-reference.f32" };
    juce::String targetChunkId { "capture/gain-010/aligned-captured.f32" };
    double sampleRate { 48000.0 };
    float normalizationGain { 1.0f };
    int segmentLength { 2048 };
    int hopSize { 1024 };
    int inputSegmentCount { 0 };
    int targetSegmentCount { 0 };

    juce::var toVar() const
    {
        auto object = new juce::DynamicObject();
        object->setProperty("prepared", prepared);
        object->setProperty("inputChunkId", inputChunkId);
        object->setProperty("targetChunkId", targetChunkId);
        object->setProperty("sampleRate", sampleRate);
        object->setProperty("normalizationGain", normalizationGain);
        object->setProperty("segmentLength", segmentLength);
        object->setProperty("hopSize", hopSize);
        object->setProperty("inputSegmentCount", inputSegmentCount);
        object->setProperty("targetSegmentCount", targetSegmentCount);
        return object;
    }
};
}

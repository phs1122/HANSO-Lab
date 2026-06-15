#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct CompactHansoModelAnchor
{
    float parameterValue { 0.5f };
    float inputGainDb { 0.0f };
    float drive { 1.0f };
    float outputGainDb { 0.0f };
    float lowShelfDb { 0.0f };
    float midPeakDb { 0.0f };
    float highShelfDb { 0.0f };
    juce::String sourceChunkId;
};

struct CompactHansoModel
{
    int version { 1 };
    double sampleRate { 48000.0 };
    int inputChannels { 1 };
    int outputChannels { 2 };
    juce::String algorithm { "HANSO compact gain-tone v1" };
    juce::String parameterKey { "gain" };
    std::vector<CompactHansoModelAnchor> anchors;

    bool isValid() const noexcept
    {
        return sampleRate > 0.0 && ! anchors.empty();
    }

    const CompactHansoModelAnchor* nearestAnchor(float parameterValue) const noexcept
    {
        if (anchors.empty())
            return nullptr;

        const auto target = juce::jlimit(0.0f, 1.0f, parameterValue);
        const auto* best = &anchors.front();
        auto bestDistance = std::abs(best->parameterValue - target);

        for (const auto& anchor : anchors)
        {
            const auto distance = std::abs(anchor.parameterValue - target);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = &anchor;
            }
        }

        return best;
    }

    juce::String summary() const
    {
        if (! isValid())
            return "No compact HANSO model";

        return algorithm + " / " + juce::String(anchors.size()) + " gain anchors / "
             + juce::String(sampleRate, 0) + " Hz";
    }
};
}

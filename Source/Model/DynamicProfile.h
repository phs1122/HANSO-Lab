#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct DynamicProfile
{
    float estimatedLatencySeconds { 0.0f };
    float dynamicRangeDb { 0.0f };
    float transientScore { 0.0f };
    // The current sweep-based capture cannot measure envelope attack/release,
    // so transientScore stays unmeasured until dedicated dynamics probes exist.
    bool transientMeasured { false };

    juce::var toVar() const
    {
        auto object = new juce::DynamicObject();
        object->setProperty("estimatedLatencySeconds", estimatedLatencySeconds);
        object->setProperty("dynamicRangeDb", dynamicRangeDb);
        object->setProperty("transientScore", transientMeasured ? juce::var(transientScore) : juce::var());
        object->setProperty("transientMeasured", transientMeasured);
        return object;
    }
};
}

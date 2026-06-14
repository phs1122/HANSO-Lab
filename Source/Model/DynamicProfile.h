#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct DynamicProfile
{
    float estimatedLatencySeconds { 0.0f };
    float dynamicRangeDb { 0.0f };
    float transientScore { 0.0f };

    juce::var toVar() const
    {
        auto object = new juce::DynamicObject();
        object->setProperty("estimatedLatencySeconds", estimatedLatencySeconds);
        object->setProperty("dynamicRangeDb", dynamicRangeDb);
        object->setProperty("transientScore", transientScore);
        return object;
    }
};
}

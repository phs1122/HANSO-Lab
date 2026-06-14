#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct CircuitProfile
{
    juce::String topologyName;
    juce::StringArray exposedParameters;
    juce::var toVar() const
    {
        auto object = new juce::DynamicObject();
        object->setProperty("topologyName", topologyName);

        juce::Array<juce::var> parameters;
        for (const auto& parameter : exposedParameters)
            parameters.add(parameter);

        object->setProperty("exposedParameters", parameters);
        return object;
    }
};
}

#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct ResidualModelInfo
{
    bool present { false };
    juce::String architecture;
    juce::String trainingSummary;

    juce::var toVar() const
    {
        auto object = new juce::DynamicObject();
        object->setProperty("present", present);
        object->setProperty("architecture", architecture);
        object->setProperty("trainingSummary", trainingSummary);
        return object;
    }
};
}

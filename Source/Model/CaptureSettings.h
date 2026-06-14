#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct CaptureSettings
{
    double sampleRate { 48000.0 };
    int bufferSize { 512 };
    int inputChannels { 1 };
    int outputChannels { 1 };
    double durationSeconds { 5.0 };
    juce::String testSignalType { "LogSineSweep" };

    juce::var toVar() const
    {
        auto object = new juce::DynamicObject();
        object->setProperty("sampleRate", sampleRate);
        object->setProperty("bufferSize", bufferSize);
        object->setProperty("inputChannels", inputChannels);
        object->setProperty("outputChannels", outputChannels);
        object->setProperty("durationSeconds", durationSeconds);
        object->setProperty("testSignalType", testSignalType);
        return object;
    }
};
}

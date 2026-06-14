#pragma once

#include <JuceHeader.h>

#include "Model/HansoCategory.h"

namespace hanso
{
struct CaptureMetadata
{
    juce::String name;
    juce::String author;
    juce::String manufacturer;
    juce::String model;
    juce::String year;
    juce::String deviceType;
    juce::String sourceDevice;
    juce::String notes;
    HansoCategory category { HansoCategory::Amp };
    double sampleRate { 48000.0 };
    int numInputChannels { 1 };
    int numOutputChannels { 1 };
    juce::Time createdAt { juce::Time::getCurrentTime() };
};
}

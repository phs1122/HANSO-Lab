#pragma once

#include <JuceHeader.h>

#include "Model/HansoPackage.h"

namespace hanso
{
class HansoSerializer final
{
public:
    static bool writeToFile(const HansoPackage& package, const juce::File& destination, juce::String& error);
    static juce::String metadataToJson(const HansoPackage& package);
};
}

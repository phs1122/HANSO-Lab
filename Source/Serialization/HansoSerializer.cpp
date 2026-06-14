#include "Serialization/HansoSerializer.h"

namespace hanso
{
juce::String HansoSerializer::metadataToJson(const HansoPackage& package)
{
    return juce::JSON::toString(package.createMetadataVar(), true);
}

bool HansoSerializer::writeToFile(const HansoPackage& package, const juce::File& destination, juce::String& error)
{
    if (! destination.replaceWithText(metadataToJson(package)))
    {
        error = "Could not write JSON package file.";
        return false;
    }

    return true;
}
}

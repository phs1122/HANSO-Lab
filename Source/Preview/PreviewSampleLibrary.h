#pragma once

#include <JuceHeader.h>

namespace hanso
{
class PreviewSampleLibrary final
{
public:
    static juce::File samplesDirectory();
    static bool ensureSamplesDirectory(juce::String& error);
    static juce::Array<juce::File> listSampleFiles();
    static juce::File importSampleFile(const juce::File& source, juce::String& error);
    static juce::File createUniqueDestination(const juce::File& requestedFile);

private:
    static bool isSupportedAudioFile(const juce::File& file);
};
}

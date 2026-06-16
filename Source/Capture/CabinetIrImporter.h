#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct CabinetIrImportResult
{
    bool success { false };
    juce::String error;
    juce::AudioBuffer<float> impulseResponse;
    double sampleRate { 0.0 };
};

class CabinetIrImporter final
{
public:
    CabinetIrImportResult importFile(const juce::File& file) const;

private:
    static void trimTrailingSilence(juce::AudioBuffer<float>& buffer);
    static void normalisePeak(juce::AudioBuffer<float>& buffer);
};
}

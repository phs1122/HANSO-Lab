#pragma once

#include <JuceHeader.h>

namespace hanso
{
struct CabinetIrExtractionResult
{
    bool success { false };
    juce::String error;
    juce::AudioBuffer<float> impulseResponse;
    int directPeakSample { 0 };
};

class CabinetIrExtractor final
{
public:
    CabinetIrExtractionResult extract(const juce::AudioBuffer<float>& dryProbe,
                                      const juce::AudioBuffer<float>& alignedReturn,
                                      double sampleRate) const;
};
}

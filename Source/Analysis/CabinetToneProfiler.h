#pragma once

#include <JuceHeader.h>

#include "Model/CabinetToneProfile.h"

namespace hanso
{
class CabinetToneProfiler final
{
public:
    // Derives the per-band tone profile from an impulse response.
    static CabinetToneProfile fromImpulseResponse(const juce::AudioBuffer<float>& impulseResponse,
                                                  double sampleRate);
};
}

#pragma once

#include <JuceHeader.h>

#include "Model/CompactHansoModel.h"

namespace hanso
{
class HansoModelCodec final
{
public:
    static constexpr const char* compactModelChunkId = "model/compact-v1.hmodel";

    static juce::MemoryBlock encodeCompactModel(const CompactHansoModel& model);
    static bool decodeCompactModel(const juce::MemoryBlock& data,
                                   CompactHansoModel& model,
                                   juce::String& error);
};
}

#pragma once

#include <JuceHeader.h>

#include "Model/CaptureMetadata.h"
#include "Model/CaptureSettings.h"
#include "Model/AnalysisSummary.h"
#include "Model/CircuitProfile.h"
#include "Model/DynamicProfile.h"
#include "Model/ResidualModelInfo.h"

namespace hanso
{
struct HansoBinaryChunk
{
    juce::String id;
    juce::MemoryBlock data;
};

class HansoPackage final
{
public:
    int formatVersion { 1 };
    CaptureMetadata metadata;
    CaptureSettings captureSettings;
    AnalysisSummary analysisSummary;
    CircuitProfile circuitProfile;
    DynamicProfile dynamicProfile;
    ResidualModelInfo residualModel;
    juce::OwnedArray<HansoBinaryChunk> chunks;

    juce::var createMetadataVar() const;
    void addChunk(juce::String id, const juce::MemoryBlock& data);
};
}

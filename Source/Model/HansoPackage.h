#pragma once

#include <JuceHeader.h>

#include "Model/CaptureMetadata.h"
#include "Model/CaptureSettings.h"
#include "Model/AnalysisSummary.h"
#include "Model/CircuitProfile.h"
#include "Model/DynamicProfile.h"
#include "Model/ResidualDatasetInfo.h"
#include "Model/ResidualModelInfo.h"

namespace hanso
{
struct HansoBinaryChunk
{
    juce::String id;
    juce::String role;
    juce::String mediaType;
    double sampleRate { 0.0 };
    int numChannels { 0 };
    int numSamples { 0 };
    juce::MemoryBlock data;
};

class HansoPackage final
{
public:
    int formatVersion { 2 };
    CaptureMetadata metadata;
    CaptureSettings captureSettings;
    AnalysisSummary analysisSummary;
    CircuitProfile circuitProfile;
    DynamicProfile dynamicProfile;
    ResidualDatasetInfo residualDataset;
    ResidualModelInfo residualModel;
    juce::var captureWorkflow;
    juce::OwnedArray<HansoBinaryChunk> chunks;

    juce::var createMetadataVar() const;
    void addChunk(juce::String id, const juce::MemoryBlock& data);
    void setChunk(juce::String id,
                  juce::String role,
                  juce::String mediaType,
                  const juce::MemoryBlock& data,
                  double sampleRate = 0.0,
                  int numChannels = 0,
                  int numSamples = 0);
    void clearChunks();
    void removeChunk(const juce::String& id);
    const HansoBinaryChunk* findChunk(const juce::String& id) const noexcept;
};
}

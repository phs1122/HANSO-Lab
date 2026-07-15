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
    // Asset tier (container doc §6.2): "master" (.hansocap, full capture audio,
    // app-managed and invisible to users) or "distribution" (.hanso, stripped
    // runtime build — the only file users see). Empty = legacy/unspecified.
    juce::String assetTier;
    // Stable capture id linking a distribution .hanso back to its archived
    // master, so re-fitting works without users managing master files.
    juce::String captureId;
    CaptureMetadata metadata;
    CaptureSettings captureSettings;
    AnalysisSummary analysisSummary;
    CircuitProfile circuitProfile;
    DynamicProfile dynamicProfile;
    ResidualDatasetInfo residualDataset;
    ResidualModelInfo residualModel;
    juce::var captureWorkflow;
    juce::var cabinetProfile;
    // Per-anchor model-vs-real-recording accuracy report (ESR etc.), written
    // by the finish-capture fidelity evaluation when sample recordings exist.
    juce::var captureFidelity;
    juce::var dspCore;
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

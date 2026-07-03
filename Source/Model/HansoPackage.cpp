#include "Model/HansoPackage.h"

#include <cmath>

namespace hanso
{
namespace
{
constexpr int dspCoreSchemaVersion = 1;

juce::String firstCabinetIrChunkId(const HansoPackage& package)
{
    if (const auto* profileObject = package.cabinetProfile.getDynamicObject())
    {
        const auto positionsVar = profileObject->getProperty("positions");
        if (positionsVar.isArray())
        {
            for (const auto& entry : *positionsVar.getArray())
            {
                const auto* positionObject = entry.getDynamicObject();
                if (positionObject == nullptr)
                    continue;

                const auto source = positionObject->getProperty("source").toString();
                if (source != "captured-ir" && source != "imported-ir")
                    continue;

                const auto chunkId = positionObject->getProperty("impulseResponseChunkId").toString();
                if (chunkId.isNotEmpty())
                    return chunkId;
            }
        }
    }

    for (const auto* chunk : package.chunks)
    {
        if (chunk->id.startsWith("cabinet/positions/") && chunk->id.endsWith("/ir.pcm16"))
            return chunk->id;
    }

    return {};
}

juce::String dspIdentityForCategory(HansoCategory category)
{
    switch (category)
    {
        case HansoCategory::Amp: return "Amp";
        case HansoCategory::Pedal: return "Effector";
        case HansoCategory::Cabinet: return "Cab";
        case HansoCategory::Rig: return "Chain";
        case HansoCategory::Speaker:
        case HansoCategory::Pickup:
        case HansoCategory::Utility: return "Utility";
        case HansoCategory::Unknown: return "Unknown";
    }

    return "Unknown";
}

juce::var makeStringArray(const juce::StringArray& values)
{
    juce::Array<juce::var> array;
    for (const auto& value : values)
        array.add(value);
    return array;
}

juce::var makeRange(double min, double max)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("min", min);
    object->setProperty("max", max);
    return object;
}

juce::var makeFilter(const juce::String& type, double frequencyHz, double q = 0.707, double gainDb = 0.0)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("type", type);
    object->setProperty("frequencyHz", frequencyHz);
    object->setProperty("q", q);
    object->setProperty("gainDb", gainDb);
    return object;
}

juce::var makeCaptureModel(const HansoPackage& package)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("modelKind", "static_v1");
    object->setProperty("anchors", makeStringArray({ "preGain", "level" }));

    const auto gain = juce::jlimit(0.25f, 4.0f, juce::Decibels::decibelsToGain(package.analysisSummary.estimatedGainDb));
    const auto drive = juce::jlimit(1.0f, 3.0f, 1.0f + package.analysisSummary.dynamicRangeDb / 12.0f);
    const auto normalizer = std::tanh(static_cast<double>(gain * drive));

    juce::Array<juce::var> lut;
    constexpr int lutSize = 33;
    for (int i = 0; i < lutSize; ++i)
    {
        const auto x = -1.0 + 2.0 * static_cast<double>(i) / static_cast<double>(lutSize - 1);
        const auto y = normalizer > 0.0 ? std::tanh(x * static_cast<double>(gain * drive)) / normalizer : x;
        lut.add(juce::jlimit(-1.0, 1.0, y));
    }
    object->setProperty("waveshaperLut", lut);

    juce::Array<juce::var> preEq;
    if (package.analysisSummary.lowBandDb < package.analysisSummary.midBandDb - 1.0f)
        preEq.add(makeFilter("HighPass", 70.0));
    object->setProperty("preEq", preEq);

    juce::Array<juce::var> postEq;
    if (package.analysisSummary.highBandDb < package.analysisSummary.midBandDb - 1.0f)
        postEq.add(makeFilter("LowPass", 6500.0));
    object->setProperty("postEq", postEq);

    object->setProperty("level", juce::jlimit(0.1f, 1.0f, gain * 0.5f));
    object->setProperty("preGain", 1.0);
    object->setProperty("source", "HANSO Lab analysisSummary");
    return object;
}

juce::var makeComponent(const juce::String& id,
                        const juce::String& type,
                        const juce::NamedValueSet& properties,
                        const juce::StringArray& nodes)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", id);
    object->setProperty("type", type);

    auto* propertyObject = new juce::DynamicObject();
    for (const auto& property : properties)
        propertyObject->setProperty(property.name, property.value);
    object->setProperty("properties", propertyObject);
    object->setProperty("nodes", makeStringArray(nodes));
    return object;
}

juce::var makeNode(const juce::String& id, const juce::StringArray& connections)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", id);
    object->setProperty("connections", makeStringArray(connections));
    return object;
}

juce::var makeCircuitModel(const HansoPackage& package)
{
    juce::Array<juce::var> components;

    juce::NamedValueSet gainProperties;
    gainProperties.set("position", 0.5);
    gainProperties.set("role", "Gain");
    gainProperties.set("taper", "linear");
    components.add(makeComponent("gainPot", "Potentiometer", gainProperties, { "in", "n1" }));

    const auto bass = juce::jlimit(0.0f, 1.0f, 0.5f + package.analysisSummary.lowBandDb / 24.0f);
    const auto mid = juce::jlimit(0.0f, 1.0f, 0.5f + package.analysisSummary.midBandDb / 24.0f);
    const auto treble = juce::jlimit(0.0f, 1.0f, 0.5f + package.analysisSummary.highBandDb / 24.0f);
    juce::NamedValueSet toneProperties;
    toneProperties.set("bass", bass);
    toneProperties.set("mid", mid);
    toneProperties.set("treble", treble);
    components.add(makeComponent("tone", "ToneControl", toneProperties, { "n1", "n2" }));

    juce::NamedValueSet outputProperties;
    outputProperties.set("level", 0.85);
    components.add(makeComponent("out", "Output", outputProperties, { "n2", "out" }));

    juce::Array<juce::var> nodes;
    nodes.add(makeNode("in", { "gainPot.1" }));
    nodes.add(makeNode("n1", { "gainPot.2", "tone.in" }));
    nodes.add(makeNode("n2", { "tone.out", "out.in" }));
    nodes.add(makeNode("out", { "out.out" }));

    auto* object = new juce::DynamicObject();
    object->setProperty("components", components);
    object->setProperty("nodes", nodes);
    object->setProperty("stageOrder", makeStringArray({ "gainPot", "tone", "out" }));
    object->setProperty("source", package.circuitProfile.topologyName);
    return object;
}

juce::var makeBinding(const juce::String& paramId,
                      const juce::String& targetType,
                      const juce::String& targetId,
                      const juce::String& property,
                      const juce::String& anchor = {},
                      double targetMin = 0.0,
                      double targetMax = 1.0)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("paramId", paramId);
    object->setProperty("targetType", targetType);
    object->setProperty("targetId", targetId);
    object->setProperty("property", property);
    object->setProperty("anchor", anchor);
    object->setProperty("mapSource", "ModelSpecific");
    object->setProperty("curve", "linear");
    object->setProperty("targetRange", makeRange(targetMin, targetMax));
    return object;
}

juce::var makeParameter(const juce::String& id,
                        const juce::String& displayName,
                        double defaultValue,
                        juce::Array<juce::var> bindings)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", id);
    object->setProperty("displayName", displayName);
    object->setProperty("range", makeRange(0.0, 1.0));
    object->setProperty("default", defaultValue);
    object->setProperty("mapSource", "ModelSpecific");
    object->setProperty("bindings", bindings);
    return object;
}

juce::var makeParameterMap()
{
    juce::Array<juce::var> params;

    juce::Array<juce::var> gainBindings;
    gainBindings.add(makeBinding("gain", "componentProperty", "gainPot", "position", {}, 0.0, 1.0));
    gainBindings.add(makeBinding("gain", "captureAnchor", {}, "preGain", "preGain", 0.75, 1.35));
    params.add(makeParameter("gain", "Gain", 0.5, gainBindings));

    juce::Array<juce::var> toneBindings;
    toneBindings.add(makeBinding("tone", "componentProperty", "tone", "treble", {}, 0.0, 1.0));
    params.add(makeParameter("tone", "Tone", 0.5, toneBindings));

    juce::Array<juce::var> levelBindings;
    levelBindings.add(makeBinding("level", "componentProperty", "out", "level", {}, 0.0, 1.0));
    levelBindings.add(makeBinding("level", "captureAnchor", {}, "level", "level", 0.1, 1.0));
    params.add(makeParameter("level", "Level", 0.85, levelBindings));

    auto* object = new juce::DynamicObject();
    object->setProperty("params", params);
    object->setProperty("bindings", juce::Array<juce::var>());
    return object;
}

juce::var makeRuntimeProfile(const HansoPackage& package)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("sampleRateHint", package.metadata.sampleRate);
    object->setProperty("oversampling", 1);
    return object;
}

juce::var makeDspCore(const HansoPackage& package)
{
    if (! package.dspCore.isVoid())
        return package.dspCore;

    auto* object = new juce::DynamicObject();
    object->setProperty("formatVersion", dspCoreSchemaVersion);
    object->setProperty("generator", "lab_fallback_v1");
    object->setProperty("identity", dspIdentityForCategory(package.metadata.category));

    auto* meta = new juce::DynamicObject();
    meta->setProperty("name", package.metadata.name);
    meta->setProperty("author", package.metadata.author);
    meta->setProperty("manufacturer", package.metadata.manufacturer);
    meta->setProperty("model", package.metadata.model);
    meta->setProperty("year", package.metadata.year);
    meta->setProperty("deviceType", package.metadata.deviceType);
    meta->setProperty("sourceDevice", package.metadata.sourceDevice);
    meta->setProperty("notes", package.metadata.notes);
    meta->setProperty("tags", makeStringArray({ toString(package.metadata.category), "HANSO Lab" }));
    object->setProperty("meta", meta);

    object->setProperty("captureModel", makeCaptureModel(package));
    object->setProperty("circuitModel", makeCircuitModel(package));

    auto* hybridBinding = new juce::DynamicObject();
    hybridBinding->setProperty("strategy", "CaptureFirstHybrid");
    auto* residualParams = new juce::DynamicObject();
    residualParams->setProperty("trim", 1.0);
    hybridBinding->setProperty("residualParams", residualParams);
    object->setProperty("hybridBinding", hybridBinding);

    object->setProperty("parameterMap", makeParameterMap());
    object->setProperty("runtimeProfile", makeRuntimeProfile(package));
    return object;
}
}

juce::var HansoPackage::createMetadataVar() const
{
    auto object = new juce::DynamicObject();
    object->setProperty("format", "hanso");
    object->setProperty("formatVersion", formatVersion);
    object->setProperty("packageKind", "toneAsset");
    object->setProperty("assetType", assetTypeForCategory(metadata.category));
    object->setProperty("name", metadata.name);
    object->setProperty("author", metadata.author);
    object->setProperty("createdBy", metadata.author);
    object->setProperty("manufacturer", metadata.manufacturer);
    object->setProperty("model", metadata.model);
    object->setProperty("year", metadata.year);
    object->setProperty("deviceType", metadata.deviceType);
    object->setProperty("category", toString(metadata.category));
    object->setProperty("sourceDevice", metadata.sourceDevice);
    object->setProperty("notes", metadata.notes);
    object->setProperty("sampleRate", metadata.sampleRate);
    object->setProperty("numInputChannels", metadata.numInputChannels);
    object->setProperty("numOutputChannels", metadata.numOutputChannels);
    object->setProperty("createdAt", metadata.createdAt.toISO8601(true));
    object->setProperty("captureSettings", captureSettings.toVar());
    object->setProperty("analysisSummary", analysisSummary.toVar());
    object->setProperty("circuitProfile", circuitProfile.toVar());
    object->setProperty("dynamicProfile", dynamicProfile.toVar());
    object->setProperty("residualModel", residualModel.toVar());
    auto captureData = new juce::DynamicObject();
    if (metadata.category == HansoCategory::Cabinet)
    {
        captureData->setProperty("dryReferenceChunkId", "capture/shared/cabinet-dry-reference.pcm16");
        captureData->setProperty("capturedChunkId", juce::var());
        const auto firstIrChunkId = firstCabinetIrChunkId(*this);
        captureData->setProperty("alignedCapturedChunkId",
                                 firstIrChunkId.isNotEmpty() ? juce::var(firstIrChunkId) : juce::var());
        captureData->setProperty("encoding", "pcm16 little-endian channel-major for cabinet IR captures");
    }
    else
    {
        captureData->setProperty("dryReferenceMode", "per-anchor");
        captureData->setProperty("legacySharedDryReferenceChunkId", "capture/shared/dry-reference.pcm16");
        captureData->setProperty("dryReferenceChunkId", "capture/gain-010/dry-reference.pcm16");
        captureData->setProperty("capturedChunkId", juce::var());
        captureData->setProperty("alignedCapturedChunkId", "capture/gain-010/aligned-captured.pcm16");
        juce::Array<juce::var> anchorDryReferences;
        anchorDryReferences.add("capture/gain-010/dry-reference.pcm16");
        anchorDryReferences.add("capture/gain-050/dry-reference.pcm16");
        anchorDryReferences.add("capture/gain-100/dry-reference.pcm16");
        captureData->setProperty("anchorDryReferenceChunkIds", anchorDryReferences);
        captureData->setProperty("encoding", "pcm16 little-endian channel-major for guided captures");
    }
    object->setProperty("captureData", captureData);
    if (! captureWorkflow.isVoid())
        object->setProperty("captureWorkflow", captureWorkflow);
    if (! cabinetProfile.isVoid())
        object->setProperty("cabProfile", cabinetProfile);

    auto modelData = new juce::DynamicObject();
    if (metadata.category == HansoCategory::Cabinet)
    {
        modelData->setProperty("primaryModelType", "cabinet-mic-position-v1");
        modelData->setProperty("algorithm", "hanso.cabinet.mic-position.v1");
        modelData->setProperty("micPositionControlAvailable", true);
        modelData->setProperty("realtimePreviewCompatible", true);
    }
    else if (const auto* compactModel = findChunk("model/compact-v1.hmodel"))
    {
        modelData->setProperty("compactModelChunkId", compactModel->id);
        modelData->setProperty("mediaType", compactModel->mediaType);
        modelData->setProperty("algorithm", "HANSO compact gain-tone v1");
        modelData->setProperty("realtimePreviewCompatible", true);
        modelData->setProperty("sizeBytes", static_cast<juce::int64>(compactModel->data.getSize()));
    }
    else
    {
        modelData->setProperty("compactModelChunkId", juce::var());
        modelData->setProperty("realtimePreviewCompatible", false);
    }
    modelData->setProperty("dspCoreLocation", "metadata.dspCore");
    modelData->setProperty("dspCoreSchemaVersion", dspCoreSchemaVersion);
    modelData->setProperty("realtimePreviewCompatible", true);
    object->setProperty("modelData", modelData);

    object->setProperty("residualDataset", residualDataset.toVar());
    object->setProperty("dspCore", makeDspCore(*this));

    object->setProperty("futureExtensions", new juce::DynamicObject());

    juce::Array<juce::var> chunkList;
    for (const auto* chunk : chunks)
    {
        auto chunkObject = new juce::DynamicObject();
        chunkObject->setProperty("id", chunk->id);
        chunkObject->setProperty("role", chunk->role);
        chunkObject->setProperty("mediaType", chunk->mediaType);
        chunkObject->setProperty("sampleRate", chunk->sampleRate);
        chunkObject->setProperty("numChannels", chunk->numChannels);
        chunkObject->setProperty("numSamples", chunk->numSamples);
        chunkObject->setProperty("sizeBytes", static_cast<juce::int64>(chunk->data.getSize()));
        chunkList.add(chunkObject);
    }
    object->setProperty("chunks", chunkList);

    return object;
}

void HansoPackage::addChunk(juce::String id, const juce::MemoryBlock& data)
{
    setChunk(std::move(id), {}, "application/octet-stream", data);
}

void HansoPackage::setChunk(juce::String id,
                            juce::String role,
                            juce::String mediaType,
                            const juce::MemoryBlock& data,
                            double sampleRate,
                            int numChannels,
                            int numSamples)
{
    for (auto* chunk : chunks)
    {
        if (chunk->id == id)
        {
            chunk->role = std::move(role);
            chunk->mediaType = std::move(mediaType);
            chunk->sampleRate = sampleRate;
            chunk->numChannels = numChannels;
            chunk->numSamples = numSamples;
            chunk->data = data;
            return;
        }
    }

    auto* chunk = new HansoBinaryChunk();
    chunk->id = std::move(id);
    chunk->role = std::move(role);
    chunk->mediaType = std::move(mediaType);
    chunk->sampleRate = sampleRate;
    chunk->numChannels = numChannels;
    chunk->numSamples = numSamples;
    chunk->data = data;
    chunks.add(chunk);
}

void HansoPackage::clearChunks()
{
    chunks.clear();
}

void HansoPackage::removeChunk(const juce::String& id)
{
    for (int i = chunks.size(); --i >= 0;)
    {
        if (chunks[i]->id == id)
        {
            chunks.remove(i);
            return;
        }
    }
}

const HansoBinaryChunk* HansoPackage::findChunk(const juce::String& id) const noexcept
{
    for (const auto* chunk : chunks)
        if (chunk->id == id)
            return chunk;

    return nullptr;
}
}

#include "Model/HansoPackage.h"

namespace hanso
{
juce::var HansoPackage::createMetadataVar() const
{
    auto object = new juce::DynamicObject();
    object->setProperty("format", "hanso");
    object->setProperty("formatVersion", formatVersion);
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
    captureData->setProperty("dryReferenceChunkId", "capture/shared/dry-reference.pcm16");
    captureData->setProperty("capturedChunkId", juce::var());
    captureData->setProperty("alignedCapturedChunkId", "capture/gain-010/aligned-captured.pcm16");
    captureData->setProperty("encoding", "pcm16 little-endian channel-major for guided captures");
    object->setProperty("captureData", captureData);
    if (! captureWorkflow.isVoid())
        object->setProperty("captureWorkflow", captureWorkflow);

    auto modelData = new juce::DynamicObject();
    if (const auto* compactModel = findChunk("model/compact-v1.hmodel"))
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
    object->setProperty("modelData", modelData);

    object->setProperty("residualDataset", residualDataset.toVar());

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

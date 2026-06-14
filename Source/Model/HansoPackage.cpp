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
    object->setProperty("futureExtensions", new juce::DynamicObject());

    juce::Array<juce::var> chunkList;
    for (const auto* chunk : chunks)
    {
        auto chunkObject = new juce::DynamicObject();
        chunkObject->setProperty("id", chunk->id);
        chunkObject->setProperty("sizeBytes", static_cast<juce::int64>(chunk->data.getSize()));
        chunkList.add(chunkObject);
    }
    object->setProperty("chunks", chunkList);

    return object;
}

void HansoPackage::addChunk(juce::String id, const juce::MemoryBlock& data)
{
    auto* chunk = new HansoBinaryChunk();
    chunk->id = std::move(id);
    chunk->data = data;
    chunks.add(chunk);
}
}

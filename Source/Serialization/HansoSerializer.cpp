#include "Serialization/HansoSerializer.h"

namespace hanso
{
namespace
{
constexpr char hansoMagic[] = { 'H', 'A', 'N', 'S', 'O', '2' };

bool writeStringBlock(juce::OutputStream& stream, const juce::String& text)
{
    const auto bytes = text.getNumBytesAsUTF8();
    stream.writeInt(static_cast<int>(bytes));
    return bytes == 0 || stream.write(text.toRawUTF8(), bytes);
}

juce::String readStringBlock(juce::InputStream& stream)
{
    const auto bytes = stream.readInt();
    if (bytes <= 0)
        return {};

    juce::MemoryBlock data(static_cast<size_t>(bytes + 1), true);
    stream.read(data.getData(), bytes);
    return juce::String::fromUTF8(static_cast<const char*>(data.getData()), bytes);
}
}

juce::String HansoSerializer::metadataToJson(const HansoPackage& package)
{
    return juce::JSON::toString(package.createMetadataVar(), true);
}

bool HansoSerializer::writeToFile(const HansoPackage& package, const juce::File& destination, juce::String& error)
{
    auto stream = std::unique_ptr<juce::FileOutputStream>(destination.createOutputStream());
    if (stream == nullptr || stream->failedToOpen())
    {
        error = "Could not open .hanso file for writing.";
        return false;
    }

    const auto json = metadataToJson(package);
    const auto jsonBytes = json.getNumBytesAsUTF8();

    stream->write(hansoMagic, sizeof(hansoMagic));
    stream->writeInt(package.formatVersion);
    stream->writeInt64(static_cast<juce::int64>(jsonBytes));
    stream->write(json.toRawUTF8(), jsonBytes);
    stream->writeInt(package.chunks.size());

    for (const auto* chunk : package.chunks)
    {
        writeStringBlock(*stream, chunk->id);
        writeStringBlock(*stream, chunk->role);
        writeStringBlock(*stream, chunk->mediaType);
        stream->writeDouble(chunk->sampleRate);
        stream->writeInt(chunk->numChannels);
        stream->writeInt(chunk->numSamples);
        stream->writeInt64(static_cast<juce::int64>(chunk->data.getSize()));
        stream->write(chunk->data.getData(), chunk->data.getSize());
    }

    if (stream->getStatus().failed())
    {
        error = stream->getStatus().getErrorMessage();
        return false;
    }

    return true;
}

bool HansoSerializer::readFromFile(const juce::File& source, HansoPackage& package, juce::String& error)
{
    auto stream = std::unique_ptr<juce::FileInputStream>(source.createInputStream());
    if (stream == nullptr || stream->failedToOpen())
    {
        error = "Could not open .hanso file for reading.";
        return false;
    }

    char magic[sizeof(hansoMagic)] {};
    stream->read(magic, sizeof(magic));
    if (std::memcmp(magic, hansoMagic, sizeof(hansoMagic)) != 0)
    {
        error = "Unsupported .hanso container. Expected HANSO2.";
        return false;
    }

    package = {};
    package.formatVersion = stream->readInt();

    const auto jsonSize = stream->readInt64();
    if (jsonSize < 0 || jsonSize > 256 * 1024 * 1024)
    {
        error = "Invalid .hanso metadata size.";
        return false;
    }

    juce::MemoryBlock jsonData(static_cast<size_t>(jsonSize + 1), true);
    stream->read(jsonData.getData(), static_cast<int>(jsonSize));
    const auto metadata = juce::JSON::parse(juce::String::fromUTF8(static_cast<const char*>(jsonData.getData()),
                                                                   static_cast<int>(jsonSize)));

    if (auto* object = metadata.getDynamicObject())
    {
        package.metadata.name = object->getProperty("name").toString();
        package.metadata.author = object->getProperty("author").toString();
        package.metadata.manufacturer = object->getProperty("manufacturer").toString();
        package.metadata.model = object->getProperty("model").toString();
        package.metadata.year = object->getProperty("year").toString();
        package.metadata.deviceType = object->getProperty("deviceType").toString();
        package.metadata.notes = object->getProperty("notes").toString();
        package.metadata.category = hansoCategoryFromString(object->getProperty("category").toString());
    }

    const auto chunkCount = stream->readInt();
    if (chunkCount < 0 || chunkCount > 4096)
    {
        error = "Invalid .hanso chunk count.";
        return false;
    }

    for (int i = 0; i < chunkCount; ++i)
    {
        auto id = readStringBlock(*stream);
        auto role = readStringBlock(*stream);
        auto mediaType = readStringBlock(*stream);
        const auto sampleRate = stream->readDouble();
        const auto numChannels = stream->readInt();
        const auto numSamples = stream->readInt();
        const auto size = stream->readInt64();

        if (size < 0 || size > 1024ll * 1024ll * 1024ll)
        {
            error = "Invalid .hanso chunk size.";
            return false;
        }

        juce::MemoryBlock data(static_cast<size_t>(size));
        stream->read(data.getData(), static_cast<int>(size));
        package.setChunk(std::move(id), std::move(role), std::move(mediaType), data, sampleRate, numChannels, numSamples);
    }

    return true;
}
}

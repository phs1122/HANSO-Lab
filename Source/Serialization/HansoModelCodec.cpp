#include "Serialization/HansoModelCodec.h"

namespace hanso
{
namespace
{
constexpr int compactModelMagic = 0x48434d31; // HCM1
constexpr int compactModelVersion = 1;

void writeString(juce::OutputStream& stream, const juce::String& text)
{
    const auto bytes = text.getNumBytesAsUTF8();
    stream.writeInt(static_cast<int>(bytes));
    if (bytes > 0)
        stream.write(text.toRawUTF8(), bytes);
}

juce::String readString(juce::InputStream& stream)
{
    const auto bytes = stream.readInt();
    if (bytes <= 0)
        return {};

    juce::MemoryBlock data(static_cast<size_t>(bytes + 1), true);
    stream.read(data.getData(), bytes);
    return juce::String::fromUTF8(static_cast<const char*>(data.getData()), bytes);
}
}

juce::MemoryBlock HansoModelCodec::encodeCompactModel(const CompactHansoModel& model)
{
    juce::MemoryBlock block;
    juce::MemoryOutputStream stream(block, false);

    stream.writeInt(compactModelMagic);
    stream.writeInt(compactModelVersion);
    stream.writeDouble(model.sampleRate);
    stream.writeInt(model.inputChannels);
    stream.writeInt(model.outputChannels);
    writeString(stream, model.algorithm);
    writeString(stream, model.parameterKey);
    stream.writeInt(static_cast<int>(model.anchors.size()));

    for (const auto& anchor : model.anchors)
    {
        stream.writeFloat(anchor.parameterValue);
        stream.writeFloat(anchor.inputGainDb);
        stream.writeFloat(anchor.drive);
        stream.writeFloat(anchor.outputGainDb);
        stream.writeFloat(anchor.lowShelfDb);
        stream.writeFloat(anchor.midPeakDb);
        stream.writeFloat(anchor.highShelfDb);
        writeString(stream, anchor.sourceChunkId);
    }

    return block;
}

bool HansoModelCodec::decodeCompactModel(const juce::MemoryBlock& data,
                                         CompactHansoModel& model,
                                         juce::String& error)
{
    juce::MemoryInputStream stream(data, false);

    if (stream.readInt() != compactModelMagic)
    {
        error = "Invalid HANSO compact model magic.";
        return false;
    }

    if (stream.readInt() != compactModelVersion)
    {
        error = "Unsupported HANSO compact model version.";
        return false;
    }

    CompactHansoModel decoded;
    decoded.version = compactModelVersion;
    decoded.sampleRate = stream.readDouble();
    decoded.inputChannels = stream.readInt();
    decoded.outputChannels = stream.readInt();
    decoded.algorithm = readString(stream);
    decoded.parameterKey = readString(stream);

    const auto anchorCount = stream.readInt();
    if (anchorCount <= 0 || anchorCount > 32)
    {
        error = "Invalid HANSO compact model anchor count.";
        return false;
    }

    decoded.anchors.reserve(static_cast<size_t>(anchorCount));
    for (int i = 0; i < anchorCount; ++i)
    {
        CompactHansoModelAnchor anchor;
        anchor.parameterValue = stream.readFloat();
        anchor.inputGainDb = stream.readFloat();
        anchor.drive = stream.readFloat();
        anchor.outputGainDb = stream.readFloat();
        anchor.lowShelfDb = stream.readFloat();
        anchor.midPeakDb = stream.readFloat();
        anchor.highShelfDb = stream.readFloat();
        anchor.sourceChunkId = readString(stream);
        decoded.anchors.push_back(std::move(anchor));
    }

    if (! decoded.isValid())
    {
        error = "Decoded HANSO compact model is empty.";
        return false;
    }

    model = std::move(decoded);
    return true;
}
}

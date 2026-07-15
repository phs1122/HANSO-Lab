#include "Serialization/HansoAudioChunkCodec.h"

#include "Model/HansoPackage.h"

namespace hanso
{
namespace
{
constexpr int audioChunkMagic = 0x48414632; // HAF2
constexpr int pcm16AudioChunkMagic = 0x48415031; // HAP1
constexpr int pcm24AudioChunkMagic = 0x48415032; // HAP2
constexpr int audioChunkVersion = 1;
}

juce::MemoryBlock HansoAudioChunkCodec::encodeFloat32Audio(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    juce::MemoryBlock block;
    juce::MemoryOutputStream stream(block, false);

    stream.writeInt(audioChunkMagic);
    stream.writeInt(audioChunkVersion);
    stream.writeDouble(sampleRate);
    stream.writeInt(buffer.getNumChannels());
    stream.writeInt(buffer.getNumSamples());

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        stream.write(buffer.getReadPointer(channel), sizeof(float) * static_cast<size_t>(buffer.getNumSamples()));

    return block;
}

juce::MemoryBlock HansoAudioChunkCodec::encodePcm16Audio(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    juce::MemoryBlock block;
    juce::MemoryOutputStream stream(block, false);

    stream.writeInt(pcm16AudioChunkMagic);
    stream.writeInt(audioChunkVersion);
    stream.writeDouble(sampleRate);
    stream.writeInt(buffer.getNumChannels());
    stream.writeInt(buffer.getNumSamples());

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto clipped = juce::jlimit(-1.0f, 1.0f, samples[sample]);
            stream.writeShort(static_cast<short>(std::round(clipped * 32767.0f)));
        }
    }

    return block;
}

juce::MemoryBlock HansoAudioChunkCodec::encodePcm24Audio(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    juce::MemoryBlock block;
    juce::MemoryOutputStream stream(block, false);

    stream.writeInt(pcm24AudioChunkMagic);
    stream.writeInt(audioChunkVersion);
    stream.writeDouble(sampleRate);
    stream.writeInt(buffer.getNumChannels());
    stream.writeInt(buffer.getNumSamples());

    // 24-bit signed, little-endian, packed 3 bytes per sample.
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto clipped = juce::jlimit(-1.0f, 1.0f, samples[sample]);
            const auto value = static_cast<juce::int32>(std::lround(clipped * 8388607.0f));
            stream.writeByte(static_cast<char>(value & 0xff));
            stream.writeByte(static_cast<char>((value >> 8) & 0xff));
            stream.writeByte(static_cast<char>((value >> 16) & 0xff));
        }
    }

    return block;
}

bool HansoAudioChunkCodec::decodeFloat32Audio(const juce::MemoryBlock& data,
                                              juce::AudioBuffer<float>& buffer,
                                              double& sampleRate,
                                              juce::String& error)
{
    juce::MemoryInputStream stream(data, false);

    if (stream.readInt() != audioChunkMagic)
    {
        error = "Invalid HANSO audio chunk magic.";
        return false;
    }

    if (stream.readInt() != audioChunkVersion)
    {
        error = "Unsupported HANSO audio chunk version.";
        return false;
    }

    sampleRate = stream.readDouble();
    const auto numChannels = stream.readInt();
    const auto numSamples = stream.readInt();

    if (numChannels <= 0 || numSamples < 0)
    {
        error = "Invalid HANSO audio chunk dimensions.";
        return false;
    }

    const auto bytesRequired = static_cast<juce::int64>(numChannels)
                             * numSamples
                             * static_cast<juce::int64>(sizeof(float));
    if (stream.getNumBytesRemaining() < bytesRequired)
    {
        error = "Truncated HANSO audio chunk.";
        return false;
    }

    buffer.setSize(numChannels, numSamples);
    for (int channel = 0; channel < numChannels; ++channel)
        stream.read(buffer.getWritePointer(channel), static_cast<int>(sizeof(float) * static_cast<size_t>(numSamples)));

    return true;
}

bool HansoAudioChunkCodec::decodePcm16Audio(const juce::MemoryBlock& data,
                                            juce::AudioBuffer<float>& buffer,
                                            double& sampleRate,
                                            juce::String& error)
{
    juce::MemoryInputStream stream(data, false);

    if (stream.readInt() != pcm16AudioChunkMagic)
    {
        error = "Invalid HANSO PCM16 audio chunk magic.";
        return false;
    }

    if (stream.readInt() != audioChunkVersion)
    {
        error = "Unsupported HANSO PCM16 audio chunk version.";
        return false;
    }

    sampleRate = stream.readDouble();
    const auto numChannels = stream.readInt();
    const auto numSamples = stream.readInt();

    if (numChannels <= 0 || numSamples < 0)
    {
        error = "Invalid HANSO PCM16 audio chunk dimensions.";
        return false;
    }

    const auto bytesRequired = static_cast<juce::int64>(numChannels)
                             * numSamples
                             * static_cast<juce::int64>(sizeof(short));
    if (stream.getNumBytesRemaining() < bytesRequired)
    {
        error = "Truncated HANSO PCM16 audio chunk.";
        return false;
    }

    buffer.setSize(numChannels, numSamples);
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* samples = buffer.getWritePointer(channel);
        for (int sample = 0; sample < numSamples; ++sample)
            samples[sample] = static_cast<float>(stream.readShort()) / 32768.0f;
    }

    return true;
}

bool HansoAudioChunkCodec::decodePcm24Audio(const juce::MemoryBlock& data,
                                            juce::AudioBuffer<float>& buffer,
                                            double& sampleRate,
                                            juce::String& error)
{
    juce::MemoryInputStream stream(data, false);

    if (stream.readInt() != pcm24AudioChunkMagic)
    {
        error = "Invalid HANSO PCM24 audio chunk magic.";
        return false;
    }

    if (stream.readInt() != audioChunkVersion)
    {
        error = "Unsupported HANSO PCM24 audio chunk version.";
        return false;
    }

    sampleRate = stream.readDouble();
    const auto numChannels = stream.readInt();
    const auto numSamples = stream.readInt();

    if (numChannels <= 0 || numSamples < 0)
    {
        error = "Invalid HANSO PCM24 audio chunk dimensions.";
        return false;
    }

    const auto bytesRequired = static_cast<juce::int64>(numChannels)
                             * numSamples
                             * 3;
    if (stream.getNumBytesRemaining() < bytesRequired)
    {
        error = "Truncated HANSO PCM24 audio chunk.";
        return false;
    }

    buffer.setSize(numChannels, numSamples);
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* samples = buffer.getWritePointer(channel);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto b0 = static_cast<juce::uint32>(stream.readByte()) & 0xffu;
            const auto b1 = static_cast<juce::uint32>(stream.readByte()) & 0xffu;
            const auto b2 = static_cast<juce::uint32>(stream.readByte()) & 0xffu;
            auto value = static_cast<juce::int32>(b0 | (b1 << 8) | (b2 << 16));
            if (value & 0x800000) // sign-extend from 24-bit
                value |= static_cast<juce::int32>(0xff000000);
            samples[sample] = static_cast<float>(value) / 8388608.0f;
        }
    }

    return true;
}

bool HansoAudioChunkCodec::decodeAudio(const juce::String& mediaType,
                                       const juce::MemoryBlock& data,
                                       juce::AudioBuffer<float>& buffer,
                                       double& sampleRate,
                                       juce::String& error)
{
    if (mediaType == mediaType::float32Audio)
        return decodeFloat32Audio(data, buffer, sampleRate, error);
    if (mediaType == mediaType::pcm24Audio)
        return decodePcm24Audio(data, buffer, sampleRate, error);
    if (mediaType == mediaType::pcm16Audio)
        return decodePcm16Audio(data, buffer, sampleRate, error);

    // Unknown/empty mediaType (legacy asset): dispatch on the binary magic.
    if (data.getSize() >= sizeof(int))
    {
        juce::MemoryInputStream probe(data, false);
        switch (probe.readInt())
        {
            case audioChunkMagic:      return decodeFloat32Audio(data, buffer, sampleRate, error);
            case pcm24AudioChunkMagic: return decodePcm24Audio(data, buffer, sampleRate, error);
            case pcm16AudioChunkMagic: return decodePcm16Audio(data, buffer, sampleRate, error);
            default: break;
        }
    }

    error = "Unrecognized HANSO audio chunk encoding: " + mediaType;
    return false;
}

const HansoBinaryChunk* HansoAudioChunkCodec::findAudioChunk(const HansoPackage& package,
                                                             const juce::String& preferredId)
{
    if (const auto* chunk = package.findChunk(preferredId))
        return chunk;

    // Retry under sibling encoding suffixes (e.g. an asset authored as .pcm16
    // when the producer now writes .f32, or vice versa).
    static const char* const suffixes[] = { ".f32", ".pcm24", ".pcm16" };
    for (const auto* suffix : suffixes)
    {
        if (! preferredId.endsWith(suffix))
            continue;

        const auto base = preferredId.dropLastCharacters(juce::String(suffix).length());
        for (const auto* candidate : suffixes)
        {
            if (juce::String(candidate) == suffix)
                continue;
            if (const auto* chunk = package.findChunk(base + candidate))
                return chunk;
        }
        break;
    }

    return nullptr;
}

bool HansoAudioChunkCodec::loadAudioChunk(const HansoPackage& package,
                                          const juce::String& preferredId,
                                          juce::AudioBuffer<float>& buffer,
                                          double& sampleRate,
                                          juce::String& error)
{
    const auto* chunk = findAudioChunk(package, preferredId);
    if (chunk == nullptr)
    {
        error = "Audio chunk not found: " + preferredId;
        return false;
    }

    return decodeAudio(chunk->mediaType, chunk->data, buffer, sampleRate, error);
}
}

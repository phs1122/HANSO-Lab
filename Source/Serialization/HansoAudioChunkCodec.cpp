#include "Serialization/HansoAudioChunkCodec.h"

namespace hanso
{
namespace
{
constexpr int audioChunkMagic = 0x48414632; // HAF2
constexpr int pcm16AudioChunkMagic = 0x48415031; // HAP1
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
}

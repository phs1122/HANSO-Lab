#pragma once

#include <JuceHeader.h>

namespace hanso
{
class HansoAudioChunkCodec final
{
public:
    static juce::MemoryBlock encodeFloat32Audio(const juce::AudioBuffer<float>& buffer, double sampleRate);
    static juce::MemoryBlock encodePcm16Audio(const juce::AudioBuffer<float>& buffer, double sampleRate);
    static bool decodeFloat32Audio(const juce::MemoryBlock& data,
                                   juce::AudioBuffer<float>& buffer,
                                   double& sampleRate,
                                   juce::String& error);
    static bool decodePcm16Audio(const juce::MemoryBlock& data,
                                 juce::AudioBuffer<float>& buffer,
                                 double& sampleRate,
                                 juce::String& error);
};
}

#pragma once

#include <JuceHeader.h>

namespace hanso
{
// Canonical mediaType strings for the audio chunk encodings.
//
// Storage tiers (see docs/HANSO_CONTAINER_FORMAT.md §6):
//   - float32 : master/analysis data (model-fit inputs, fidelity references,
//               de-embedding IRs) — conversion- and dither-free, clip-safe.
//   - pcm24   : playback-only data (runtime-baked audition assets) — leaner
//               distribution, no perceptible fidelity loss vs float32.
//   - pcm16   : legacy encoding, retained for reading pre-existing assets only.
namespace mediaType
{
inline constexpr const char* float32Audio = "audio/x-hanso-float32";
inline constexpr const char* pcm24Audio = "audio/x-hanso-pcm24";
inline constexpr const char* pcm16Audio = "audio/x-hanso-pcm16";
}

class HansoPackage;
struct HansoBinaryChunk;

class HansoAudioChunkCodec final
{
public:
    static juce::MemoryBlock encodeFloat32Audio(const juce::AudioBuffer<float>& buffer, double sampleRate);
    static juce::MemoryBlock encodePcm24Audio(const juce::AudioBuffer<float>& buffer, double sampleRate);
    static juce::MemoryBlock encodePcm16Audio(const juce::AudioBuffer<float>& buffer, double sampleRate);
    static bool decodeFloat32Audio(const juce::MemoryBlock& data,
                                   juce::AudioBuffer<float>& buffer,
                                   double& sampleRate,
                                   juce::String& error);
    static bool decodePcm24Audio(const juce::MemoryBlock& data,
                                 juce::AudioBuffer<float>& buffer,
                                 double& sampleRate,
                                 juce::String& error);
    static bool decodePcm16Audio(const juce::MemoryBlock& data,
                                 juce::AudioBuffer<float>& buffer,
                                 double& sampleRate,
                                 juce::String& error);

    // Encoding-agnostic decode: dispatches on the chunk's declared mediaType so
    // callers do not need to know how a chunk was stored. Falls back to the
    // legacy binary magic when mediaType is empty/unknown (old assets).
    static bool decodeAudio(const juce::String& mediaType,
                            const juce::MemoryBlock& data,
                            juce::AudioBuffer<float>& buffer,
                            double& sampleRate,
                            juce::String& error);

    // Look up an audio chunk by id, tolerating a changed encoding suffix:
    // tries preferredId first, then the sibling ".f32"/".pcm24"/".pcm16"
    // variants so packages authored under the old suffix still resolve.
    static const HansoBinaryChunk* findAudioChunk(const HansoPackage& package,
                                                  const juce::String& preferredId);

    // Convenience: findAudioChunk + decodeAudio(mediaType) in one call.
    static bool loadAudioChunk(const HansoPackage& package,
                               const juce::String& preferredId,
                               juce::AudioBuffer<float>& buffer,
                               double& sampleRate,
                               juce::String& error);
};
}

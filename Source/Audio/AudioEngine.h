#pragma once

#include <JuceHeader.h>

#include "App/ApplicationState.h"
#include "Audio/AudioDeviceController.h"
#include "Audio/CaptureAudioSource.h"
#include "Audio/InputChannelLayout.h"

namespace hanso
{
class AudioEngine final : private juce::AudioIODeviceCallback
{
public:
    explicit AudioEngine(ApplicationState& state);
    ~AudioEngine() override;

    void initialise();
    void shutdown();

    AudioDeviceController& deviceController() noexcept;
    CaptureAudioSource& captureSource() noexcept;
    double currentSampleRate() const noexcept;
    int currentBlockSize() const noexcept;

private:
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    ApplicationState& appState;
    juce::AudioDeviceManager deviceManager;
    AudioDeviceController controller;
    CaptureAudioSource captureSourceInstance;
    juce::AudioBuffer<float> callbackBuffer;
    InputChannelLayout inputLayout;
    double sampleRate { 48000.0 };
    int blockSize { 512 };
};
}

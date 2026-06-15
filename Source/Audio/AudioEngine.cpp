#include "Audio/AudioEngine.h"

namespace hanso
{
AudioEngine::AudioEngine(ApplicationState& state)
    : appState(state),
      controller(deviceManager)
{
}

AudioEngine::~AudioEngine()
{
    shutdown();
}

void AudioEngine::initialise()
{
    const auto error = controller.initialise();
    if (error.isNotEmpty())
        appState.appendLog("Audio initialization warning: " + error);

    deviceManager.addAudioCallback(this);
    appState.appendLog("Audio engine ready: " + controller.describeCurrentDevice());
}

void AudioEngine::shutdown()
{
    deviceManager.removeAudioCallback(this);
    captureSourceInstance.release();
}

AudioDeviceController& AudioEngine::deviceController() noexcept
{
    return controller;
}

CaptureAudioSource& AudioEngine::captureSource() noexcept
{
    return captureSourceInstance;
}

double AudioEngine::currentSampleRate() const noexcept
{
    return sampleRate;
}

int AudioEngine::currentBlockSize() const noexcept
{
    return blockSize;
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    sampleRate = device != nullptr ? device->getCurrentSampleRate() : 48000.0;
    blockSize = device != nullptr ? device->getCurrentBufferSizeSamples() : 512;
    const auto channelCount = device != nullptr ? device->getActiveOutputChannels().countNumberOfSetBits() : 2;
    if (device != nullptr)
    {
        inputLayout = InputChannelLayout::fromDevice(*device);
        captureSourceInstance.setInputChannelLayout(inputLayout);
        appState.appendLog("Input channel layout: " + inputLayout.describeActiveChannels());
    }

    callbackBuffer.setSize(juce::jmax(1, channelCount),
                           juce::jmax(1, blockSize),
                           false,
                           false,
                           true);
    captureSourceInstance.prepare(sampleRate, blockSize, callbackBuffer.getNumChannels());
}

void AudioEngine::audioDeviceStopped()
{
    captureSourceInstance.release();
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                   int numInputChannels,
                                                   float* const* outputChannelData,
                                                   int numOutputChannels,
                                                   int numSamples,
                                                   const juce::AudioIODeviceCallbackContext&)
{
    if (numOutputChannels > callbackBuffer.getNumChannels() || numSamples > callbackBuffer.getNumSamples())
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)
            if (outputChannelData[channel] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);

        return;
    }

    auto* writePointers = callbackBuffer.getArrayOfWritePointers();
    juce::AudioBuffer<float> blockBuffer(writePointers, numOutputChannels, numSamples);
    captureSourceInstance.process(inputChannelData, numInputChannels, blockBuffer);

    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::copy(outputChannelData[channel],
                                              blockBuffer.getReadPointer(channel),
                                              numSamples);
    }
}
}

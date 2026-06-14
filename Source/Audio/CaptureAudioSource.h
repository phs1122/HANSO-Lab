#pragma once

#include <JuceHeader.h>

#include "Capture/TestSignalGenerator.h"

namespace hanso
{
class CaptureAudioSource final
{
public:
    void prepare(double sampleRate, int maximumBlockSize, int outputChannels);
    void release();
    void setMonitoringEnabled(bool shouldMonitor) noexcept;
    bool isMonitoringEnabled() const noexcept;
    void setCaptureInputChannel(int zeroBasedChannelIndex) noexcept;
    int captureInputChannel() const noexcept;
    void setCaptureChannelCount(int channels) noexcept;
    int captureChannelCount() const noexcept;
    void loadCaptureSignal(const juce::AudioBuffer<float>& drySignal);
    bool startCapture() noexcept;
    void stopCapture() noexcept;
    bool isCaptureRunning() const noexcept;
    bool hasCaptureFinished() const noexcept;
    juce::AudioBuffer<float> copyCapturedSignal() const;
    float inputLevel() const noexcept;
    float outputLevel() const noexcept;
    void process(const float* const* inputChannelData,
                 int numInputChannels,
                 juce::AudioBuffer<float>& outputBuffer) noexcept;

private:
    bool monitoringEnabled { true };
    std::atomic<int> selectedInputChannel { -1 };
    std::atomic<int> selectedCaptureChannels { 2 };
    std::atomic<int> latestResolvedInputChannel { -1 };
    juce::AudioBuffer<float> playbackSignal;
    juce::AudioBuffer<float> capturedSignal;
    std::atomic<int> playhead { 0 };
    std::atomic<bool> captureRunning { false };
    std::atomic<bool> captureFinished { false };
    std::atomic<float> latestInputLevel { 0.0f };
    std::atomic<float> latestOutputLevel { 0.0f };
};
}

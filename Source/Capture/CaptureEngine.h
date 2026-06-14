#pragma once

#include <JuceHeader.h>

#include "App/ApplicationState.h"
#include "Audio/AudioEngine.h"
#include "Capture/CaptureSession.h"
#include "Capture/SignalAlignment.h"

namespace hanso
{
class CaptureEngine final
{
public:
    CaptureEngine(ApplicationState& state, AudioEngine& audioEngine);

    void generateTestSignal(TestSignalType type, double durationSeconds, float amplitude);
    void setCaptureInputChannel(int zeroBasedChannelIndex) noexcept;
    void setCaptureChannelCount(int channels) noexcept;
    int captureInputChannel() const noexcept;
    int captureChannelCount() const noexcept;
    void setMonitoringEnabled(bool enabled) noexcept;
    bool isMonitoringEnabled() const noexcept;
    void start();
    void stop();
    void reset();
    void refresh();
    void alignCompletedCapture();
    CaptureSessionState state() const noexcept;
    juce::String stateText() const;
    float inputLevel() const noexcept;
    float outputLevel() const noexcept;
    float capturedPeakDb() const noexcept;
    float capturedRmsDb() const noexcept;
    juce::String captureStorageText() const;
    double captureDurationSeconds() const noexcept;
    double estimatedLatencyMs() const noexcept;
    int estimatedLatencySamples() const noexcept;
    const CaptureSession& currentSession() const noexcept;
    bool hasCompletedCapture() const noexcept;

private:
    ApplicationState& appState;
    AudioEngine& audio;
    CaptureSession session;
    TestSignalGenerator generator;
    SignalAlignment alignment;
};
}

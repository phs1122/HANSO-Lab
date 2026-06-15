#pragma once

#include <JuceHeader.h>

#include "App/ApplicationState.h"
#include "Analysis/CaptureQualityAnalyzer.h"
#include "Audio/AudioEngine.h"
#include "Capture/CaptureSession.h"
#include "Capture/EasyCaptureSafetyPolicy.h"
#include "Capture/SignalAlignment.h"
#include "Model/CompactHansoModel.h"

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
    void startCaptureStep(const juce::String& stepId);
    void completeNonAudioStep(const juce::String& stepId);
    void resetCaptureStep(const juce::String& stepId);
    void setCalibrationOutputDb(float dbFs);
    float calibrationOutputDb() const noexcept;
    void startCalibrationMonitor();
    void stopCalibrationMonitor();
    void stopOutputSignal();
    void updateLiveCalibration();
    bool isCalibrationMonitorRunning() const noexcept;
    bool isCalibrationLevelSafe() const noexcept;
    bool isCalibrationOutputLevelSafe() const noexcept;
    void stop();
    void reset();
    void refresh();
    void alignCompletedCapture();
    CaptureSessionState state() const noexcept;
    juce::String activeCaptureStepId() const;
    double captureProgress() const noexcept;
    juce::String stateText() const;
    float inputLevel() const noexcept;
    float outputLevel() const noexcept;
    float capturedPeakDb() const noexcept;
    float capturedRmsDb() const noexcept;
    juce::String captureStorageText() const;
    double captureDurationSeconds() const noexcept;
    double estimatedLatencyMs() const noexcept;
    int estimatedLatencySamples() const noexcept;
    double currentSampleRate() const noexcept;
    const CaptureSession& currentSession() const noexcept;
    bool hasCompletedCapture() const noexcept;
    bool loadPreviewModel(const CompactHansoModel& model);
    void setPreviewGainPercent(float percent);
    void clearPreviewModel();
    const CompactHansoModel* currentPreviewModel() const noexcept;
    int previewModelRevision() const noexcept;
    void loadPreviewSample(const juce::AudioBuffer<float>& sample);
    bool startPreviewSample();
    void stopPreviewSample();
    bool isPreviewSamplePlaying() const noexcept;
    double previewSampleProgress() const noexcept;
    void seekPreviewSample(double normalizedProgress);
    void setPreviewLoopEnabled(bool enabled);
    bool isPreviewLoopEnabled() const noexcept;
    void setPreviewOutputGainDb(float gainDb);
    float previewOutputGainDb() const noexcept;
    void setPreviewNormalizationEnabled(bool enabled);
    bool isPreviewNormalizationEnabled() const noexcept;
    void setPreviewCabinetEnabled(bool enabled);
    bool isPreviewCabinetEnabled() const noexcept;
    bool hasPreviewModel() const noexcept;
    juce::String previewModelSummary() const;

private:
    ApplicationState& appState;
    AudioEngine& audio;
    CaptureSession session;
    TestSignalGenerator generator;
    SignalAlignment alignment;
    CaptureQualityAnalyzer qualityAnalyzer;
    EasyCaptureSafetyPolicy easyPolicy;
    juce::String activeStepId;
    float userCalibrationOutputDb { -33.0f };
    int calibrationSafeTicks { 0 };
    CompactHansoModel previewModel;
    bool previewModelLoaded { false };
    int previewRevision { 0 };
};
}

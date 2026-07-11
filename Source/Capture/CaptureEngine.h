#pragma once

#include <JuceHeader.h>

#include "App/ApplicationState.h"
#include "Analysis/CaptureQualityAnalyzer.h"
#include "Audio/AudioEngine.h"
#include "Capture/CaptureSession.h"
#include "Capture/EasyCaptureSafetyPolicy.h"
#include "Capture/SignalAlignment.h"
#include "Model/CompactHansoModel.h"
#include "Model/HansoPackage.h"
#include "Model/MicColoration.h"

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
    bool importCabinetIrForStep(const juce::String& stepId,
                                const juce::File& file,
                                CabinetMicClass micClass = CabinetMicClass::Unknown,
                                const juce::String& micModelName = {});
    bool buildCabinetFromSlots();
    void setCalibrationOutputDb(float dbFs);
    float calibrationOutputDb() const noexcept;
    // Mode-adjusted output level actually emitted (Easy safety gate / Standard cap).
    float effectiveOutputDb() const noexcept;
    void startCalibrationMonitor();
    void stopCalibrationMonitor();
    void stopOutputSignal();
    void updateLiveCalibration();
    bool isCalibrationMonitorRunning() const noexcept;
    bool isCalibrationLevelSafe() const noexcept;
    bool isCalibrationOutputLevelSafe() const noexcept;
    juce::String calibrationStatusText() const;
    float calibrationNoiseFloorDbfs() const noexcept;
    float calibrationSignalToNoiseDb() const noexcept;
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
    // Pedal slot of the preview rig (rendered in front of the amp slot).
    bool loadPreviewPedalModel(const CompactHansoModel& model);
    void clearPreviewPedalModel();
    bool hasPreviewPedalModel() const noexcept;
    juce::String previewPedalSummary() const;
    void setPreviewGainPercent(float percent);
    void clearPreviewModel();
    bool loadPreviewCabinetPackage(const HansoPackage& package);
    void setPreviewMicPositionPercent(float percent);
    void setPreviewCabinetMicDistanceCm(float distanceCm);
    void setPreviewCabinetMicClass(CabinetMicClass micClass);
    bool previewCabinetHasMicMatrix() const noexcept;
    juce::String previewCabinetSummary() const;
    // Complement cab for model previews: user-chosen cabinet .hanso (IR) as
    // an alternative to the built-in standard EQ. Session-scoped.
    bool loadPreviewComplementCabFile(const juce::File& file);
    void setPreviewComplementCabUseCustom(bool useCustom);
    bool previewComplementCabUseCustom() const noexcept;
    bool hasPreviewComplementCabPackage() const noexcept;
    juce::String previewComplementCabSummary() const;
    void clearPreviewCabinetPackage();
    bool hasPreviewCabinetPackage() const noexcept;
    int previewCabinetRevision() const noexcept;
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
    float modeAdjustedOutputDb() const noexcept;

    enum class CalibrationMonitorPhase
    {
        Idle,
        MeasuringNoise,
        Probing,
        Blocked
    };

    ApplicationState& appState;
    AudioEngine& audio;
    CaptureSession session;
    TestSignalGenerator generator;
    SignalAlignment alignment;
    CaptureQualityAnalyzer qualityAnalyzer;
    EasyCaptureSafetyPolicy easyPolicy;
    juce::String activeStepId;
    // Sample-injection layout of the active anchor capture: the composite
    // output is primary probe + gaps + preview samples, and these offsets let
    // refresh() slice the aligned recording back into per-sample chunks.
    struct ActiveSampleSegment
    {
        juce::String id;
        int startSample { 0 };
        int numSamples { 0 };
    };
    std::vector<ActiveSampleSegment> activeSampleSegments;
    std::vector<HansoProbeSegment> activeProbeSegments;
    int activePrimarySignalSamples { 0 };
    float userCalibrationOutputDb { -33.0f };
    int calibrationSafeTicks { 0 };
    CalibrationMonitorPhase calibrationPhase { CalibrationMonitorPhase::Idle };
    double calibrationPhaseStartMs { 0.0 };
    double calibrationNoisePowerSum { 0.0 };
    int calibrationNoiseMeasurements { 0 };
    float calibrationNoiseFloorDbfsValue { -120.0f };
    float calibrationSignalToNoiseDbValue { -120.0f };
    float calibrationToneDominanceDb { -120.0f };
    // Mute-drop identity state (distortion-robust rescue when Goertzel tone
    // dominance fails on a heavily saturated but honest return).
    bool calibrationMuteWindowActive { false };
    double calibrationMuteStartMs { 0.0 };
    double calibrationLastMuteProbeMs { 0.0 };
    double calibrationMuteSettleUntilMs { 0.0 };
    float calibrationPreMuteReturnDbfs { -120.0f };
    bool calibrationMuteDropIdentityOk { false };
    double calibrationMuteDropValidUntilMs { 0.0 };
    juce::String calibrationLiveStatusText;
    juce::String calibrationLastLogCode;
    CompactHansoModel previewModel;
    bool previewModelLoaded { false };
    int previewRevision { 0 };
    int previewCabinetRevisionCounter { 0 };
    bool previewCabinetLoaded { false };
};
}

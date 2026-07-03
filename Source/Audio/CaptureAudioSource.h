#pragma once

#include <JuceHeader.h>

#include "Audio/InputChannelLayout.h"
#include "Audio/PreviewCabinetIrProcessor.h"
#include "Audio/PreviewCabinetProcessor.h"
#include "Audio/PreviewModelProcessor.h"
#include "Capture/OutputRoutingPolicy.h"
#include "Capture/TestSignalGenerator.h"
#include "Model/CompactHansoModel.h"
#include "Model/HansoPackage.h"

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
    void setOutputRoutingMode(OutputRoutingMode mode) noexcept;
    OutputRoutingMode outputRoutingMode() const noexcept;
    void setInputChannelLayout(const InputChannelLayout& layout);
    void loadCaptureSignal(const juce::AudioBuffer<float>& drySignal);
    void loadCalibrationSignal(const juce::AudioBuffer<float>& signal);
    void replaceCalibrationSignal(const juce::AudioBuffer<float>& signal);
    bool startCapture() noexcept;
    void stopCapture() noexcept;
    bool startCalibrationSignal() noexcept;
    void stopCalibrationSignal() noexcept;
    bool isCalibrationSignalRunning() const noexcept;
    bool isCaptureRunning() const noexcept;
    double captureProgress() const noexcept;
    bool hasCaptureFinished() const noexcept;
    juce::AudioBuffer<float> copyCapturedSignal() const;
    bool loadPreviewModel(const CompactHansoModel& model);
    void setPreviewGainPercent(float percent) noexcept;
    void clearPreviewModel() noexcept;
    bool loadPreviewCabinetPackage(const HansoPackage& package, juce::String& error);
    void setPreviewMicPositionNormalized(float normalizedPosition) noexcept;
    void clearPreviewCabinetPackage() noexcept;
    bool hasPreviewCabinetPackage() const noexcept;
    juce::String previewCabinetSummary() const;
    void loadPreviewSample(const juce::AudioBuffer<float>& sample);
    bool startPreviewSample() noexcept;
    void stopPreviewSample() noexcept;
    bool isPreviewSamplePlaying() const noexcept;
    double previewSampleProgress() const noexcept;
    void seekPreviewSample(double normalizedProgress) noexcept;
    void setPreviewLoopEnabled(bool enabled) noexcept;
    bool isPreviewLoopEnabled() const noexcept;
    void setPreviewOutputGainDb(float gainDb) noexcept;
    float previewOutputGainDb() const noexcept;
    void setPreviewNormalizationEnabled(bool enabled) noexcept;
    bool isPreviewNormalizationEnabled() const noexcept;
    void setPreviewCabinetEnabled(bool enabled) noexcept;
    bool isPreviewCabinetEnabled() const noexcept;
    bool hasPreviewModel() const noexcept;
    juce::String previewModelSummary() const;
    float inputLevel() const noexcept;
    float outputLevel() const noexcept;
    juce::AudioBuffer<float> copyRecentInputSignal(int maxSamples) const;
    void clearRecentInputHistory() noexcept;
    void process(const float* const* inputChannelData,
                 int numInputChannels,
                 juce::AudioBuffer<float>& outputBuffer) noexcept;

private:
    bool monitoringEnabled { false };
    std::atomic<int> selectedInputChannel { -1 };
    std::atomic<int> selectedCaptureChannels { 2 };
    std::atomic<int> selectedOutputRouting { static_cast<int>(OutputRoutingMode::Default) };
    std::atomic<int> latestResolvedInputChannel { -1 };
    InputChannelLayout inputLayout;
    juce::AudioBuffer<float> playbackSignal;
    juce::AudioBuffer<float> calibrationSignal;
    juce::AudioBuffer<float> capturedSignal;
    juce::AudioBuffer<float> previewSampleSignal;
    juce::AudioBuffer<float> previewScratchBuffer;
    PreviewModelProcessor previewProcessor;
    PreviewCabinetProcessor cabinetProcessor;
    PreviewCabinetIrProcessor cabinetIrProcessor;
    std::atomic<int> playhead { 0 };
    std::atomic<int> calibrationPlayhead { 0 };
    std::atomic<bool> captureRunning { false };
    std::atomic<bool> captureFinished { false };
    std::atomic<bool> calibrationRunning { false };
    std::atomic<bool> previewSampleRunning { false };
    std::atomic<int> previewSamplePlayhead { 0 };
    std::atomic<float> latestInputLevel { 0.0f };
    std::atomic<float> latestOutputLevel { 0.0f };
    std::unique_ptr<std::atomic<float>[]> recentInputRing;
    int recentInputCapacity { 0 };
    std::atomic<int> recentInputWritePosition { 0 };
    std::atomic<int> recentInputValidSamples { 0 };

    void writeRecentInputBlock(const float* input, int numSamples) noexcept;
    void applyPreviewMonitoring(juce::AudioBuffer<float>& outputBuffer, int numSamples) noexcept;

    std::atomic<bool> previewLoopEnabled { false };
    std::atomic<bool> previewNormalizationEnabled { true };
    std::atomic<bool> previewCabinetEnabled { true };
    std::atomic<float> previewOutputGainDbValue { 0.0f };
    float currentPreviewOutputGain { 1.0f };
    float currentPreviewNormalizationGain { 1.0f };
};
}

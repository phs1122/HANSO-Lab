#pragma once

#include <JuceHeader.h>

#include "Capture/TestSignalGenerator.h"
#include "Model/AnalysisSummary.h"
#include "Model/CaptureMetadata.h"

namespace hanso
{
enum class CaptureSessionState
{
    Idle,
    Armed,
    Recording,
    Completed
};

class CaptureSession final
{
public:
    void createNew(double sampleRate, int numChannels, const TestSignalSpec& spec, juce::AudioBuffer<float> drySignal);
    void begin();
    void finish();
    void reset();
    void storeCapturedSignal(const juce::AudioBuffer<float>& captured);
    void storeAlignedCapturedSignal(juce::AudioBuffer<float> aligned, int latencySamples);
    void setAnalysisSummary(const AnalysisSummary& summary);
    void setExportPath(juce::File file);

    CaptureSessionState state() const noexcept;
    double getSampleRate() const noexcept;
    int getNumChannels() const noexcept;
    juce::String getSessionId() const noexcept;
    const TestSignalSpec& getTestSignalSpec() const noexcept;
    const juce::AudioBuffer<float>& dryReferenceSignal() const noexcept;
    const juce::AudioBuffer<float>& capturedSignal() const noexcept;
    const juce::AudioBuffer<float>& alignedCapturedSignal() const noexcept;
    int estimatedLatencySamples() const noexcept;
    double estimatedLatencyMilliseconds() const noexcept;
    const AnalysisSummary& analysisSummary() const noexcept;
    bool hasCapture() const noexcept;
    bool isExported() const noexcept;
    juce::File exportPath() const;

private:
    CaptureSessionState sessionState { CaptureSessionState::Idle };
    juce::String sessionId;
    double captureSampleRate { 48000.0 };
    int captureChannels { 1 };
    TestSignalSpec signalSpec;
    juce::AudioBuffer<float> drySignalBuffer;
    juce::AudioBuffer<float> capturedSignalBuffer;
    juce::AudioBuffer<float> alignedCapturedSignalBuffer;
    int latencySamples { 0 };
    AnalysisSummary summary;
    bool exported { false };
    juce::File lastExportPath;
};
}

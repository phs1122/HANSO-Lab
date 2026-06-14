#include "Capture/CaptureSession.h"

namespace hanso
{
void CaptureSession::createNew(double newSampleRate, int numChannels, const TestSignalSpec& spec, juce::AudioBuffer<float> drySignal)
{
    captureSampleRate = newSampleRate;
    captureChannels = juce::jmax(1, numChannels);
    signalSpec = spec;
    drySignalBuffer = std::move(drySignal);
    capturedSignalBuffer.setSize(captureChannels, drySignalBuffer.getNumSamples(), false, true, true);
    alignedCapturedSignalBuffer.setSize(captureChannels, 0);
    latencySamples = 0;
    summary = {};
    exported = false;
    lastExportPath = juce::File();
    sessionId = juce::Uuid().toString();
    sessionState = CaptureSessionState::Armed;
}

void CaptureSession::begin()
{
    if (sessionState == CaptureSessionState::Armed)
        sessionState = CaptureSessionState::Recording;
}

void CaptureSession::finish()
{
    if (sessionState == CaptureSessionState::Recording)
        sessionState = CaptureSessionState::Completed;
}

void CaptureSession::reset()
{
    sessionState = CaptureSessionState::Idle;
    drySignalBuffer.setSize(0, 0);
    capturedSignalBuffer.setSize(0, 0);
    alignedCapturedSignalBuffer.setSize(0, 0);
}

void CaptureSession::storeCapturedSignal(const juce::AudioBuffer<float>& captured)
{
    capturedSignalBuffer.makeCopyOf(captured, true);
}

void CaptureSession::storeAlignedCapturedSignal(juce::AudioBuffer<float> aligned, int newLatencySamples)
{
    alignedCapturedSignalBuffer = std::move(aligned);
    latencySamples = newLatencySamples;
}

void CaptureSession::setAnalysisSummary(const AnalysisSummary& newSummary)
{
    summary = newSummary;
}

void CaptureSession::setExportPath(juce::File file)
{
    lastExportPath = std::move(file);
    exported = true;
}

CaptureSessionState CaptureSession::state() const noexcept
{
    return sessionState;
}

double CaptureSession::getSampleRate() const noexcept
{
    return captureSampleRate;
}

int CaptureSession::getNumChannels() const noexcept
{
    return captureChannels;
}

juce::String CaptureSession::getSessionId() const noexcept
{
    return sessionId;
}

const TestSignalSpec& CaptureSession::getTestSignalSpec() const noexcept
{
    return signalSpec;
}

const juce::AudioBuffer<float>& CaptureSession::dryReferenceSignal() const noexcept
{
    return drySignalBuffer;
}

const juce::AudioBuffer<float>& CaptureSession::capturedSignal() const noexcept
{
    return capturedSignalBuffer;
}

const juce::AudioBuffer<float>& CaptureSession::alignedCapturedSignal() const noexcept
{
    return alignedCapturedSignalBuffer;
}

int CaptureSession::estimatedLatencySamples() const noexcept
{
    return latencySamples;
}

double CaptureSession::estimatedLatencyMilliseconds() const noexcept
{
    return captureSampleRate > 0.0 ? 1000.0 * static_cast<double>(latencySamples) / captureSampleRate : 0.0;
}

const AnalysisSummary& CaptureSession::analysisSummary() const noexcept
{
    return summary;
}

bool CaptureSession::hasCapture() const noexcept
{
    return capturedSignalBuffer.getNumSamples() > 0 && sessionState == CaptureSessionState::Completed;
}

bool CaptureSession::isExported() const noexcept
{
    return exported;
}

juce::File CaptureSession::exportPath() const
{
    return lastExportPath;
}
}

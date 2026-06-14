#include "Capture/CaptureEngine.h"

#include "Audio/AudioMetrics.h"

namespace hanso
{
CaptureEngine::CaptureEngine(ApplicationState& state, AudioEngine& audioEngine)
    : appState(state),
      audio(audioEngine)
{
}

void CaptureEngine::generateTestSignal(TestSignalType type, double durationSeconds, float amplitude)
{
    if (session.state() == CaptureSessionState::Recording)
    {
        appState.appendLog("Cannot generate a new signal while capture is recording.");
        return;
    }

    TestSignalSpec spec;
    spec.type = type;
    spec.sampleRate = audio.currentSampleRate();
    spec.durationSeconds = durationSeconds;
    spec.amplitude = amplitude;

    auto drySignal = generator.generate(spec);
    audio.captureSource().loadCaptureSignal(drySignal);
    const auto captureChannels = audio.captureSource().captureChannelCount();
    session.createNew(audio.currentSampleRate(), captureChannels, spec, std::move(drySignal));

    auto& package = appState.currentPackage();
    package.metadata.sampleRate = spec.sampleRate;
    package.metadata.numInputChannels = captureChannels;
    package.metadata.numOutputChannels = 1;
    package.captureSettings.sampleRate = spec.sampleRate;
    package.captureSettings.bufferSize = audio.currentBlockSize();
    package.captureSettings.inputChannels = captureChannels;
    package.captureSettings.outputChannels = 1;
    package.captureSettings.durationSeconds = spec.durationSeconds;
    package.captureSettings.testSignalType = TestSignalGenerator::toString(spec.type);

    appState.appendLog("Generated test signal: " + TestSignalGenerator::toString(type));
}

void CaptureEngine::setCaptureInputChannel(int zeroBasedChannelIndex) noexcept
{
    audio.captureSource().setCaptureInputChannel(zeroBasedChannelIndex);
}

void CaptureEngine::setCaptureChannelCount(int channels) noexcept
{
    audio.captureSource().setCaptureChannelCount(channels);
}

int CaptureEngine::captureInputChannel() const noexcept
{
    return audio.captureSource().captureInputChannel();
}

int CaptureEngine::captureChannelCount() const noexcept
{
    return audio.captureSource().captureChannelCount();
}

void CaptureEngine::setMonitoringEnabled(bool enabled) noexcept
{
    audio.captureSource().setMonitoringEnabled(enabled);
}

bool CaptureEngine::isMonitoringEnabled() const noexcept
{
    return audio.captureSource().isMonitoringEnabled();
}

void CaptureEngine::start()
{
    if (session.state() == CaptureSessionState::Completed)
    {
        appState.appendLog("Capture already completed. Reset or generate a new signal before overwriting.");
        return;
    }

    if (session.state() != CaptureSessionState::Armed)
    {
        appState.appendLog("Generate a test signal before starting capture.");
        return;
    }

    if (! audio.captureSource().startCapture())
    {
        appState.appendLog("Capture could not start because no test signal is loaded.");
        return;
    }

    session.begin();
    appState.appendLog("Capture recording started.");
}

void CaptureEngine::stop()
{
    audio.captureSource().stopCapture();
    if (session.state() == CaptureSessionState::Recording)
        session.storeCapturedSignal(audio.captureSource().copyCapturedSignal());

    session.finish();
    appState.appendLog("Capture recording completed.");
}

void CaptureEngine::reset()
{
    session.reset();
    audio.captureSource().stopCapture();
    appState.appendLog("Capture reset.");
}

void CaptureEngine::refresh()
{
    if (session.state() == CaptureSessionState::Recording && audio.captureSource().hasCaptureFinished())
    {
        session.storeCapturedSignal(audio.captureSource().copyCapturedSignal());
        session.finish();
        appState.appendLog("Capture completed at end of test signal. Captured peak: "
                           + juce::String(capturedPeakDb(), 1) + " dBFS, RMS: "
                           + juce::String(capturedRmsDb(), 1) + " dBFS.");

        if (capturedPeakDb() <= -90.0f)
            appState.appendLog("Captured signal is near silence. Check input channel selection and Babyface input meter.");
    }
}

void CaptureEngine::alignCompletedCapture()
{
    if (! session.hasCapture())
    {
        appState.appendLog("No completed capture to align.");
        return;
    }

    auto result = alignment.estimateOffset(session.dryReferenceSignal(),
                                           session.capturedSignal(),
                                           session.getSampleRate());
    session.storeAlignedCapturedSignal(std::move(result.alignedCaptured), result.estimatedOffsetSamples);

    auto& package = appState.currentPackage();
    package.analysisSummary.estimatedLatencySamples = result.estimatedOffsetSamples;
    package.analysisSummary.estimatedLatencyMs = result.estimatedOffsetMs;
    package.dynamicProfile.estimatedLatencySeconds = static_cast<float>(result.estimatedOffsetMs / 1000.0);

    appState.appendLog("Alignment completed: " + juce::String(result.estimatedOffsetSamples)
                       + " samples / " + juce::String(result.estimatedOffsetMs, 2) + " ms.");
}

CaptureSessionState CaptureEngine::state() const noexcept
{
    return session.state();
}

juce::String CaptureEngine::stateText() const
{
    switch (session.state())
    {
        case CaptureSessionState::Idle: return "Idle";
        case CaptureSessionState::Armed: return "Armed";
        case CaptureSessionState::Recording: return "Recording";
        case CaptureSessionState::Completed: return "Completed";
    }

    return "Unknown";
}

float CaptureEngine::inputLevel() const noexcept
{
    return audio.captureSource().inputLevel();
}

float CaptureEngine::outputLevel() const noexcept
{
    return audio.captureSource().outputLevel();
}

float CaptureEngine::capturedPeakDb() const noexcept
{
    return peakDb(session.capturedSignal());
}

float CaptureEngine::capturedRmsDb() const noexcept
{
    return rmsDb(session.capturedSignal());
}

juce::String CaptureEngine::captureStorageText() const
{
    const auto& captured = session.capturedSignal();
    if (session.state() == CaptureSessionState::Idle)
        return "Stored capture: none";

    if (session.state() == CaptureSessionState::Armed)
        return "Stored capture: waiting";

    if (session.state() == CaptureSessionState::Recording)
        return "Stored capture: recording";

    return "Stored capture: "
         + juce::String(captured.getNumChannels()) + " ch / "
         + juce::String(captured.getNumSamples()) + " samples / Peak "
         + juce::String(capturedPeakDb(), 1) + " dBFS / RMS "
         + juce::String(capturedRmsDb(), 1) + " dBFS";
}

double CaptureEngine::captureDurationSeconds() const noexcept
{
    return session.getTestSignalSpec().durationSeconds;
}

double CaptureEngine::estimatedLatencyMs() const noexcept
{
    return session.estimatedLatencyMilliseconds();
}

int CaptureEngine::estimatedLatencySamples() const noexcept
{
    return session.estimatedLatencySamples();
}

const CaptureSession& CaptureEngine::currentSession() const noexcept
{
    return session;
}

bool CaptureEngine::hasCompletedCapture() const noexcept
{
    return session.hasCapture();
}
}

#include "Capture/CaptureEngine.h"

#include "Analysis/CalibrationValidator.h"
#include "Analysis/CabinetToneProfiler.h"
#include "App/Utf8.h"
#include "Audio/AudioMetrics.h"
#include "Capture/CabinetIrImporter.h"
#include "Capture/CabinetMessages.h"
#include "Capture/CaptureStepUtils.h"
#include "Serialization/HansoAudioChunkCodec.h"

namespace hanso
{
namespace
{
constexpr float calibrationInputMinDb = -36.0f;
constexpr float calibrationInputMaxDb = -8.0f;
constexpr float calibrationOutputMinDb = -42.0f;
constexpr float calibrationOutputMaxDb = -24.0f;
constexpr float calibrationRequiredSnrDb = 12.0f;
constexpr float calibrationRequiredToneDominanceDb = 6.0f;
constexpr float calibrationSilentLoopbackThresholdDb = -50.0f;
constexpr double calibrationSilentMeasureSeconds = 1.0;
constexpr double calibrationRecentWindowSeconds = 0.75;
constexpr double calibrationLoopbackGraceMs = 200.0;

double dbToPower(float db) noexcept
{
    if (db <= -120.0f)
        return 0.0;

    const auto gain = juce::Decibels::decibelsToGain(db);
    return static_cast<double>(gain) * static_cast<double>(gain);
}

float powerToDb(double power) noexcept
{
    if (power <= 1.0e-24)
        return -120.0f;

    return juce::Decibels::gainToDecibels(static_cast<float>(std::sqrt(power)), -120.0f);
}

juce::AudioBuffer<float> makeSilentCalibrationSignal(double sampleRate)
{
    juce::AudioBuffer<float> buffer(1, juce::jmax(1, static_cast<int>(sampleRate * calibrationSilentMeasureSeconds)));
    buffer.clear();
    return buffer;
}

juce::AudioBuffer<float> makeProbeCalibrationSignal(const TestSignalGenerator& generator,
                                                    double sampleRate,
                                                    float outputDbfs)
{
    TestSignalSpec spec;
    spec.type = TestSignalType::MultiSine;
    spec.sampleRate = sampleRate;
    spec.durationSeconds = 1.0;
    spec.amplitude = juce::Decibels::decibelsToGain(outputDbfs);
    return generator.generate(spec);
}

CalibrationValidationConfig calibrationValidationConfig() noexcept
{
    CalibrationValidationConfig config;
    config.inputMinDbfs = calibrationInputMinDb;
    config.inputMaxDbfs = calibrationInputMaxDb;
    config.outputMinDbfs = calibrationOutputMinDb;
    config.outputMaxDbfs = calibrationOutputMaxDb;
    config.silentReturnLoopbackThresholdDbfs = calibrationSilentLoopbackThresholdDb;
    config.requiredSnrDb = calibrationRequiredSnrDb;
    config.requiredToneDominanceDb = calibrationRequiredToneDominanceDb;
    return config;
}

bool isCabinetWorkflow(const CaptureWizardState& wizard) noexcept
{
    return wizard.captureType == CaptureType::Cabinet;
}

void configurePackageForCaptureType(HansoPackage& package, const CaptureWizardState& wizard)
{
    package.formatVersion = 2;
    package.metadata.category = categoryForCaptureType(wizard.captureType);
    package.metadata.deviceType = toString(wizard.captureType);
}

void unlockPostCalibrationSteps(CaptureWizardState& wizard)
{
    if (wizard.captureType == CaptureType::Cabinet)
    {
        for (auto& step : wizard.recipe.steps)
            if (isCabinetPositionStep(step) && step.status == CaptureStepStatus::NotStarted)
                step.status = CaptureStepStatus::Ready;
        return;
    }

    if (auto* next = wizard.findStep("gain-010"))
        if (next->status == CaptureStepStatus::NotStarted)
            next->status = CaptureStepStatus::Ready;
}

CaptureStepStatus statusFromQuality(const CaptureQualityReport& report) noexcept
{
    if (report.overallSeverity == CaptureQualitySeverity::Error)
        return CaptureStepStatus::Failed;
    if (report.overallSeverity == CaptureQualitySeverity::Warning)
        return CaptureStepStatus::Warning;
    return CaptureStepStatus::Passed;
}

CaptureQualityTarget qualityTargetForStep(const CaptureStep& step)
{
    if (! step.isAnchorCapture() || step.anchor.parameterKey != "gain")
        return {};

    if (step.anchor.normalizedValue <= 0.15f)
        return { -72.0f, -12.0f, "Gain 10%", 18.0f, true };

    if (step.anchor.normalizedValue >= 0.85f)
        return { -36.0f, -3.0f, "Gain 100%" };

    return { -42.0f, -8.0f, "Gain 50%" };
}

void setAudioChunk(HansoPackage& package,
                   const juce::String& id,
                   const juce::String& role,
                   const juce::AudioBuffer<float>& buffer,
                   double sampleRate)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    package.setChunk(id,
                     role,
                     "audio/x-hanso-float32",
                     HansoAudioChunkCodec::encodeFloat32Audio(buffer, sampleRate),
                     sampleRate,
                     buffer.getNumChannels(),
                     buffer.getNumSamples());
}

void setPcm16AudioChunk(HansoPackage& package,
                        const juce::String& id,
                        const juce::String& role,
                        const juce::AudioBuffer<float>& buffer,
                        double sampleRate)
{
    if (id.isEmpty() || buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    package.setChunk(id,
                     role,
                     "audio/x-hanso-pcm16",
                     HansoAudioChunkCodec::encodePcm16Audio(buffer, sampleRate),
                     sampleRate,
                     buffer.getNumChannels(),
                     buffer.getNumSamples());
}
}

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
    configurePackageForCaptureType(package, appState.captureWizard());
    package.clearChunks();
    package.metadata.sampleRate = spec.sampleRate;
    package.metadata.numInputChannels = captureChannels;
    package.metadata.numOutputChannels = 1;
    package.captureSettings.sampleRate = spec.sampleRate;
    package.captureSettings.bufferSize = audio.currentBlockSize();
    package.captureSettings.inputChannels = captureChannels;
    package.captureSettings.outputChannels = 1;
    package.captureSettings.durationSeconds = spec.durationSeconds;
    package.captureSettings.testSignalType = TestSignalGenerator::toString(spec.type);
    setPcm16AudioChunk(package, "capture/dry-reference.pcm16", "dryReference", session.dryReferenceSignal(), spec.sampleRate);

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

void CaptureEngine::startCaptureStep(const juce::String& stepId)
{
    auto& wizard = appState.captureWizard();
    auto* step = wizard.findStep(stepId);
    if (step == nullptr)
    {
        appState.appendLog("Unknown capture step: " + stepId);
        return;
    }

    wizard.currentStepId = stepId;

    if (stepId == "calibration")
    {
        startCalibrationMonitor();
        return;
    }

    if (! step->isAnchorCapture())
    {
        completeNonAudioStep(stepId);
        return;
    }

    if (wizard.mode == CaptureMode::Easy && step->isAnchorCapture() && ! wizard.calibrationPassed)
    {
        appState.appendLog("Easy Capture calibration must pass before audio capture.");
        return;
    }

    const auto routing = OutputRoutingPolicy::forCaptureMode(wizard.mode);
    audio.captureSource().setOutputRoutingMode(routing);

    const auto isCalibration = stepId == "calibration";

    // Capture output follows the level dialed in during Calibration, so the
    // level you verify as safe is exactly the level the real captures use.
    // Easy mode keeps its hard safety ceiling on top of that.
    const auto calibratedAmplitude = juce::Decibels::decibelsToGain(userCalibrationOutputDb);
    const auto amplitude = wizard.mode == CaptureMode::Easy
                         ? easyPolicy.limitAmplitude(calibratedAmplitude)
                         : calibratedAmplitude;

    TestSignalSpec spec;
    spec.type = TestSignalType::LogSineSweep;
    spec.sampleRate = audio.currentSampleRate();
    spec.durationSeconds = isCalibration ? 2.0 : 5.0;
    spec.amplitude = amplitude;

    auto drySignal = generator.generate(spec);
    audio.captureSource().loadCaptureSignal(drySignal);
    const auto captureChannels = audio.captureSource().captureChannelCount();
    session.createNew(audio.currentSampleRate(), captureChannels, spec, std::move(drySignal));

    auto& package = appState.currentPackage();
    configurePackageForCaptureType(package, wizard);
    package.metadata.sampleRate = spec.sampleRate;
    package.metadata.numInputChannels = captureChannels;
    package.metadata.numOutputChannels = wizard.mode == CaptureMode::Easy ? 1 : 2;
    package.captureSettings.sampleRate = spec.sampleRate;
    package.captureSettings.bufferSize = audio.currentBlockSize();
    package.captureSettings.inputChannels = captureChannels;
    package.captureSettings.outputChannels = package.metadata.numOutputChannels;
    package.captureSettings.durationSeconds = spec.durationSeconds;
    package.captureSettings.testSignalType = TestSignalGenerator::toString(spec.type);

    // Safety note: the generated dry signal still uses the calibrated dBFS
    // level plus Easy-mode limiting above. This only changes how that already
    // safe output is stored: gain anchors get a self-contained dry reference
    // so model extraction can compare each return against the exact signal
    // that was sent during that capture.
    const auto dryChunkId = dryChunkIdForStep(*step);
    if (! dryChunkIsSharedForStep(*step) || package.findChunk(dryChunkId) == nullptr)
        setPcm16AudioChunk(package, dryChunkId, "dryReference", session.dryReferenceSignal(), spec.sampleRate);

    if (isCabinetPositionStep(*step))
        wizard.markCabinetSlotCapturing(stepId);

    wizard.setStepStatus(stepId, CaptureStepStatus::InProgress);
    package.captureWorkflow = wizard.toMetadataVar();
    activeStepId = stepId;

    if (! audio.captureSource().startCapture())
    {
        wizard.setStepStatus(stepId, CaptureStepStatus::Failed);
        package.captureWorkflow = wizard.toMetadataVar();
        appState.appendLog("Capture could not start because no test signal is loaded.");
        return;
    }

    session.begin();
    appState.appendLog("Started guided capture step: " + step->title);
}

void CaptureEngine::completeNonAudioStep(const juce::String& stepId)
{
    auto& wizard = appState.captureWizard();
    wizard.currentStepId = stepId;
    wizard.setStepStatus(stepId, CaptureStepStatus::Passed);

    if (stepId == "setup")
        if (auto* calibration = wizard.findStep("calibration"))
            if (calibration->status == CaptureStepStatus::NotStarted)
                calibration->status = CaptureStepStatus::Ready;

    appState.currentPackage().captureWorkflow = wizard.toMetadataVar();
    appState.appendLog("Completed guided step: " + stepId);
}

void CaptureEngine::resetCaptureStep(const juce::String& stepId)
{
    auto& wizard = appState.captureWizard();
    auto* step = wizard.findStep(stepId);
    if (step == nullptr)
    {
        appState.appendLog("Unknown capture step reset request: " + stepId);
        return;
    }

    if (activeStepId == stepId && session.state() == CaptureSessionState::Recording)
    {
        audio.captureSource().stopCapture();
        session.reset();
        activeStepId.clear();
    }

    auto& package = appState.currentPackage();
    if (! dryChunkIsSharedForStep(*step))
        package.removeChunk(dryChunkIdForStep(*step));
    package.removeChunk(capturedChunkIdForStep(*step));
    package.removeChunk(alignedChunkIdForStep(*step));
    if (isCabinetPositionStep(*step))
        wizard.resetCabinetSlot(stepId);
    wizard.removeResult(stepId);
    wizard.setStepStatus(stepId, CaptureStepStatus::Ready);
    wizard.currentStepId = stepId;
    package.captureWorkflow = wizard.toMetadataVar();

    appState.appendLog("Reset guided capture step: " + step->title);
}

bool CaptureEngine::importCabinetIrForStep(const juce::String& stepId, const juce::File& file)
{
    auto& wizard = appState.captureWizard();
    auto* step = wizard.findStep(stepId);
    if (step == nullptr || ! isCabinetPositionStep(*step))
    {
        appState.appendLog("Import IR is only available for cabinet position slots.");
        return false;
    }

    CabinetIrImporter importer;
    auto result = importer.importFile(file);
    if (! result.success)
    {
        wizard.markCabinetSlotError(stepId, result.error);
        wizard.setStepStatus(stepId, CaptureStepStatus::Failed);
        appState.currentPackage().captureWorkflow = wizard.toMetadataVar();
        appState.appendLog("IR import failed for " + step->title + ": " + result.error);
        return false;
    }

    auto& package = appState.currentPackage();
    configurePackageForCaptureType(package, wizard);
    package.metadata.sampleRate = result.sampleRate;
    const auto chunkId = alignedChunkIdForStep(*step);
    setPcm16AudioChunk(package, chunkId, "cabinet-ir", result.impulseResponse, result.sampleRate);

    CaptureQualityReport quality;
    quality.overallSeverity = CaptureQualitySeverity::Good;
    quality.peakDbfs = peakDb(result.impulseResponse);
    quality.rmsDbfs = rmsDb(result.impulseResponse);
    quality.alignmentConfidence = 1.0f;

    CaptureStepResult stepResult;
    stepResult.stepId = stepId;
    stepResult.status = CaptureStepStatus::Passed;
    stepResult.quality = quality;
    stepResult.alignedChunkId = chunkId;
    wizard.storeResult(stepResult);
    wizard.markCabinetSlotImported(stepId, chunkId, file.getFileName());

    if (wizard.hasCabinetRealSource())
        if (auto* finalStep = wizard.findStep("final-validation"))
            if (finalStep->status == CaptureStepStatus::NotStarted)
                finalStep->status = CaptureStepStatus::Ready;

    package.captureWorkflow = wizard.toMetadataVar();
    appState.markCaptureDataDirty();
    appState.appendLog("Imported cabinet IR for " + step->title + ": " + file.getFileName());
    return true;
}

bool CaptureEngine::buildCabinetFromSlots()
{
    auto& wizard = appState.captureWizard();
    if (! isCabinetWorkflow(wizard))
        return false;

    if (! wizard.hasCabinetRealSource())
    {
        appState.appendLog(cabinetExportRequiresRealSourceMessage());
        return false;
    }

    auto& package = appState.currentPackage();

    // Derive the compact per-band tone profile of every real IR source so that
    // estimated slots can be interpolated from actual measurements and so the
    // package carries a lightweight EQ payload alongside the raw IR chunks.
    auto profiledCount = 0;
    for (auto& slot : wizard.cabinetSlots)
    {
        if (! slot.hasRealSource() || slot.impulseResponseChunkId.isEmpty())
            continue;

        juce::AudioBuffer<float> impulseResponse;
        double irSampleRate = 0.0;
        juce::String decodeError;
        const auto* chunk = package.findChunk(slot.impulseResponseChunkId);
        if (chunk == nullptr
            || ! HansoAudioChunkCodec::decodePcm16Audio(chunk->data, impulseResponse, irSampleRate, decodeError))
        {
            appState.appendLog("Tone profile skipped for " + slot.label + ": "
                               + (chunk == nullptr ? "missing IR chunk" : decodeError));
            continue;
        }

        slot.toneProfile = CabinetToneProfiler::fromImpulseResponse(impulseResponse, irSampleRate);
        if (slot.toneProfile.valid)
            ++profiledCount;
    }

    wizard.estimateEmptyCabinetSlots();
    wizard.setStepStatus("final-validation", CaptureStepStatus::Passed);
    wizard.currentStepId = "final-validation";

    configurePackageForCaptureType(package, wizard);
    package.captureWorkflow = wizard.toMetadataVar();
    appState.markCaptureDataDirty();
    appState.appendLog("Cabinet package built from available mic position sources. Tone profiles computed for "
                       + juce::String(profiledCount) + " real source slot(s).");
    return true;
}

void CaptureEngine::setCalibrationOutputDb(float dbFs)
{
    userCalibrationOutputDb = juce::jlimit(-50.0f, -18.0f, dbFs);

    if (! isCalibrationMonitorRunning() || calibrationPhase != CalibrationMonitorPhase::Probing)
        return;

    audio.captureSource().replaceCalibrationSignal(makeProbeCalibrationSignal(generator,
                                                                              audio.currentSampleRate(),
                                                                              userCalibrationOutputDb));
    audio.captureSource().clearRecentInputHistory();
    calibrationSafeTicks = 0;
}

float CaptureEngine::calibrationOutputDb() const noexcept
{
    return userCalibrationOutputDb;
}

void CaptureEngine::startCalibrationMonitor()
{
    auto& wizard = appState.captureWizard();
    auto* step = wizard.findStep("calibration");
    if (step == nullptr)
        return;

    if (session.state() == CaptureSessionState::Recording)
    {
        appState.appendLog("Stop the active capture before starting calibration.");
        return;
    }

    const auto routing = OutputRoutingPolicy::forCaptureMode(wizard.mode);
    audio.captureSource().setOutputRoutingMode(routing);

    // Safety note: Calibration now starts with literal digital silence so the
    // return path can prove it is quiet before any probe is sent. The later
    // MultiSine probe still uses the existing -50..-18 dBFS slider range; no
    // output amplitude ceiling is raised here.
    audio.captureSource().clearRecentInputHistory();
    audio.captureSource().loadCalibrationSignal(makeSilentCalibrationSignal(audio.currentSampleRate()));
    if (! audio.captureSource().startCalibrationSignal())
    {
        wizard.setStepStatus("calibration", CaptureStepStatus::Failed);
        appState.appendLog("Calibration monitor could not start.");
        return;
    }

    calibrationSafeTicks = 0;
    calibrationPhase = CalibrationMonitorPhase::MeasuringNoise;
    calibrationPhaseStartMs = juce::Time::getMillisecondCounterHiRes();
    calibrationNoisePowerSum = 0.0;
    calibrationNoiseMeasurements = 0;
    calibrationNoiseFloorDbfsValue = -120.0f;
    calibrationSignalToNoiseDbValue = -120.0f;
    calibrationToneDominanceDb = -120.0f;
    calibrationLastLogCode.clear();
    calibrationLiveStatusText = utf8("무음 구간: 리턴 노이즈 플로어를 측정 중입니다.");
    wizard.currentStepId = "calibration";
    wizard.setStepStatus("calibration", CaptureStepStatus::InProgress);
    appState.currentPackage().captureWorkflow = wizard.toMetadataVar();
    appState.appendLog("Calibration monitor started with a silent noise-floor check. Keep interface monitoring loopback muted.");
}

void CaptureEngine::stopCalibrationMonitor()
{
    if (audio.captureSource().isCalibrationSignalRunning())
        appState.appendLog("Calibration monitor stopped.");

    audio.captureSource().stopCalibrationSignal();
    calibrationSafeTicks = 0;
    calibrationPhase = CalibrationMonitorPhase::Idle;
    calibrationLiveStatusText.clear();
    calibrationLastLogCode.clear();
}

void CaptureEngine::stopOutputSignal()
{
    audio.captureSource().stopCapture();
    audio.captureSource().stopCalibrationSignal();
    audio.captureSource().stopPreviewSample();
    audio.captureSource().setMonitoringEnabled(false);
    calibrationSafeTicks = 0;
    calibrationPhase = CalibrationMonitorPhase::Idle;
    calibrationLiveStatusText.clear();
    calibrationLastLogCode.clear();
}

bool CaptureEngine::isCalibrationMonitorRunning() const noexcept
{
    return audio.captureSource().isCalibrationSignalRunning();
}

bool CaptureEngine::isCalibrationLevelSafe() const noexcept
{
    const auto db = juce::Decibels::gainToDecibels(inputLevel(), -120.0f);
    return calibrationPhase == CalibrationMonitorPhase::Probing
        && db >= calibrationInputMinDb
        && db <= calibrationInputMaxDb
        && calibrationSignalToNoiseDbValue >= calibrationRequiredSnrDb
        && calibrationToneDominanceDb >= calibrationRequiredToneDominanceDb;
}

bool CaptureEngine::isCalibrationOutputLevelSafe() const noexcept
{
    const auto db = juce::Decibels::gainToDecibels(outputLevel(), -120.0f);
    return db >= calibrationOutputMinDb && db <= calibrationOutputMaxDb;
}

juce::String CaptureEngine::calibrationStatusText() const
{
    return calibrationLiveStatusText;
}

float CaptureEngine::calibrationNoiseFloorDbfs() const noexcept
{
    return calibrationNoiseFloorDbfsValue;
}

float CaptureEngine::calibrationSignalToNoiseDb() const noexcept
{
    return calibrationSignalToNoiseDbValue;
}

void CaptureEngine::updateLiveCalibration()
{
    if (! isCalibrationMonitorRunning())
        return;

    auto& wizard = appState.captureWizard();
    auto* step = wizard.findStep("calibration");
    if (step == nullptr || step->status == CaptureStepStatus::Passed)
        return;

    const auto peakDb = juce::Decibels::gainToDecibels(inputLevel(), -120.0f);
    const auto outputDb = juce::Decibels::gainToDecibels(outputLevel(), -120.0f);
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    const auto sampleRate = audio.currentSampleRate();
    const auto recentSamples = juce::jmax(1, static_cast<int>(sampleRate * calibrationRecentWindowSeconds));
    const auto recentReturn = audio.captureSource().copyRecentInputSignal(recentSamples);
    const auto config = calibrationValidationConfig();

    if (calibrationPhase == CalibrationMonitorPhase::MeasuringNoise)
    {
        const auto silentResult = CalibrationValidator::evaluateSilentReturn(recentReturn, config);
        calibrationNoiseFloorDbfsValue = silentResult.returnRmsDbfs;

        if (recentReturn.getNumSamples() > 0)
        {
            calibrationNoisePowerSum += dbToPower(silentResult.returnRmsDbfs);
            ++calibrationNoiseMeasurements;
        }

        if (nowMs - calibrationPhaseStartMs >= calibrationLoopbackGraceMs && silentResult.loopbackSuspected)
        {
            calibrationPhase = CalibrationMonitorPhase::Blocked;
            calibrationSafeTicks = 0;
            step->status = CaptureStepStatus::InProgress;
            calibrationLiveStatusText = silentResult.messageKorean
                                      + "\nReturn noise floor "
                                      + juce::String(silentResult.returnRmsDbfs, 1)
                                      + " dBFS";
            if (calibrationLastLogCode != silentResult.code)
            {
                calibrationLastLogCode = silentResult.code;
                appState.appendLog("Calibration blocked: " + silentResult.messageEnglish
                                   + " Return " + juce::String(silentResult.returnRmsDbfs, 1) + " dBFS.");
            }
            appState.currentPackage().captureWorkflow = wizard.toMetadataVar();
            return;
        }

        if (nowMs - calibrationPhaseStartMs < calibrationSilentMeasureSeconds * 1000.0)
        {
            calibrationSafeTicks = 0;
            step->status = CaptureStepStatus::InProgress;
            calibrationLiveStatusText = silentResult.messageKorean
                                      + "\nReturn noise floor "
                                      + juce::String(silentResult.returnRmsDbfs, 1)
                                      + " dBFS";
            appState.currentPackage().captureWorkflow = wizard.toMetadataVar();
            return;
        }

        if (calibrationNoiseMeasurements > 0)
            calibrationNoiseFloorDbfsValue = powerToDb(calibrationNoisePowerSum / static_cast<double>(calibrationNoiseMeasurements));

        audio.captureSource().replaceCalibrationSignal(makeProbeCalibrationSignal(generator,
                                                                                  sampleRate,
                                                                                  userCalibrationOutputDb));
        audio.captureSource().clearRecentInputHistory();
        calibrationPhase = CalibrationMonitorPhase::Probing;
        calibrationPhaseStartMs = nowMs;
        calibrationSafeTicks = 0;
        calibrationLastLogCode.clear();
        calibrationLiveStatusText = utf8("노이즈 플로어 측정 완료. MultiSine 프로브 리턴을 확인 중입니다.")
                                  + juce::String("\nNoise floor ")
                                  + juce::String(calibrationNoiseFloorDbfsValue, 1)
                                  + " dBFS";
        appState.appendLog("Calibration noise floor measured: "
                           + juce::String(calibrationNoiseFloorDbfsValue, 1)
                           + " dBFS. Starting MultiSine identity probe.");
        appState.currentPackage().captureWorkflow = wizard.toMetadataVar();
        return;
    }

    if (calibrationPhase == CalibrationMonitorPhase::Blocked)
    {
        calibrationSafeTicks = 0;
        step->status = CaptureStepStatus::InProgress;
        appState.currentPackage().captureWorkflow = wizard.toMetadataVar();
        return;
    }

    const auto frequencies = TestSignalGenerator::multiSineFrequencies();
    const auto validation = CalibrationValidator::validateProbe(recentReturn,
                                                               sampleRate,
                                                               frequencies.data(),
                                                               static_cast<int>(frequencies.size()),
                                                               peakDb,
                                                               outputDb,
                                                               calibrationNoiseFloorDbfsValue,
                                                               config);
    calibrationSignalToNoiseDbValue = validation.signalToNoiseDb;
    calibrationToneDominanceDb = validation.toneDominanceDb;

    if (validation.status == CalibrationValidationStatus::Passed)
    {
        ++calibrationSafeTicks;
        calibrationLiveStatusText = validation.messageKorean
                                  + "\nSNR "
                                  + juce::String(validation.signalToNoiseDb, 1)
                                  + " dB / Tone dominance "
                                  + juce::String(validation.toneDominanceDb, 1)
                                  + " dB";
    }
    else
    {
        calibrationSafeTicks = 0;
        step->status = CaptureStepStatus::InProgress;
        calibrationLiveStatusText = validation.messageKorean
                                  + "\nSNR "
                                  + juce::String(validation.signalToNoiseDb, 1)
                                  + " dB / Tone dominance "
                                  + juce::String(validation.toneDominanceDb, 1)
                                  + " dB / Noise floor "
                                  + juce::String(calibrationNoiseFloorDbfsValue, 1)
                                  + " dBFS";
        if (calibrationLastLogCode != validation.code)
        {
            calibrationLastLogCode = validation.code;
            appState.appendLog("Calibration waiting: " + validation.messageEnglish
                               + " SNR " + juce::String(validation.signalToNoiseDb, 1)
                               + " dB, tone dominance " + juce::String(validation.toneDominanceDb, 1) + " dB.");
        }
    }

    if (calibrationSafeTicks < 18)
    {
        appState.currentPackage().captureWorkflow = wizard.toMetadataVar();
        return;
    }

    CaptureQualityReport quality;
    quality.overallSeverity = CaptureQualitySeverity::Good;
    quality.peakDbfs = peakDb;
    quality.rmsDbfs = validation.returnRmsDbfs;
    quality.noiseFloorDbfs = calibrationNoiseFloorDbfsValue;
    quality.signalToNoiseDb = validation.signalToNoiseDb;
    quality.alignmentConfidence = 1.0f;

    CaptureStepResult result;
    result.stepId = "calibration";
    result.status = CaptureStepStatus::Passed;
    result.quality = quality;
    wizard.storeResult(result);
    wizard.calibrationPassed = true;

    unlockPostCalibrationSteps(wizard);

    appState.currentPackage().captureWorkflow = wizard.toMetadataVar();
    stopCalibrationMonitor();
    appState.appendLog("Calibration completed from live level meter: "
                       + juce::String(peakDb, 1)
                       + " dBFS, noise floor "
                       + juce::String(quality.noiseFloorDbfs, 1)
                       + " dBFS, SNR "
                       + juce::String(quality.signalToNoiseDb, 1)
                       + " dB.");
}

void CaptureEngine::stop()
{
    if (isCalibrationMonitorRunning())
    {
        stopCalibrationMonitor();
        return;
    }

    audio.captureSource().stopCapture();
    if (session.state() != CaptureSessionState::Recording)
    {
        appState.appendLog("No active capture to stop.");
        return;
    }

    if (activeStepId.isNotEmpty())
    {
        auto& wizard = appState.captureWizard();
        wizard.setStepStatus(activeStepId, CaptureStepStatus::Failed);
        appState.currentPackage().captureWorkflow = wizard.toMetadataVar();
        session.reset();
        appState.appendLog("Guided capture stopped before completion: " + activeStepId);
        activeStepId.clear();
        return;
    }

    session.storeCapturedSignal(audio.captureSource().copyCapturedSignal());

    setAudioChunk(appState.currentPackage(),
                  "capture/captured.f32",
                  "captured",
                  session.capturedSignal(),
                  session.getSampleRate());
    appState.markCaptureDataDirty();
    session.finish();
    appState.appendLog("Capture recording completed.");
}

void CaptureEngine::reset()
{
    session.reset();
    audio.captureSource().stopCapture();
    audio.captureSource().stopCalibrationSignal();
    clearPreviewModel();
    activeStepId.clear();
    calibrationSafeTicks = 0;
    calibrationPhase = CalibrationMonitorPhase::Idle;
    calibrationLiveStatusText.clear();
    calibrationLastLogCode.clear();
    appState.appendLog("Capture reset.");
}

void CaptureEngine::refresh()
{
    if (session.state() == CaptureSessionState::Recording && audio.captureSource().hasCaptureFinished())
    {
        session.storeCapturedSignal(audio.captureSource().copyCapturedSignal());
        session.finish();
        auto& package = appState.currentPackage();
        auto& wizard = appState.captureWizard();

        if (auto* step = wizard.findStep(activeStepId))
        {
            auto alignResult = alignment.estimateOffset(session.dryReferenceSignal(),
                                                        session.capturedSignal(),
                                                        session.getSampleRate());
            session.storeAlignedCapturedSignal(std::move(alignResult.alignedCaptured), alignResult.estimatedOffsetSamples);

            const auto dryChunkId = dryChunkIdForStep(*step);
            const auto capturedChunkId = capturedChunkIdForStep(*step);
            const auto alignedChunkId = alignedChunkIdForStep(*step);
            if (! step->isAnchorCapture())
                setAudioChunk(package, capturedChunkId, "captured", session.capturedSignal(), session.getSampleRate());
            setPcm16AudioChunk(package, alignedChunkId, "alignedCaptured", session.alignedCapturedSignal(), session.getSampleRate());

            const auto rightMuted = wizard.mode != CaptureMode::Easy
                                 || OutputRoutingPolicy::isRightChannelMuted(audio.captureSource().outputRoutingMode());
            auto quality = qualityAnalyzer.analyze(session.capturedSignal(),
                                                   session.dryReferenceSignal().getNumSamples(),
                                                   alignResult.estimatedOffsetSamples,
                                                   alignResult.estimatedOffsetMs,
                                                   alignResult.confidence,
                                                   rightMuted,
                                                   qualityTargetForStep(*step));

            CaptureStepResult result;
            result.stepId = step->stepId;
            result.status = statusFromQuality(quality);
            result.quality = quality;
            result.dryChunkId = dryChunkId;
            result.capturedChunkId = capturedChunkId;
            result.alignedChunkId = alignedChunkId;
            result.dryPeakDbfs = peakDb(session.dryReferenceSignal());
            result.dryRmsDbfs = rmsDb(session.dryReferenceSignal());
            result.captureOutputDbfs = juce::Decibels::gainToDecibels(session.getTestSignalSpec().amplitude, -120.0f);
            wizard.storeResult(result);
            appState.markCaptureDataDirty();

            if (step->stepId == "calibration" && result.status != CaptureStepStatus::Failed)
            {
                wizard.calibrationPassed = true;
                unlockPostCalibrationSteps(wizard);
            }
            else if (step->stepId == "gain-010" && result.status != CaptureStepStatus::Failed)
            {
                if (auto* next = wizard.findStep("gain-050"))
                    if (next->status == CaptureStepStatus::NotStarted)
                        next->status = CaptureStepStatus::Ready;
            }
            else if (step->stepId == "gain-050" && result.status != CaptureStepStatus::Failed)
            {
                if (auto* next = wizard.findStep("gain-100"))
                    if (next->status == CaptureStepStatus::NotStarted)
                        next->status = CaptureStepStatus::Ready;
            }
            else if (step->stepId == "gain-100" && result.status != CaptureStepStatus::Failed)
            {
                if (auto* finalStep = wizard.findStep("final-validation"))
                    finalStep->status = CaptureStepStatus::Ready;
            }
            else if (isCabinetPositionStep(*step))
            {
                if (result.status != CaptureStepStatus::Failed)
                {
                    wizard.markCabinetSlotCaptured(step->stepId, alignedChunkId);
                    if (wizard.hasCabinetRealSource())
                        if (auto* finalStep = wizard.findStep("final-validation"))
                            if (finalStep->status == CaptureStepStatus::NotStarted)
                                finalStep->status = CaptureStepStatus::Ready;
                }
                else
                {
                    wizard.markCabinetSlotError(step->stepId, "Capture quality check failed.");
                }
            }

            package.analysisSummary.estimatedLatencySamples = alignResult.estimatedOffsetSamples;
            package.analysisSummary.estimatedLatencyMs = alignResult.estimatedOffsetMs;
            package.dynamicProfile.estimatedLatencySeconds = static_cast<float>(alignResult.estimatedOffsetMs / 1000.0);
            package.captureWorkflow = wizard.toMetadataVar();
            audio.captureSource().stopCapture();

            appState.appendLog("Guided capture step completed: " + step->title
                               + " / " + toString(result.status)
                               + " / Peak " + juce::String(quality.peakDbfs, 1)
                               + " dBFS, RMS " + juce::String(quality.rmsDbfs, 1) + " dBFS.");
        }
        else
        {
            setAudioChunk(package,
                          "capture/captured.f32",
                          "captured",
                          session.capturedSignal(),
                          session.getSampleRate());
            appState.markCaptureDataDirty();
        }

        appState.appendLog("Capture completed at end of test signal. Captured peak: "
                           + juce::String(capturedPeakDb(), 1) + " dBFS, RMS: "
                           + juce::String(capturedRmsDb(), 1) + " dBFS.");

        if (capturedPeakDb() <= -90.0f)
            appState.appendLog("Captured signal is near silence. Check input channel selection and Babyface input meter.");

        activeStepId.clear();
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
    package.removeChunk("capture/aligned-captured.f32");

    const auto& wizard = appState.captureWizard();
    const auto hasGuidedAnchorCaptures = wizard.findResult("gain-010") != nullptr
                                      || wizard.findResult("gain-050") != nullptr
                                      || wizard.findResult("gain-100") != nullptr;
    if (! hasGuidedAnchorCaptures)
        setPcm16AudioChunk(package,
                           "capture/aligned-captured.pcm16",
                           "alignedCaptured",
                           session.alignedCapturedSignal(),
                           session.getSampleRate());

    appState.markCaptureDataDirty();
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

juce::String CaptureEngine::activeCaptureStepId() const
{
    return activeStepId;
}

double CaptureEngine::captureProgress() const noexcept
{
    return audio.captureSource().captureProgress();
}

juce::String CaptureEngine::stateText() const
{
    if (isCalibrationMonitorRunning())
        return "Calibrating";

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

double CaptureEngine::currentSampleRate() const noexcept
{
    return audio.currentSampleRate();
}

const CaptureSession& CaptureEngine::currentSession() const noexcept
{
    return session;
}

bool CaptureEngine::hasCompletedCapture() const noexcept
{
    return session.hasCapture();
}

bool CaptureEngine::loadPreviewModel(const CompactHansoModel& model)
{
    clearPreviewCabinetPackage();
    const auto loaded = audio.captureSource().loadPreviewModel(model);
    if (loaded)
    {
        previewModel = model;
        previewModelLoaded = true;
        ++previewRevision;
        appState.appendLog("Tone preview model loaded: " + audio.captureSource().previewModelSummary());
    }
    else
    {
        appState.appendLog("Tone preview model could not be loaded.");
    }

    return loaded;
}

void CaptureEngine::setPreviewGainPercent(float percent)
{
    audio.captureSource().setPreviewGainPercent(percent);
}

void CaptureEngine::clearPreviewModel()
{
    audio.captureSource().clearPreviewModel();
    previewModel = {};
    previewModelLoaded = false;
    ++previewRevision;
    appState.appendLog("Tone preview model cleared. Preview will play clean samples.");
}

bool CaptureEngine::loadPreviewCabinetPackage(const HansoPackage& package)
{
    clearPreviewModel();
    juce::String error;
    const auto loaded = audio.captureSource().loadPreviewCabinetPackage(package, error);
    if (loaded)
    {
        previewCabinetLoaded = true;
        ++previewCabinetRevisionCounter;
        appState.appendLog("Tone preview cabinet loaded: " + audio.captureSource().previewCabinetSummary());
    }
    else
    {
        previewCabinetLoaded = false;
        appState.appendLog("Tone preview cabinet could not be loaded: " + error);
    }

    return loaded;
}

void CaptureEngine::setPreviewMicPositionPercent(float percent)
{
    audio.captureSource().setPreviewMicPositionNormalized(juce::jlimit(0.0f, 100.0f, percent) / 100.0f);
}

void CaptureEngine::clearPreviewCabinetPackage()
{
    if (! previewCabinetLoaded && ! audio.captureSource().hasPreviewCabinetPackage())
        return;

    audio.captureSource().clearPreviewCabinetPackage();
    previewCabinetLoaded = false;
    ++previewCabinetRevisionCounter;
    appState.appendLog("Tone preview cabinet cleared.");
}

bool CaptureEngine::hasPreviewCabinetPackage() const noexcept
{
    return previewCabinetLoaded && audio.captureSource().hasPreviewCabinetPackage();
}

int CaptureEngine::previewCabinetRevision() const noexcept
{
    return previewCabinetRevisionCounter;
}

const CompactHansoModel* CaptureEngine::currentPreviewModel() const noexcept
{
    return previewModelLoaded ? &previewModel : nullptr;
}

int CaptureEngine::previewModelRevision() const noexcept
{
    return previewRevision;
}

void CaptureEngine::loadPreviewSample(const juce::AudioBuffer<float>& sample)
{
    audio.captureSource().loadPreviewSample(sample);
}

bool CaptureEngine::startPreviewSample()
{
    const auto started = audio.captureSource().startPreviewSample();
    appState.appendLog(started ? "Tone preview sample playback started."
                               : "Tone preview could not start. Load a preview sample first.");
    return started;
}

void CaptureEngine::stopPreviewSample()
{
    audio.captureSource().stopPreviewSample();
    appState.appendLog("Tone preview sample playback stopped.");
}

bool CaptureEngine::isPreviewSamplePlaying() const noexcept
{
    return audio.captureSource().isPreviewSamplePlaying();
}

double CaptureEngine::previewSampleProgress() const noexcept
{
    return audio.captureSource().previewSampleProgress();
}

void CaptureEngine::seekPreviewSample(double normalizedProgress)
{
    audio.captureSource().seekPreviewSample(normalizedProgress);
}

void CaptureEngine::setPreviewLoopEnabled(bool enabled)
{
    audio.captureSource().setPreviewLoopEnabled(enabled);
}

bool CaptureEngine::isPreviewLoopEnabled() const noexcept
{
    return audio.captureSource().isPreviewLoopEnabled();
}

void CaptureEngine::setPreviewOutputGainDb(float gainDb)
{
    audio.captureSource().setPreviewOutputGainDb(gainDb);
}

float CaptureEngine::previewOutputGainDb() const noexcept
{
    return audio.captureSource().previewOutputGainDb();
}

void CaptureEngine::setPreviewNormalizationEnabled(bool enabled)
{
    audio.captureSource().setPreviewNormalizationEnabled(enabled);
}

bool CaptureEngine::isPreviewNormalizationEnabled() const noexcept
{
    return audio.captureSource().isPreviewNormalizationEnabled();
}

void CaptureEngine::setPreviewCabinetEnabled(bool enabled)
{
    audio.captureSource().setPreviewCabinetEnabled(enabled);
}

bool CaptureEngine::isPreviewCabinetEnabled() const noexcept
{
    return audio.captureSource().isPreviewCabinetEnabled();
}

bool CaptureEngine::hasPreviewModel() const noexcept
{
    return audio.captureSource().hasPreviewModel();
}

juce::String CaptureEngine::previewModelSummary() const
{
    return audio.captureSource().previewModelSummary();
}
}

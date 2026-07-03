// Offline regression tests for the HANSO Lab analysis pipeline.
// Every check runs on synthetic signals with known ground truth, so a failure
// means the analysis math regressed, not that hardware behaved unexpectedly.

#include <JuceHeader.h>

#include "Analysis/CabinetToneProfiler.h"
#include "Analysis/CalibrationValidator.h"
#include "Analysis/CaptureQualityAnalyzer.h"
#include "Analysis/ModelExtractionEngine.h"
#include "Analysis/SpectrumUtils.h"
#include "Capture/CaptureWizardState.h"
#include "Capture/SignalAlignment.h"
#include "Capture/TestSignalGenerator.h"
#include "Model/HansoPackage.h"
#include "Serialization/HansoAudioChunkCodec.h"

namespace
{
int failureCount = 0;

void check(bool condition, const juce::String& name, const juce::String& detail = {})
{
    if (condition)
    {
        std::cout << "[PASS] " << name << std::endl;
    }
    else
    {
        ++failureCount;
        std::cout << "[FAIL] " << name;
        if (detail.isNotEmpty())
            std::cout << " — " << detail;
        std::cout << std::endl;
    }
}

constexpr double sampleRate = 48000.0;

juce::AudioBuffer<float> makeSweep(double seconds, float amplitude)
{
    hanso::TestSignalSpec spec;
    spec.type = hanso::TestSignalType::LogSineSweep;
    spec.sampleRate = sampleRate;
    spec.durationSeconds = seconds;
    spec.amplitude = amplitude;
    return hanso::TestSignalGenerator().generate(spec);
}

juce::AudioBuffer<float> makeMultiSine(double seconds, float amplitude)
{
    hanso::TestSignalSpec spec;
    spec.type = hanso::TestSignalType::MultiSine;
    spec.sampleRate = sampleRate;
    spec.durationSeconds = seconds;
    spec.amplitude = amplitude;
    return hanso::TestSignalGenerator().generate(spec);
}

float rmsDbfs(const juce::AudioBuffer<float>& buffer)
{
    return hanso::CalibrationValidator::rmsDbfs(buffer);
}

float peakDbfs(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return -120.0f;

    auto peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        peak = juce::jmax(peak, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));

    return juce::Decibels::gainToDecibels(peak, -120.0f);
}

juce::AudioBuffer<float> makeWhiteNoiseWithRms(double seconds, float targetRmsDbfs, int seed)
{
    const auto numSamples = juce::jmax(1, static_cast<int>(seconds * sampleRate));
    const auto targetRms = juce::Decibels::decibelsToGain(targetRmsDbfs);
    const auto uniformPeak = targetRms * std::sqrt(3.0f);
    juce::Random random(seed);
    juce::AudioBuffer<float> buffer(1, numSamples);
    for (int sample = 0; sample < numSamples; ++sample)
        buffer.setSample(0, sample, (random.nextFloat() * 2.0f - 1.0f) * uniformPeak);

    return buffer;
}

void addWhiteNoiseAtRms(juce::AudioBuffer<float>& buffer, float targetRmsDbfs, int seed)
{
    const auto targetRms = juce::Decibels::decibelsToGain(targetRmsDbfs);
    const auto uniformPeak = targetRms * std::sqrt(3.0f);
    juce::Random random(seed);
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        buffer.addSample(0, sample, (random.nextFloat() * 2.0f - 1.0f) * uniformPeak);
}

juce::AudioBuffer<float> applyTanhDrive(const juce::AudioBuffer<float>& dry, float drive, float outputScale)
{
    auto peak = dry.getMagnitude(0, 0, dry.getNumSamples());
    if (peak <= 0.0f)
        peak = 1.0f;

    juce::AudioBuffer<float> out(1, dry.getNumSamples());
    const auto norm = std::tanh(drive);
    for (int i = 0; i < dry.getNumSamples(); ++i)
        out.setSample(0, i, std::tanh(drive * dry.getSample(0, i) / peak) / norm * outputScale);

    return out;
}

juce::AudioBuffer<float> applyTanhDriveWithOutputGain(const juce::AudioBuffer<float>& dry, float drive, float outputGain)
{
    return applyTanhDrive(dry, drive, dry.getMagnitude(0, 0, dry.getNumSamples()) * outputGain);
}

void setPcm16Chunk(hanso::HansoPackage& package,
                   const juce::String& id,
                   const juce::AudioBuffer<float>& buffer)
{
    package.setChunk(id,
                     "test",
                     "audio/x-hanso-pcm16",
                     hanso::HansoAudioChunkCodec::encodePcm16Audio(buffer, sampleRate),
                     sampleRate,
                     buffer.getNumChannels(),
                     buffer.getNumSamples());
}

void testFullSpectrumCoverage()
{
    const auto sweep = makeSweep(5.0, 0.2f);
    const hanso::AveragedPowerSpectrum spectrum(sweep, sampleRate);

    const auto lowDb = spectrum.bandPowerDb(60.0, 250.0);
    const auto midDb = spectrum.bandPowerDb(250.0, 2500.0);
    const auto highDb = spectrum.bandPowerDb(2500.0, 10000.0);

    // A log sweep spends equal time per octave, so every band must contain
    // real energy. The pre-fix implementation returned silence above ~50 Hz.
    check(midDb > -60.0f, "spectrum covers mid band", "midDb=" + juce::String(midDb, 1));
    check(highDb > -60.0f, "spectrum covers high band", "highDb=" + juce::String(highDb, 1));
    check(std::abs(lowDb - midDb) < 15.0f, "sweep band balance low/mid",
          juce::String(lowDb, 1) + " vs " + juce::String(midDb, 1));
}

void testAlignmentOffset()
{
    const auto dry = makeSweep(2.0, 0.2f);
    constexpr auto offset = 1234;
    juce::AudioBuffer<float> delayed(1, dry.getNumSamples() + offset);
    delayed.clear();
    delayed.copyFrom(0, offset, dry, 0, 0, dry.getNumSamples());
    delayed.applyGain(0.5f);

    const auto result = hanso::SignalAlignment().estimateOffset(dry, delayed, sampleRate);
    check(std::abs(result.estimatedOffsetSamples - offset) <= 4, "alignment finds known offset",
          "estimated=" + juce::String(result.estimatedOffsetSamples));
    check(result.confidence > 0.8f, "alignment confidence high on clean delay",
          "confidence=" + juce::String(result.confidence, 2));
}

void testDriveExtraction()
{
    const auto dry = makeSweep(5.0, 0.2f);

    hanso::HansoPackage package;
    package.metadata.numOutputChannels = 1;
    setPcm16Chunk(package, "capture/shared/dry-reference.pcm16", dry);
    setPcm16Chunk(package, "capture/gain-010/aligned-captured.pcm16", applyTanhDrive(dry, 1.5f, 0.3f));
    setPcm16Chunk(package, "capture/gain-050/aligned-captured.pcm16", applyTanhDrive(dry, 5.0f, 0.4f));
    setPcm16Chunk(package, "capture/gain-100/aligned-captured.pcm16", applyTanhDrive(dry, 9.0f, 0.5f));

    const auto result = hanso::ModelExtractionEngine().extractIntoPackage(package);
    check(result.success, "model extraction succeeds", result.message);
    if (! result.success)
        return;

    check(result.model.anchors.size() == 3, "three anchors extracted");

    const auto& anchors = result.model.anchors;
    check(anchors[0].drive < anchors[1].drive && anchors[1].drive < anchors[2].drive,
          "drive ordering follows actual distortion",
          juce::String(anchors[0].drive, 2) + " / " + juce::String(anchors[1].drive, 2)
              + " / " + juce::String(anchors[2].drive, 2));
    check(std::abs(anchors[1].drive - 5.0f) < 2.0f, "mid anchor drive near ground truth",
          "fitted=" + juce::String(anchors[1].drive, 2));
    check(std::abs(anchors[2].drive - 9.0f) < 3.0f, "high anchor drive near ground truth",
          "fitted=" + juce::String(anchors[2].drive, 2));
}

void testLinearSystemGivesLowDrive()
{
    const auto dry = makeSweep(5.0, 0.2f);
    juce::AudioBuffer<float> linear(1, dry.getNumSamples());
    linear.copyFrom(0, 0, dry, 0, 0, dry.getNumSamples());
    linear.applyGain(0.7f);

    hanso::HansoPackage package;
    package.metadata.numOutputChannels = 1;
    setPcm16Chunk(package, "capture/shared/dry-reference.pcm16", dry);
    setPcm16Chunk(package, "capture/gain-050/aligned-captured.pcm16", linear);

    const auto result = hanso::ModelExtractionEngine().extractIntoPackage(package);
    check(result.success, "linear extraction succeeds", result.message);
    if (result.success && ! result.model.anchors.empty())
        check(result.model.anchors.front().drive < 2.5f, "clean device fits low drive",
              "fitted=" + juce::String(result.model.anchors.front().drive, 2));
}

void testSelfContainedAnchorDryLevels()
{
    auto dryLow = makeSweep(5.0, 0.18f);
    auto dryMid = makeSweep(5.0, 0.07f);
    auto dryHigh = makeSweep(5.0, 0.24f);

    hanso::HansoPackage package;
    package.metadata.numOutputChannels = 1;

    setPcm16Chunk(package, "capture/gain-010/dry-reference.pcm16", dryLow);
    setPcm16Chunk(package, "capture/gain-050/dry-reference.pcm16", dryMid);
    setPcm16Chunk(package, "capture/gain-100/dry-reference.pcm16", dryHigh);
    setPcm16Chunk(package, "capture/gain-010/aligned-captured.pcm16", applyTanhDriveWithOutputGain(dryLow, 5.0f, 0.45f));
    setPcm16Chunk(package, "capture/gain-050/aligned-captured.pcm16", applyTanhDriveWithOutputGain(dryMid, 5.0f, 0.75f));
    setPcm16Chunk(package, "capture/gain-100/aligned-captured.pcm16", applyTanhDriveWithOutputGain(dryHigh, 5.0f, 1.10f));

    const auto result = hanso::ModelExtractionEngine().extractIntoPackage(package);
    check(result.success, "self-contained anchor extraction succeeds", result.message);
    if (! result.success)
        return;

    const auto& anchors = result.model.anchors;
    check(anchors.size() == 3, "self-contained extraction keeps three anchors");
    if (anchors.size() != 3)
        return;

    const auto maxDriveDelta = juce::jmax(std::abs(anchors[0].drive - anchors[1].drive),
                                          std::abs(anchors[1].drive - anchors[2].drive));
    check(maxDriveDelta <= 0.5f, "drive fit ignores anchor dry-level differences",
          juce::String(anchors[0].drive, 2) + " / " + juce::String(anchors[1].drive, 2)
              + " / " + juce::String(anchors[2].drive, 2));

    check(anchors[0].outputGainDb < anchors[1].outputGainDb
              && anchors[1].outputGainDb < anchors[2].outputGainDb,
          "output gain is monotonic after per-anchor dry normalization",
          juce::String(anchors[0].outputGainDb, 2) + " / " + juce::String(anchors[1].outputGainDb, 2)
              + " / " + juce::String(anchors[2].outputGainDb, 2));
}

void testCabinetToneProfiler()
{
    juce::AudioBuffer<float> flatIr(1, 4096);
    flatIr.clear();
    flatIr.setSample(0, 0, 1.0f);

    juce::AudioBuffer<float> darkIr(1, 4096);
    darkIr.clear();
    auto state = 0.0f;
    for (int i = 0; i < darkIr.getNumSamples(); ++i)
    {
        const auto input = i == 0 ? 1.0f : 0.0f;
        state = 0.9f * state + 0.1f * input;
        darkIr.setSample(0, i, state);
    }

    const auto flat = hanso::CabinetToneProfiler::fromImpulseResponse(flatIr, sampleRate);
    const auto dark = hanso::CabinetToneProfiler::fromImpulseResponse(darkIr, sampleRate);

    check(flat.valid && dark.valid, "tone profiles computed");
    if (! flat.valid || ! dark.valid)
        return;

    const auto flatSpread = flat.bandGainsDb.back() - flat.bandGainsDb.front();
    const auto darkSpread = dark.bandGainsDb.back() - dark.bandGainsDb.front();
    check(std::abs(flatSpread) < 6.0f, "flat IR profile is flat", juce::String(flatSpread, 1));
    check(darkSpread < -12.0f, "lowpassed IR profile rolls off highs", juce::String(darkSpread, 1));

    const auto mid = hanso::CabinetToneProfile::interpolate(flat, dark, 0.5f);
    check(mid.valid && mid.estimated, "interpolated profile flagged estimated");
    const auto midHigh = mid.bandGainsDb.back();
    check(midHigh < flat.bandGainsDb.back() && midHigh > dark.bandGainsDb.back(),
          "interpolated high band between anchors", juce::String(midHigh, 1));
}

void testEstimatedSlotInterpolation()
{
    hanso::CaptureWizardState wizard(hanso::CaptureType::Cabinet);

    auto* center = wizard.findCabinetSlot("cab-center");
    auto* offAxis = wizard.findCabinetSlot("cab-off-axis");
    check(center != nullptr && offAxis != nullptr, "default cabinet slots exist");
    if (center == nullptr || offAxis == nullptr)
        return;

    center->source = hanso::CabinetSlotSource::CapturedIr;
    center->toneProfile.valid = true;
    center->toneProfile.levelDb = 0.0f;
    center->toneProfile.bandGainsDb.fill(4.0f);

    offAxis->source = hanso::CabinetSlotSource::ImportedIr;
    offAxis->toneProfile.valid = true;
    offAxis->toneProfile.levelDb = -6.0f;
    offAxis->toneProfile.bandGainsDb.fill(-4.0f);

    wizard.estimateEmptyCabinetSlots();

    const auto* edge = wizard.findCabinetSlot("cab-edge");
    check(edge != nullptr && edge->source == hanso::CabinetSlotSource::EstimatedCompactCab,
          "empty slot becomes estimated");
    if (edge == nullptr)
        return;

    check(edge->toneProfile.valid && edge->toneProfile.estimated, "estimated slot carries a tone profile");
    check(edge->toneProfile.bandGainsDb.front() > -4.0f && edge->toneProfile.bandGainsDb.front() < 4.0f,
          "estimated profile interpolated between anchors",
          juce::String(edge->toneProfile.bandGainsDb.front(), 2));

    const auto roundTrip = hanso::CabinetToneProfile::fromVar(edge->toneProfile.toVar());
    check(roundTrip.valid
              && std::abs(roundTrip.bandGainsDb.front() - edge->toneProfile.bandGainsDb.front()) < 0.01f,
          "tone profile serialization round-trips");
}

void testQualityAnalyzer()
{
    const hanso::CaptureQualityAnalyzer analyzer;

    juce::AudioBuffer<float> silent(1, static_cast<int>(sampleRate));
    silent.clear();
    const auto silentReport = analyzer.analyze(silent, silent.getNumSamples(), 512, 10.0, 0.9f, true);
    check(silentReport.hasErrors(), "silent capture reports error");

    auto good = makeSweep(1.0, 0.1f);
    const auto goodReport = analyzer.analyze(good, good.getNumSamples(), 512, 10.0, 0.9f, true);
    check(! goodReport.hasErrors(), "healthy capture passes");

    const auto misrouted = analyzer.analyze(good, good.getNumSamples(), 512, 10.0, 0.1f, true);
    check(misrouted.hasErrors(), "uncorrelated capture reports alignment error");

    juce::AudioBuffer<float> clipped(1, static_cast<int>(sampleRate));
    for (int i = 0; i < clipped.getNumSamples(); ++i)
        clipped.setSample(0, i, i % 100 < 50 ? 1.0f : -1.0f);
    const auto clippedReport = analyzer.analyze(clipped, clipped.getNumSamples(), 512, 10.0, 0.9f, true);
    check(clippedReport.hasErrors(), "clipped capture reports error");
}

void testCalibrationValidator()
{
    const auto frequencies = hanso::TestSignalGenerator::multiSineFrequencies();

    auto broadbandNoise = makeWhiteNoiseWithRms(1.5, -30.0f, 0x48414e53);
    auto noiseOnly = hanso::CalibrationValidator::validateProbe(broadbandNoise,
                                                                sampleRate,
                                                                frequencies.data(),
                                                                static_cast<int>(frequencies.size()),
                                                                peakDbfs(broadbandNoise),
                                                                -33.0f,
                                                                -80.0f);
    check(noiseOnly.status == hanso::CalibrationValidationStatus::IdentityFailed,
          "calibration rejects broadband noise in level window",
          "status=" + noiseOnly.code + ", dominance=" + juce::String(noiseOnly.toneDominanceDb, 1));

    auto cleanProbe = makeMultiSine(1.5, 0.14f);
    const auto probeRmsDb = rmsDbfs(cleanProbe);
    auto probeWithWeakNoise = cleanProbe;
    addWhiteNoiseAtRms(probeWithWeakNoise, probeRmsDb - 20.0f, 0x51554554);
    auto validProbe = hanso::CalibrationValidator::validateProbe(probeWithWeakNoise,
                                                                 sampleRate,
                                                                 frequencies.data(),
                                                                 static_cast<int>(frequencies.size()),
                                                                 peakDbfs(probeWithWeakNoise),
                                                                 -33.0f,
                                                                 probeRmsDb - 20.0f);
    check(validProbe.status == hanso::CalibrationValidationStatus::Passed,
          "calibration accepts MultiSine with 20 dB SNR",
          "status=" + validProbe.code + ", snr=" + juce::String(validProbe.signalToNoiseDb, 1)
              + ", dominance=" + juce::String(validProbe.toneDominanceDb, 1));

    auto probeWithStrongNoise = cleanProbe;
    addWhiteNoiseAtRms(probeWithStrongNoise, probeRmsDb - 10.0f, 0x4e4f4953);
    auto noisyProbe = hanso::CalibrationValidator::validateProbe(probeWithStrongNoise,
                                                                 sampleRate,
                                                                 frequencies.data(),
                                                                 static_cast<int>(frequencies.size()),
                                                                 peakDbfs(probeWithStrongNoise),
                                                                 -33.0f,
                                                                 probeRmsDb - 10.0f);
    check(noisyProbe.status != hanso::CalibrationValidationStatus::Passed && ! noisyProbe.snrOk,
          "calibration rejects MultiSine at 10 dB SNR",
          "status=" + noisyProbe.code + ", snr=" + juce::String(noisyProbe.signalToNoiseDb, 1));

    juce::AudioBuffer<float> silentOutputReturn(1, static_cast<int>(sampleRate));
    const auto loopbackRms = juce::Decibels::decibelsToGain(-40.0f);
    for (int sample = 0; sample < silentOutputReturn.getNumSamples(); ++sample)
    {
        const auto phase = juce::MathConstants<double>::twoPi * 440.0 * static_cast<double>(sample) / sampleRate;
        silentOutputReturn.setSample(0, sample, loopbackRms * std::sqrt(2.0f) * static_cast<float>(std::sin(phase)));
    }

    const auto silentResult = hanso::CalibrationValidator::evaluateSilentReturn(silentOutputReturn);
    check(silentResult.status == hanso::CalibrationValidationStatus::LoopbackSuspected,
          "calibration flags silent-output -40 dBFS return as loopback suspect",
          "status=" + silentResult.code + ", rms=" + juce::String(silentResult.returnRmsDbfs, 1));
}
}

int main()
{
    testFullSpectrumCoverage();
    testAlignmentOffset();
    testDriveExtraction();
    testLinearSystemGivesLowDrive();
    testSelfContainedAnchorDryLevels();
    testCabinetToneProfiler();
    testEstimatedSlotInterpolation();
    testQualityAnalyzer();
    testCalibrationValidator();

    if (failureCount > 0)
    {
        std::cout << failureCount << " check(s) failed." << std::endl;
        return 1;
    }

    std::cout << "All checks passed." << std::endl;
    return 0;
}

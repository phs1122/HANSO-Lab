// Offline regression tests for the HANSO Lab analysis pipeline.
// Every check runs on synthetic signals with known ground truth, so a failure
// means the analysis math regressed, not that hardware behaved unexpectedly.

#include <JuceHeader.h>

#include "Analysis/CabinetMicMatrixEstimator.h"
#include "Analysis/CabinetToneProfiler.h"
#include "Analysis/MicColorationProfiles.h"
#include "Audio/CaptureAudioSource.h"
#include "Audio/PreviewMicDistanceProcessor.h"
#include "Audio/PreviewMicColorProcessor.h"
#include "Analysis/CalibrationValidator.h"
#include "Analysis/ModelFidelityEvaluator.h"
#include "Analysis/CaptureQualityAnalyzer.h"
#include "Analysis/ModelExtractionEngine.h"
#include "Analysis/SpectrumUtils.h"
#include "Capture/CabinetMicPositions.h"
#include "Capture/CaptureWizardState.h"
#include "Capture/SignalAlignment.h"
#include "Capture/TestSignalGenerator.h"
#include "Model/HansoPackage.h"
#include "Preview/PreviewChainPolicy.h"
#include "Serialization/HansoAudioChunkCodec.h"
#include "Serialization/HansoSerializer.h"

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
    const auto matrix = hanso::CabinetMicMatrixEstimator::estimate(wizard.cabinetSlots);
    const hanso::CabinetMicMatrixEntry* expected = nullptr;
    for (const auto& entry : matrix.entries)
        if (entry.positionId == "cab-edge" && entry.micClass == hanso::CabinetMicClass::Dynamic)
            expected = &entry;
    check(expected != nullptr && expected->toneProfile.valid
              && std::abs(edge->toneProfile.bandGainsDb.front() - expected->toneProfile.bandGainsDb.front()) < 0.01f,
          "estimated slot uses its calculated Cone Edge profile",
          juce::String(edge->toneProfile.bandGainsDb.front(), 2));

    const auto roundTrip = hanso::CabinetToneProfile::fromVar(edge->toneProfile.toVar());
    check(roundTrip.valid
              && std::abs(roundTrip.bandGainsDb.front() - edge->toneProfile.bandGainsDb.front()) < 0.01f,
          "tone profile serialization round-trips");
}

void testPreviewChainPolicy()
{
    using hanso::CaptureType;
    check(hanso::previewComplementCabDefaultForCaptureType(CaptureType::Amp)
              && hanso::previewComplementCabDefaultForCaptureType(CaptureType::PreAmp),
          "amp head and preamp previews attach the complement cab");
    check(! hanso::previewComplementCabDefaultForCaptureType(CaptureType::FullRig)
              && ! hanso::previewComplementCabDefaultForCaptureType(CaptureType::Pedal)
              && ! hanso::previewComplementCabDefaultForCaptureType(CaptureType::Effect)
              && ! hanso::previewComplementCabDefaultForCaptureType(CaptureType::Cabinet),
          "full rig, pedal, and cabinet previews stay uncolored");

    using hanso::HansoCategory;
    check(hanso::previewComplementCabDefaultForPackage("Amp", HansoCategory::Amp)
              && hanso::previewComplementCabDefaultForPackage("PreAmp", HansoCategory::Amp),
          "amp/preamp packages default to complement cab");
    check(! hanso::previewComplementCabDefaultForPackage("FullRig", HansoCategory::Rig)
              && ! hanso::previewComplementCabDefaultForPackage("Pedal", HansoCategory::Pedal),
          "full rig / pedal packages default to no complement cab");
    check(! hanso::previewComplementCabDefaultForPackage("", HansoCategory::Rig),
          "device-type-less rig package falls back to category");
    check(hanso::previewComplementCabDefaultForPackage("", HansoCategory::Unknown),
          "unknown package keeps legacy cab-on default");
}

void testModelDataChainCoverage()
{
    const auto coverageFor = [](const juce::String& deviceType, hanso::HansoCategory category)
    {
        hanso::HansoPackage package;
        package.metadata.deviceType = deviceType;
        package.metadata.category = category;
        const auto metadataVar = package.createMetadataVar();
        const auto* object = metadataVar.getDynamicObject();
        if (object == nullptr)
            return juce::StringArray();

        const auto modelData = object->getProperty("modelData");
        const auto* coverage = modelData.getProperty("chainCoverage", {}).getArray();
        juce::StringArray names;
        if (coverage != nullptr)
            for (const auto& entry : *coverage)
                names.add(entry.toString());
        return names;
    };

    check(coverageFor("FullRig", hanso::HansoCategory::Rig) == juce::StringArray { "amp", "cabinet" },
          "full rig covers amp and cabinet");
    check(coverageFor("Amp", hanso::HansoCategory::Amp) == juce::StringArray { "amp" },
          "amp head covers amp only");
    check(coverageFor("PreAmp", hanso::HansoCategory::Amp) == juce::StringArray { "preamp" },
          "preamp covers preamp only");
    check(coverageFor("Pedal", hanso::HansoCategory::Pedal) == juce::StringArray { "pedal" },
          "pedal covers pedal only");
    check(coverageFor("Cabinet", hanso::HansoCategory::Cabinet) == juce::StringArray { "cabinet" },
          "cabinet covers cabinet only");
    check(coverageFor("", hanso::HansoCategory::Rig) == juce::StringArray { "amp", "cabinet" },
          "category fallback works without deviceType");
    check(coverageFor("", hanso::HansoCategory::Unknown).isEmpty(),
          "unknown packages omit chainCoverage");
}

void testPreviewRigChainCoexistence()
{
    hanso::CaptureAudioSource source;
    source.prepare(48000.0, 256, 2);

    hanso::CompactHansoModel rigModel;
    rigModel.anchors.push_back({});

    check(source.loadPreviewModel(rigModel), "amp slot loads a model");
    check(source.loadPreviewPedalModel(rigModel), "pedal slot loads a model");

    // Cabinet slot: minimal cabinet package with one real IR position.
    hanso::CaptureWizardState wizard(hanso::CaptureType::Cabinet);
    hanso::HansoPackage package;
    juce::AudioBuffer<float> ir(1, 512);
    ir.clear();
    ir.setSample(0, 0, 1.0f);
    const auto chunkId = hanso::impulseResponseChunkIdForCabinetPosition("cab-edge");
    package.setChunk(chunkId, "cabinet-ir", "audio/x-hanso-pcm16",
                     hanso::HansoAudioChunkCodec::encodePcm16Audio(ir, 48000.0),
                     48000.0, 1, ir.getNumSamples());
    wizard.markCabinetSlotImported("cab-edge", chunkId, "edge.wav", hanso::CabinetMicClass::Dynamic, "SM57");
    if (auto* slot = wizard.findCabinetSlot("cab-edge"))
        slot->toneProfile = hanso::CabinetToneProfiler::fromImpulseResponse(ir, 48000.0);
    package.cabinetProfile = wizard.toCabinetProfileVar("Coexist Cab", "SM57", "", "");

    juce::String error;
    check(source.loadPreviewCabinetPackage(package, error), "cabinet slot loads a package", error);
    check(source.hasPreviewModel() && source.hasPreviewPedalModel() && source.hasPreviewCabinetPackage(),
          "pedal, amp, and cabinet slots coexist in the preview rig");

    // End-to-end: the full chain renders audio for a preview sample.
    juce::AudioBuffer<float> sample(2, 4096);
    for (int channel = 0; channel < sample.getNumChannels(); ++channel)
        for (int i = 0; i < sample.getNumSamples(); ++i)
            sample.setSample(channel, i,
                             0.25f * std::sin(2.0f * juce::MathConstants<float>::pi * 220.0f
                                              * static_cast<float>(i) / 48000.0f));

    source.loadPreviewSample(sample);
    check(source.startPreviewSample(), "preview sample starts");

    juce::AudioBuffer<float> output(2, 256);
    output.clear();
    source.process(nullptr, 0, output);
    check(output.getMagnitude(0, 0, output.getNumSamples()) > 1.0e-4f,
          "preview rig chain produces audio through all slots",
          juce::String(output.getMagnitude(0, 0, output.getNumSamples()), 6));
    source.stopPreviewSample();
    source.release();
}

void testSerializerReExportOverwrites()
{
    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("hanso-lab-test-reexport.hanso");
    file.deleteFile();

    hanso::HansoPackage first;
    first.formatVersion = 2;
    first.metadata.name = "First Export";

    hanso::HansoPackage second;
    second.formatVersion = 2;
    second.metadata.name = "Second Export";

    juce::String error;
    check(hanso::HansoSerializer::writeToFile(first, file, error), "first export writes", error);
    check(hanso::HansoSerializer::writeToFile(second, file, error), "re-export over existing file writes", error);

    // Reference size: the same package written to a fresh file.
    const auto freshFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                               .getChildFile("hanso-lab-test-reexport-fresh.hanso");
    freshFile.deleteFile();
    check(hanso::HansoSerializer::writeToFile(second, freshFile, error), "fresh export writes", error);
    check(file.getSize() == freshFile.getSize(), "re-export truncates instead of appending",
          juce::String(file.getSize()) + " vs " + juce::String(freshFile.getSize()));
    freshFile.deleteFile();

    hanso::HansoPackage read;
    check(hanso::HansoSerializer::readFromFile(file, read, error)
              && read.metadata.name == "Second Export",
          "re-exported file reads back the latest package", read.metadata.name);
    file.deleteFile();
}

void testPreviewMicColorProcessor()
{
    // Produce a cabProfile with a real micMatrix through the wizard, exactly
    // like an exported package would carry.
    hanso::CaptureWizardState wizard(hanso::CaptureType::Cabinet);
    auto* edge = wizard.findCabinetSlot("cab-edge");
    if (edge == nullptr)
    {
        check(false, "cab-edge slot exists for preview mic color test");
        return;
    }

    edge->source = hanso::CabinetSlotSource::ImportedIr;
    edge->micClass = hanso::CabinetMicClass::Dynamic;
    edge->toneProfile.valid = true;
    edge->toneProfile.levelDb = -3.0f;
    edge->toneProfile.bandGainsDb = { 2.0f, 1.0f, 0.0f, -1.0f, 1.0f, -3.0f };

    hanso::HansoPackage package;
    package.cabinetProfile = wizard.toCabinetProfileVar("Test Cab", "SM57", "Test speaker", "");

    hanso::PreviewMicColorProcessor processor;
    processor.prepare(sampleRate, 2);
    check(processor.loadFromPackage(package), "preview mic color processor loads micMatrix");
    processor.setMicPositionNormalized(0.33f);

    juce::AudioBuffer<float> original(2, 512);
    for (int channel = 0; channel < original.getNumChannels(); ++channel)
        for (int i = 0; i < original.getNumSamples(); ++i)
            original.setSample(channel, i,
                               0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f
                                               * static_cast<float>(i) / static_cast<float>(sampleRate)));

    auto bypassed = original;
    processor.setTargetMicClass(hanso::CabinetMicClass::Unknown);
    processor.process(bypassed, bypassed.getNumSamples());
    auto bypassDelta = 0.0f;
    for (int i = 0; i < original.getNumSamples(); ++i)
        bypassDelta = juce::jmax(bypassDelta,
                                 std::abs(bypassed.getSample(0, i) - original.getSample(0, i)));
    check(bypassDelta < 1.0e-6f, "original mic selection is a strict bypass",
          juce::String(bypassDelta, 8));

    processor.reset();
    auto conePosition = original;
    processor.setMicPositionNormalized(0.0f);
    processor.setTargetMicClass(hanso::CabinetMicClass::Unknown);
    processor.process(conePosition, conePosition.getNumSamples());
    auto positionDelta = 0.0f;
    for (int i = 0; i < original.getNumSamples(); ++i)
        positionDelta = juce::jmax(positionDelta,
                                   std::abs(conePosition.getSample(0, i) - original.getSample(0, i)));
    check(positionDelta > 1.0e-3f, "estimated Cone position changes the real IR with its profile",
          juce::String(positionDelta, 6));

    processor.reset();
    auto swapped = original;
    processor.setMicPositionNormalized(0.33f);
    processor.setTargetMicClass(hanso::CabinetMicClass::Ribbon);
    processor.process(swapped, swapped.getNumSamples());
    auto swapDelta = 0.0f;
    for (int i = 0; i < original.getNumSamples(); ++i)
        swapDelta = juce::jmax(swapDelta,
                               std::abs(swapped.getSample(0, i) - original.getSample(0, i)));
    check(swapDelta > 1.0e-3f, "ribbon mic swap audibly changes the signal",
          juce::String(swapDelta, 6));

    hanso::PreviewMicColorProcessor emptyProcessor;
    emptyProcessor.prepare(sampleRate, 2);
    hanso::HansoPackage emptyPackage;
    check(! emptyProcessor.loadFromPackage(emptyPackage), "package without micMatrix stays inactive");
}

void testPreviewMicDistanceProcessor()
{
    juce::AudioBuffer<float> original(1, 4096);
    for (int i = 0; i < original.getNumSamples(); ++i)
        original.setSample(0, i,
                           0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 5000.0f
                                           * static_cast<float>(i) / static_cast<float>(sampleRate)));

    hanso::PreviewMicDistanceProcessor processor;
    processor.prepare(sampleRate, 1);

    auto closeMic = original;
    processor.setDistanceCm(hanso::PreviewMicDistanceProcessor::referenceDistanceCm);
    processor.process(closeMic, closeMic.getNumSamples());
    auto closeDelta = 0.0f;
    for (int i = 0; i < original.getNumSamples(); ++i)
        closeDelta = juce::jmax(closeDelta,
                                std::abs(closeMic.getSample(0, i) - original.getSample(0, i)));
    check(closeDelta < 1.0e-6f, "reference mic distance leaves the captured IR unchanged",
          juce::String(closeDelta, 8));

    processor.reset();
    auto farMic = original;
    processor.setDistanceCm(20.0f);
    processor.process(farMic, farMic.getNumSamples());
    check(farMic.getRMSLevel(0, 0, farMic.getNumSamples())
              < original.getRMSLevel(0, 0, original.getNumSamples()) * 0.5f,
          "farther mic distance attenuates the preview");
}

void testCabinetCaptureMicMetadata()
{
    hanso::CaptureWizardState wizard(hanso::CaptureType::Cabinet);

    wizard.applyCabinetCaptureMic(hanso::CabinetMicClass::Ribbon, "R-121");
    wizard.markCabinetSlotCaptured("cab-center", "cabinet/positions/cab-center/ir.pcm16");
    const auto* center = wizard.findCabinetSlot("cab-center");
    check(center != nullptr && center->micClass == hanso::CabinetMicClass::Ribbon
              && center->micModelName == "R-121",
          "captured slot inherits session capture mic");

    wizard.applyCabinetCaptureMic(hanso::CabinetMicClass::Dynamic, "SM57");
    check(center != nullptr && center->micClass == hanso::CabinetMicClass::Dynamic,
          "changing session mic retroactively updates captured slots");

    wizard.markCabinetSlotImported("cab-edge", "cabinet/positions/cab-edge/ir.pcm16", "edge.wav",
                                   hanso::CabinetMicClass::Condenser, "C414");
    wizard.applyCabinetCaptureMic(hanso::CabinetMicClass::Ribbon, "R-121");
    const auto* edge = wizard.findCabinetSlot("cab-edge");
    check(edge != nullptr && edge->micClass == hanso::CabinetMicClass::Condenser,
          "imported slot keeps its own mic metadata");

    wizard.setCaptureType(hanso::CaptureType::Cabinet);
    check(wizard.cabinetCaptureMicClass == hanso::CabinetMicClass::Unknown
              && wizard.cabinetCaptureMicModelName.isEmpty(),
          "new capture resets session mic");
}

// Independent RBJ biquad used to synthesize mic/position-colored IRs by real
// filtering. The estimator computes its curves analytically, so agreement here
// validates the whole de-embedding chain through a second path.
struct TestBiquad
{
    float b0 { 1.0f }, b1 { 0.0f }, b2 { 0.0f }, a1 { 0.0f }, a2 { 0.0f };
    float z1 { 0.0f }, z2 { 0.0f };

    enum class Type { LowPass, Peaking, LowShelf, HighShelf };

    static TestBiquad make(Type type, double freqHz, double q, double gainDb)
    {
        const auto w0 = 2.0 * juce::MathConstants<double>::pi * freqHz / sampleRate;
        const auto cosW0 = std::cos(w0);
        const auto sinW0 = std::sin(w0);
        const auto alpha = sinW0 / (2.0 * q);
        const auto A = std::pow(10.0, gainDb / 40.0);
        const auto sqrtA = std::sqrt(A);
        const auto twoSqrtAAlpha = 2.0 * sqrtA * alpha;

        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a0 = 1.0, a1 = 0.0, a2 = 0.0;
        switch (type)
        {
            case Type::LowPass:
                b0 = (1.0 - cosW0) * 0.5; b1 = 1.0 - cosW0; b2 = (1.0 - cosW0) * 0.5;
                a0 = 1.0 + alpha; a1 = -2.0 * cosW0; a2 = 1.0 - alpha;
                break;
            case Type::Peaking:
                b0 = 1.0 + alpha * A; b1 = -2.0 * cosW0; b2 = 1.0 - alpha * A;
                a0 = 1.0 + alpha / A; a1 = -2.0 * cosW0; a2 = 1.0 - alpha / A;
                break;
            case Type::LowShelf:
                b0 = A * ((A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha);
                b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosW0);
                b2 = A * ((A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha);
                a0 = (A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha;
                a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosW0);
                a2 = (A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha;
                break;
            case Type::HighShelf:
                b0 = A * ((A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha);
                b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW0);
                b2 = A * ((A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha);
                a0 = (A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha;
                a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosW0);
                a2 = (A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha;
                break;
        }

        TestBiquad biquad;
        biquad.b0 = static_cast<float>(b0 / a0);
        biquad.b1 = static_cast<float>(b1 / a0);
        biquad.b2 = static_cast<float>(b2 / a0);
        biquad.a1 = static_cast<float>(a1 / a0);
        biquad.a2 = static_cast<float>(a2 / a0);
        return biquad;
    }

    float process(float x)
    {
        const auto y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

void applyFilterChain(juce::AudioBuffer<float>& buffer, std::vector<TestBiquad> chain, float gainDb = 0.0f)
{
    auto* data = buffer.getWritePointer(0);
    const auto gain = std::pow(10.0f, gainDb / 20.0f);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        auto sample = data[i];
        for (auto& stage : chain)
            sample = stage.process(sample);
        data[i] = sample * gain;
    }
}

std::vector<TestBiquad> micFilterChain(hanso::CabinetMicClass micClass)
{
    using Type = TestBiquad::Type;
    switch (micClass)
    {
        case hanso::CabinetMicClass::Ribbon:
            return { TestBiquad::make(Type::Peaking, 3500.0, 0.7, 0.5),
                     TestBiquad::make(Type::Peaking, 420.0, 0.9, 4.0),
                     TestBiquad::make(Type::Peaking, 6500.0, 0.8, -4.0) };
        case hanso::CabinetMicClass::Condenser:
            return { TestBiquad::make(Type::Peaking, 5000.0, 0.7, 1.0),
                     TestBiquad::make(Type::Peaking, 11000.0, 0.5, 2.0) };
        case hanso::CabinetMicClass::Unknown:
        case hanso::CabinetMicClass::Dynamic:
        default:
            return { TestBiquad::make(Type::Peaking, 4200.0, 0.9, 3.0),
                     TestBiquad::make(Type::Peaking, 8000.0, 0.7, -5.0) };
    }
}

std::vector<TestBiquad> positionFilterChain(int positionIndex)
{
    using Type = TestBiquad::Type;
    struct Params { double shelfHz, shelfQ, shelfDb, peakHz, peakQ, peakDb, highHz, highQ, highDb, cutHz; };
    static const Params kParams[] = {
        { 120.0, 0.7, 2.5, 2500.0, 1.0, 2.0, 6500.0, 0.8, 3.5, 9000.0 },
        { 130.0, 0.7, 1.5, 2000.0, 1.0, 1.0, 6000.0, 0.8, 2.0, 8500.0 },
        { 150.0, 0.8, 2.0, 1500.0, 1.1, 3.5, 5500.0, 0.9, 0.5, 7500.0 },
        { 160.0, 0.7, 1.0, 2200.0, 1.0, -1.5, 5000.0, 0.8, -3.5, 6500.0 },
    };

    const auto& p = kParams[positionIndex];
    return { TestBiquad::make(Type::LowShelf, p.shelfHz, p.shelfQ, p.shelfDb),
             TestBiquad::make(Type::Peaking, p.peakHz, p.peakQ, p.peakDb),
             TestBiquad::make(Type::HighShelf, p.highHz, p.highQ, p.highDb),
             TestBiquad::make(Type::LowPass, p.cutHz, 0.707, 0.0) };
}

// Broadband synthetic "neutral cabinet" IR the coloration chains are applied to.
juce::AudioBuffer<float> makeBaseCabinetIr()
{
    juce::AudioBuffer<float> ir(1, 8192);
    ir.clear();
    ir.setSample(0, 0, 1.0f);
    applyFilterChain(ir, { TestBiquad::make(TestBiquad::Type::LowPass, 6500.0, 0.707, 0.0),
                           TestBiquad::make(TestBiquad::Type::Peaking, 2000.0, 0.8, 3.0),
                           TestBiquad::make(TestBiquad::Type::LowShelf, 120.0, 0.7, 2.0) });
    return ir;
}

void testMicClassSuggestion()
{
    using hanso::CabinetMicClass;
    check(hanso::suggestMicClassForModelName("Shure SM57") == CabinetMicClass::Dynamic,
          "SM57 suggests dynamic");
    check(hanso::suggestMicClassForModelName("royer r-121") == CabinetMicClass::Ribbon,
          "R-121 suggests ribbon");
    check(hanso::suggestMicClassForModelName("AKG C414 XLS") == CabinetMicClass::Condenser,
          "C414 suggests condenser");
    check(hanso::suggestMicClassForModelName("My Custom Mic") == CabinetMicClass::Unknown,
          "unrecognised model stays unknown");
    check(hanso::cabinetMicClassFromString(hanso::toString(CabinetMicClass::Ribbon)) == CabinetMicClass::Ribbon,
          "mic class string round-trips");
}

void testMicColorationCurves()
{
    const auto dynamicBands = hanso::MicColorationProfiles::micColorationBandsDb(hanso::CabinetMicClass::Dynamic);
    const auto ribbonBands = hanso::MicColorationProfiles::micColorationBandsDb(hanso::CabinetMicClass::Ribbon);
    const auto unknownBands = hanso::MicColorationProfiles::micColorationBandsDb(hanso::CabinetMicClass::Unknown);

    check(unknownBands == dynamicBands, "unknown mic assumes dynamic coloration");

    // Ribbon: strong low-mid (160-350 band) vs dynamic; dynamic: presence peak
    // (1.8k-4k band) vs ribbon.
    check(ribbonBands[1] > dynamicBands[1] + 1.0f, "ribbon low-mid exceeds dynamic",
          juce::String(ribbonBands[1], 2) + " / " + juce::String(dynamicBands[1], 2));
    check(dynamicBands[4] > ribbonBands[4] + 1.0f, "dynamic presence exceeds ribbon",
          juce::String(dynamicBands[4], 2) + " / " + juce::String(ribbonBands[4], 2));

    check(juce::String(hanso::kCabinetMicPositions[0].label) == "Cone"
              && juce::String(hanso::kCabinetMicPositions[1].label) == "Cone Edge"
              && juce::String(hanso::kCabinetMicPositions[2].label) == "Edge"
              && juce::String(hanso::kCabinetMicPositions[3].label) == "Off-Axis",
          "cabinet position slots use Cone/Cone Edge/Edge/Off-Axis terminology");

    const auto cone = hanso::MicColorationProfiles::positionColorationBandsDb(0);
    const auto offAxis = hanso::MicColorationProfiles::positionColorationBandsDb(3);
    check(cone[5] > offAxis[5] + 3.0f, "cone keeps more highs than off-axis",
          juce::String(cone[5], 2) + " / " + juce::String(offAxis[5], 2));
}

void testCabinetMicMatrixEstimator()
{
    // Reference capture: dynamic mic at cap edge (slot cab-edge, index 1).
    auto referenceIr = makeBaseCabinetIr();
    applyFilterChain(referenceIr, micFilterChain(hanso::CabinetMicClass::Dynamic));
    applyFilterChain(referenceIr, positionFilterChain(1), 0.0f);

    // Ground truth for two combinations the estimator must predict.
    auto ribbonEdgeIr = makeBaseCabinetIr();
    applyFilterChain(ribbonEdgeIr, micFilterChain(hanso::CabinetMicClass::Ribbon));
    applyFilterChain(ribbonEdgeIr, positionFilterChain(1), 0.0f);

    auto dynamicCenterIr = makeBaseCabinetIr();
    applyFilterChain(dynamicCenterIr, micFilterChain(hanso::CabinetMicClass::Dynamic));
    applyFilterChain(dynamicCenterIr, positionFilterChain(0), 0.5f);

    hanso::CaptureWizardState wizard(hanso::CaptureType::Cabinet);
    auto* edge = wizard.findCabinetSlot("cab-edge");
    check(edge != nullptr, "cab-edge slot exists");
    if (edge == nullptr)
        return;

    edge->source = hanso::CabinetSlotSource::ImportedIr;
    edge->micClass = hanso::CabinetMicClass::Dynamic;
    edge->micModelName = "SM57";
    edge->toneProfile = hanso::CabinetToneProfiler::fromImpulseResponse(referenceIr, sampleRate);
    check(edge->toneProfile.valid, "reference tone profile valid");

    const auto matrix = hanso::CabinetMicMatrixEstimator::estimate(wizard.cabinetSlots);
    check(matrix.valid, "mic matrix computed from one real slot");
    check(static_cast<int>(matrix.entries.size()) == 3 * hanso::cabinetMicPositionCount(),
          "mic matrix covers every mic class x position combination");

    const auto findEntry = [&matrix](hanso::CabinetMicClass micClass, const juce::String& positionId)
        -> const hanso::CabinetMicMatrixEntry*
    {
        for (const auto& entry : matrix.entries)
            if (entry.micClass == micClass && entry.positionId == positionId)
                return &entry;
        return nullptr;
    };

    const auto* measured = findEntry(hanso::CabinetMicClass::Dynamic, "cab-edge");
    check(measured != nullptr && measured->measured && ! measured->toneProfile.estimated,
          "reference combination stays measured");

    const auto maxBandError = [](const hanso::CabinetToneProfile& predicted,
                                 const hanso::CabinetToneProfile& truth)
    {
        auto maxError = 0.0f;
        for (size_t band = 0; band < predicted.bandGainsDb.size(); ++band)
            maxError = juce::jmax(maxError,
                                  std::abs(predicted.bandGainsDb[band] - truth.bandGainsDb[band]));
        return maxError;
    };

    const auto ribbonTruth = hanso::CabinetToneProfiler::fromImpulseResponse(ribbonEdgeIr, sampleRate);
    const auto* ribbonEntry = findEntry(hanso::CabinetMicClass::Ribbon, "cab-edge");
    check(ribbonEntry != nullptr && ribbonEntry->toneProfile.valid && ribbonEntry->toneProfile.estimated,
          "ribbon swap entry estimated");
    if (ribbonEntry != nullptr && ribbonTruth.valid)
    {
        const auto error = maxBandError(ribbonEntry->toneProfile, ribbonTruth);
        check(error < 2.0f, "mic swap prediction matches filtered ground truth",
              "max band error " + juce::String(error, 2) + " dB");
    }

    const auto centerTruth = hanso::CabinetToneProfiler::fromImpulseResponse(dynamicCenterIr, sampleRate);
    const auto* centerEntry = findEntry(hanso::CabinetMicClass::Dynamic, "cab-center");
    check(centerEntry != nullptr && centerEntry->toneProfile.valid && centerEntry->toneProfile.estimated,
          "position swap entry estimated");
    if (centerEntry != nullptr && centerTruth.valid)
    {
        const auto error = maxBandError(centerEntry->toneProfile, centerTruth);
        check(error < 2.0f, "position swap prediction matches filtered ground truth",
              "max band error " + juce::String(error, 2) + " dB");
        check(std::abs(centerEntry->toneProfile.levelDb - centerTruth.levelDb) < 3.0f,
              "position swap level within tolerance",
              juce::String(centerEntry->toneProfile.levelDb, 2) + " / "
                  + juce::String(centerTruth.levelDb, 2));
    }

    const auto matrixVar = wizard.toCabinetMicMatrixVar();
    const auto* matrixObject = matrixVar.getDynamicObject();
    check(matrixObject != nullptr
              && matrixObject->getProperty("status").toString() == "computed"
              && matrixObject->getProperty("entries").getArray() != nullptr
              && matrixObject->getProperty("entries").getArray()->size()
                     == 3 * hanso::cabinetMicPositionCount(),
          "mic matrix serializes with all entries");
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

void testMuteDropIdentity()
{
    // Pure decision function: a >=6 dB drop while muted confirms identity.
    check(hanso::CalibrationValidator::checkMuteDropIdentity(-20.0f, -40.0f), "mute drop 20 dB confirms identity");
    check(! hanso::CalibrationValidator::checkMuteDropIdentity(-20.0f, -22.0f), "mute drop 2 dB rejects identity");
    check(! hanso::CalibrationValidator::checkMuteDropIdentity(-120.0f, -120.0f), "mute drop on silence rejects");

    // A return whose energy sits beside the probe tones (as heavy IMD can do)
    // fails Goertzel dominance; the mute-drop confirmation must rescue it.
    const auto frequencies = hanso::TestSignalGenerator::multiSineFrequencies();
    juce::AudioBuffer<float> offBandReturn(1, static_cast<int>(sampleRate * 1.5));
    offBandReturn.clear();
    for (int i = 0; i < offBandReturn.getNumSamples(); ++i)
    {
        auto value = 0.0;
        for (const auto frequency : frequencies)
        {
            const auto t = static_cast<double>(i) / sampleRate;
            value += std::sin(juce::MathConstants<double>::twoPi * frequency * 0.92 * t);
            value += std::sin(juce::MathConstants<double>::twoPi * frequency * 1.08 * t);
        }

        offBandReturn.setSample(0, i, static_cast<float>(value / 20.0) * 0.14f);
    }

    const auto offBandRmsDb = rmsDbfs(offBandReturn);
    const auto withoutRescue = hanso::CalibrationValidator::validateProbe(offBandReturn,
                                                                          sampleRate,
                                                                          frequencies.data(),
                                                                          static_cast<int>(frequencies.size()),
                                                                          peakDbfs(offBandReturn),
                                                                          -33.0f,
                                                                          offBandRmsDb - 30.0f);
    check(withoutRescue.status == hanso::CalibrationValidationStatus::IdentityFailed,
          "off-band return fails Goertzel dominance",
          "dominance=" + juce::String(withoutRescue.toneDominanceDb, 1));

    const auto withRescue = hanso::CalibrationValidator::validateProbe(offBandReturn,
                                                                       sampleRate,
                                                                       frequencies.data(),
                                                                       static_cast<int>(frequencies.size()),
                                                                       peakDbfs(offBandReturn),
                                                                       -33.0f,
                                                                       offBandRmsDb - 30.0f,
                                                                       {},
                                                                       true);
    check(withRescue.status == hanso::CalibrationValidationStatus::Passed && withRescue.muteDropRescuedIdentity,
          "mute-drop confirmation rescues identity-failing return",
          "status=" + withRescue.code);

    // Rescue must not bypass the level/SNR gates: noise stays rejected even
    // with a (spurious) mute-drop confirmation.
    auto broadbandNoise = makeWhiteNoiseWithRms(1.5, -30.0f, 0x4d554521);
    const auto noiseWithRescue = hanso::CalibrationValidator::validateProbe(broadbandNoise,
                                                                            sampleRate,
                                                                            frequencies.data(),
                                                                            static_cast<int>(frequencies.size()),
                                                                            peakDbfs(broadbandNoise),
                                                                            -33.0f,
                                                                            -31.0f,
                                                                            {},
                                                                            true);
    check(noiseWithRescue.status != hanso::CalibrationValidationStatus::Passed,
          "mute-drop rescue does not bypass SNR gate",
          "status=" + noiseWithRescue.code);
}

void testFidelityEvaluatorAndRefinement()
{
    const auto sample = makeMultiSine(2.0, 0.3f);

    // ESR unit behaviour: identical buffers are near-perfect, uncorrelated
    // noise is a total miss (~0 dB or worse).
    check(hanso::ModelFidelityEvaluator::esrDb(sample, sample) <= -40.0f,
          "ESR of identical buffers is very low",
          juce::String(hanso::ModelFidelityEvaluator::esrDb(sample, sample), 1));

    const auto noiseA = makeWhiteNoiseWithRms(2.0, -20.0f, 0x45535231);
    const auto noiseB = makeWhiteNoiseWithRms(2.0, -20.0f, 0x45535232);
    check(hanso::ModelFidelityEvaluator::esrDb(noiseA, noiseB) > -3.0f,
          "ESR of uncorrelated noise is near 0 dB",
          juce::String(hanso::ModelFidelityEvaluator::esrDb(noiseA, noiseB), 1));

    // Refinement recovers a known anchor: the "real" recording is the render
    // of a ground-truth anchor; starting from a detuned anchor the coordinate
    // descent must reduce ESR and move drive toward the truth.
    hanso::CompactHansoModelAnchor truth;
    truth.parameterValue = 1.0f;
    truth.drive = 6.0f;
    truth.outputGainDb = -4.0f;
    truth.lowShelfDb = 2.0f;
    truth.midPeakDb = -1.0f;
    truth.highShelfDb = 3.0f;

    const auto real = hanso::ModelFidelityEvaluator::renderAnchor(sample, truth, sampleRate);

    auto detuned = truth;
    detuned.drive = 3.0f;
    detuned.outputGainDb = -6.0f;

    std::vector<const juce::AudioBuffer<float>*> dryPointers { &sample };
    std::vector<const juce::AudioBuffer<float>*> realPointers { &real };
    auto esrBefore = 0.0f;
    auto esrAfter = 0.0f;
    const auto refined = hanso::ModelFidelityEvaluator::refineAnchor(detuned,
                                                                     dryPointers,
                                                                     realPointers,
                                                                     sampleRate,
                                                                     esrBefore,
                                                                     esrAfter);
    check(esrAfter < esrBefore, "refinement reduces ESR",
          juce::String(esrBefore, 1) + " -> " + juce::String(esrAfter, 1));
    check(std::abs(refined.drive - truth.drive) <= 1.0f, "refinement recovers ground-truth drive",
          "refined=" + juce::String(refined.drive, 2));
}

void testHighGainFirstUnlockChain()
{
    hanso::CaptureWizardState wizard(hanso::CaptureType::Amp);

    juce::StringArray anchorOrder;
    for (const auto& step : wizard.recipe.steps)
        if (step.isAnchorCapture())
            anchorOrder.add(step.stepId);

    check(anchorOrder.size() == 3
              && anchorOrder[0] == "gain-100"
              && anchorOrder[1] == "gain-050"
              && anchorOrder[2] == "gain-010",
          "amp recipe orders anchors gain-100 -> gain-050 -> gain-010",
          anchorOrder.joinIntoString(" -> "));

    const auto* calibration = wizard.findStep("calibration");
    check(calibration != nullptr && calibration->instructionText.contains(juce::String::fromUTF8("100%")),
          "calibration instruction references gain 100%");
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
    testMicClassSuggestion();
    testMicColorationCurves();
    testCabinetMicMatrixEstimator();
    testCabinetCaptureMicMetadata();
    testPreviewMicColorProcessor();
    testPreviewMicDistanceProcessor();
    testPreviewChainPolicy();
    testModelDataChainCoverage();
    testPreviewRigChainCoexistence();
    testSerializerReExportOverwrites();
    testQualityAnalyzer();
    testCalibrationValidator();
    testMuteDropIdentity();
    testFidelityEvaluatorAndRefinement();
    testHighGainFirstUnlockChain();

    if (failureCount > 0)
    {
        std::cout << failureCount << " check(s) failed." << std::endl;
        return 1;
    }

    std::cout << "All checks passed." << std::endl;
    return 0;
}

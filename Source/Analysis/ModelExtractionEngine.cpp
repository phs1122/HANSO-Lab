#include "Analysis/ModelExtractionEngine.h"

#include "Analysis/SpectrumUtils.h"
#include "Audio/AudioMetrics.h"
#include "Serialization/HansoAudioChunkCodec.h"
#include "Serialization/HansoModelCodec.h"

namespace hanso
{
namespace
{
struct AnchorSource
{
    juce::String dryChunkId;
    juce::String chunkId;
    float parameterValue { 0.0f };
};

float clampDb(float value, float minDb, float maxDb) noexcept
{
    return juce::jlimit(minDb, maxDb, std::isfinite(value) ? value : 0.0f);
}

bool decodeAudioChunk(const HansoPackage& package,
                      const juce::String& chunkId,
                      juce::AudioBuffer<float>& buffer,
                      double& sampleRate,
                      juce::String& error)
{
    // Resolves the current (.f32) id and falls back to legacy (.pcm16) assets,
    // decoding by the chunk's declared mediaType.
    return HansoAudioChunkCodec::loadAudioChunk(package, chunkId, buffer, sampleRate, error);
}

struct DriveFitResult
{
    float drive { 1.0f };
    bool fromMeasurement { false };
};

// Fits a tanh drive to the measured static input->output amplitude mapping.
// Matched quantiles of |dry| and |captured| recover the memoryless transfer
// curve exactly for any monotone waveshaper, independent of the test-signal
// envelope and of phase shifts inside the device.
DriveFitResult fitDriveFromTransfer(const juce::AudioBuffer<float>& dry,
                                    const juce::AudioBuffer<float>& captured)
{
    DriveFitResult result;

    const auto numSamples = juce::jmin(dry.getNumSamples(), captured.getNumSamples());
    if (numSamples < 4096 || dry.getNumChannels() == 0 || captured.getNumChannels() == 0)
        return result;

    const auto* dryData = dry.getReadPointer(0);
    const auto* capturedData = captured.getReadPointer(0);

    const auto stride = juce::jmax(1, numSamples / 200000);
    std::vector<float> dryMagnitudes, capturedMagnitudes;
    dryMagnitudes.reserve(static_cast<size_t>(numSamples / stride) + 1);
    capturedMagnitudes.reserve(static_cast<size_t>(numSamples / stride) + 1);
    for (int i = 0; i < numSamples; i += stride)
    {
        dryMagnitudes.push_back(std::abs(dryData[i]));
        capturedMagnitudes.push_back(std::abs(capturedData[i]));
    }

    std::sort(dryMagnitudes.begin(), dryMagnitudes.end());
    std::sort(capturedMagnitudes.begin(), capturedMagnitudes.end());

    const auto dryPeak = dryMagnitudes.back();
    const auto capturedPeak = capturedMagnitudes.back();
    if (dryPeak <= 1.0e-5f || capturedPeak <= 1.0e-6f)
        return result;

    // Quantiles from the mid range up: the lowest magnitudes are dominated by
    // noise, the extreme top by isolated peaks.
    std::vector<float> xs, ys;
    const auto count = dryMagnitudes.size();
    for (auto q = 0.30; q <= 0.996; q += 0.03)
    {
        const auto index = juce::jmin(count - 1, static_cast<size_t>(q * static_cast<double>(count)));
        xs.push_back(dryMagnitudes[index] / dryPeak);
        ys.push_back(capturedMagnitudes[index]);
    }

    if (xs.size() < 8 || xs.back() - xs.front() < 0.2f)
        return result;

    const auto yMax = ys.back();
    if (yMax <= 1.0e-6f)
        return result;

    for (auto& y : ys)
        y /= yMax;

    auto bestDrive = 1.0f;
    auto bestError = std::numeric_limits<float>::max();

    for (auto drive = 1.0f; drive <= 16.0f; drive += 0.25f)
    {
        const auto norm = std::tanh(drive);
        auto ty = 0.0f;
        auto tt = 0.0f;
        for (size_t k = 0; k < xs.size(); ++k)
        {
            const auto t = std::tanh(drive * xs[k]) / norm;
            ty += ys[k] * t;
            tt += t * t;
        }

        const auto scale = tt > 0.0f ? ty / tt : 0.0f;
        auto error = 0.0f;
        for (size_t k = 0; k < xs.size(); ++k)
        {
            const auto residual = ys[k] - scale * std::tanh(drive * xs[k]) / std::tanh(drive);
            error += residual * residual;
        }

        if (error < bestError)
        {
            bestError = error;
            bestDrive = drive;
        }
    }

    result.drive = bestDrive;
    result.fromMeasurement = true;
    return result;
}

struct BuiltAnchor
{
    CompactHansoModelAnchor anchor;
    float dryRmsDb { -120.0f };
    float capturedRmsDb { -120.0f };
    bool driveMeasured { false };
    bool usedLegacySharedDry { false };
};

BuiltAnchor buildAnchor(const juce::AudioBuffer<float>& dry,
                        const juce::AudioBuffer<float>& captured,
                        double sampleRate,
                        const AnchorSource& source)
{
    const auto dryLevels = measureBufferLevels(dry);
    const auto capturedLevels = measureBufferLevels(captured);
    const auto dryRmsDb = juce::Decibels::gainToDecibels(dryLevels.rms, -120.0f);
    const auto capturedRmsDb = juce::Decibels::gainToDecibels(capturedLevels.rms, -120.0f);
    const auto dryPeakDb = juce::Decibels::gainToDecibels(dryLevels.peak, -120.0f);
    const auto capturedPeakDb = juce::Decibels::gainToDecibels(capturedLevels.peak, -120.0f);

    const AveragedPowerSpectrum drySpectrum(dry, sampleRate);
    const AveragedPowerSpectrum capturedSpectrum(captured, sampleRate);

    const auto lowDelta = capturedSpectrum.bandPowerDb(60.0, 250.0) - drySpectrum.bandPowerDb(60.0, 250.0);
    const auto midDelta = capturedSpectrum.bandPowerDb(250.0, 2500.0) - drySpectrum.bandPowerDb(250.0, 2500.0);
    const auto highDelta = capturedSpectrum.bandPowerDb(2500.0, 10000.0) - drySpectrum.bandPowerDb(2500.0, 10000.0);

    // Band deltas describe the overall tilt; remove the average level shift so
    // they express tone shape rather than gain (gain lives in outputGainDb).
    const auto tiltMean = (lowDelta + midDelta + highDelta) / 3.0f;

    const auto driveFit = fitDriveFromTransfer(dry, captured);
    auto drive = driveFit.drive;
    if (! driveFit.fromMeasurement)
    {
        // Fallback: crest-factor reduction is a weak but measured distortion
        // proxy. The knob position is deliberately not used here.
        const auto dryCrest = dryPeakDb - dryRmsDb;
        const auto capturedCrest = capturedPeakDb - capturedRmsDb;
        drive = juce::jlimit(1.0f, 16.0f, 1.0f + juce::jmax(0.0f, dryCrest - capturedCrest) * 0.5f);
    }

    BuiltAnchor built;
    built.dryRmsDb = dryRmsDb;
    built.capturedRmsDb = capturedRmsDb;
    built.driveMeasured = driveFit.fromMeasurement;
    built.anchor.parameterValue = source.parameterValue;
    built.anchor.inputGainDb = 0.0f;
    // Each gain anchor is normalised against the dry signal that was actually
    // sent for that capture. This removes app-output level differences from
    // outputGainDb and puts every anchor back on the same "0 dB input" basis.
    // We deliberately do not remove per-anchor return-chain gain here: without
    // a separate loopback/return calibration signal it is indistinguishable
    // from real amp gain change. Users should keep the return chain fixed; the
    // monotonicity warning below catches obvious violations.
    built.anchor.outputGainDb = clampDb(capturedRmsDb - dryRmsDb, -36.0f, 24.0f);
    built.anchor.drive = juce::jlimit(1.0f, 16.0f, drive);
    built.anchor.lowShelfDb = clampDb(lowDelta - tiltMean, -18.0f, 18.0f);
    built.anchor.midPeakDb = clampDb(midDelta - tiltMean, -18.0f, 18.0f);
    built.anchor.highShelfDb = clampDb(highDelta - tiltMean, -18.0f, 18.0f);
    built.anchor.sourceChunkId = source.chunkId;
    return built;
}
}

juce::var ModelExtractionEngine::buildDspCoreFromAnalysis(const HansoPackage& package) const
{
    if (! package.dspCore.isVoid())
        return package.dspCore;

    const auto metadata = package.createMetadataVar();
    if (const auto* metadataObject = metadata.getDynamicObject())
        return metadataObject->getProperty("dspCore");

    return {};
}

ModelExtractionResult ModelExtractionEngine::extractIntoPackage(HansoPackage& package) const
{
    ModelExtractionResult result;

    static const AnchorSource sources[] {
        { "capture/gain-010/dry-reference.f32", "capture/gain-010/aligned-captured.f32", 0.1f },
        { "capture/gain-050/dry-reference.f32", "capture/gain-050/aligned-captured.f32", 0.5f },
        { "capture/gain-100/dry-reference.f32", "capture/gain-100/aligned-captured.f32", 1.0f }
    };

    CompactHansoModel model;
    model.outputChannels = juce::jmax(1, package.metadata.numOutputChannels);

    juce::StringArray warnings;
    std::vector<BuiltAnchor> builtAnchors;

    for (const auto& source : sources)
    {
        juce::AudioBuffer<float> captured;
        double capturedSampleRate = 0.0;
        juce::String decodeError;
        if (! decodeAudioChunk(package, source.chunkId, captured, capturedSampleRate, decodeError))
            continue;

        if (captured.getNumSamples() <= 0 || rmsDb(captured) <= -90.0f)
            continue;

        juce::AudioBuffer<float> dry;
        double drySampleRate = 0.0;
        juce::String dryDecodeError;
        auto usedLegacySharedDry = false;
        if (! decodeAudioChunk(package, source.dryChunkId, dry, drySampleRate, dryDecodeError))
        {
            usedLegacySharedDry = true;
            if (! decodeAudioChunk(package, "capture/shared/dry-reference.f32", dry, drySampleRate, dryDecodeError))
            {
                warnings.add("Skipped gain "
                             + juce::String(source.parameterValue * 100.0f, 0)
                             + "% anchor because dry reference is missing: "
                             + source.dryChunkId);
                continue;
            }
        }

        if (dry.getNumSamples() <= 0 || rmsDb(dry) <= -90.0f)
        {
            warnings.add("Skipped gain "
                         + juce::String(source.parameterValue * 100.0f, 0)
                         + "% anchor because dry reference is silent.");
            continue;
        }

        if (std::abs(drySampleRate - capturedSampleRate) > 1.0)
            warnings.add("Gain "
                         + juce::String(source.parameterValue * 100.0f, 0)
                         + "% dry/captured sample rates differ; using captured sample rate for analysis.");

        builtAnchors.push_back(buildAnchor(dry, captured, capturedSampleRate, source));
        builtAnchors.back().usedLegacySharedDry = usedLegacySharedDry;
        model.anchors.push_back(builtAnchors.back().anchor);

        if (model.sampleRate <= 0.0 || model.anchors.size() == 1)
        {
            model.sampleRate = drySampleRate > 0.0 ? drySampleRate : capturedSampleRate;
            model.inputChannels = juce::jmax(1, dry.getNumChannels());
        }
    }

    if (! model.isValid())
    {
        result.message = "Model extraction failed: no valid gain anchors were found.";
        return result;
    }

    auto measuredCount = 0;
    for (size_t i = 0; i < builtAnchors.size(); ++i)
    {
        if (builtAnchors[i].driveMeasured)
            ++measuredCount;

        if (builtAnchors[i].usedLegacySharedDry)
            warnings.add("Gain "
                         + juce::String(builtAnchors[i].anchor.parameterValue * 100.0f, 0)
                         + "% used legacy shared dry reference; recapture to make the anchor self-contained.");

        if (i > 0 && builtAnchors[i].anchor.outputGainDb < builtAnchors[i - 1].anchor.outputGainDb - 3.0f)
            warnings.add("Anchor level inconsistency: gain "
                         + juce::String(builtAnchors[i].anchor.parameterValue * 100.0f, 0)
                         + "% normalized output gain is lower than the previous anchor. Check gain knob positions and keep the return chain fixed.");
    }

    if (measuredCount < static_cast<int>(builtAnchors.size()))
        warnings.add(juce::String(static_cast<int>(builtAnchors.size()) - measuredCount)
                     + " anchor(s) fell back to crest-factor drive estimation (transfer fit had too little data).");

    package.setChunk(HansoModelCodec::compactModelChunkId,
                     "realtimePreviewModel",
                     "application/x-hanso-compact-model",
                     HansoModelCodec::encodeCompactModel(model),
                     model.sampleRate,
                     model.inputChannels,
                     0);

    package.circuitProfile.topologyName = "HANSO compact gain-tone preview model (measured transfer fit)";
    package.circuitProfile.exposedParameters.clear();
    package.circuitProfile.exposedParameters.add("Gain");
    package.circuitProfile.exposedParameters.add("Level");
    package.dynamicProfile.dynamicRangeDb = package.analysisSummary.dynamicRangeDb;
    package.residualModel.present = false;
    package.residualModel.architecture = "none";
    package.residualModel.trainingSummary = "Compact model extracted from measured static transfer fit; neural residual not trained.";

    if (const auto* previewAnchor = model.nearestAnchor(0.5f))
        package.analysisSummary.estimatedGainDb = previewAnchor->outputGainDb;

    package.dspCore = buildDspCoreFromAnalysis(package);

    result.success = true;
    result.model = std::move(model);
    result.message = "Compact model extracted: " + result.model.summary();
    if (! warnings.isEmpty())
        result.message += "\nWarnings:\n- " + warnings.joinIntoString("\n- ");

    return result;
}
}

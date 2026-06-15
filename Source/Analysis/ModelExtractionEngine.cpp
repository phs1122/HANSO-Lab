#include "Analysis/ModelExtractionEngine.h"

#include "Audio/AudioMetrics.h"
#include "Serialization/HansoAudioChunkCodec.h"
#include "Serialization/HansoModelCodec.h"

namespace hanso
{
namespace
{
struct AnchorSource
{
    juce::String chunkId;
    float parameterValue { 0.0f };
};

float clampDb(float value, float minDb, float maxDb) noexcept
{
    return juce::jlimit(minDb, maxDb, std::isfinite(value) ? value : 0.0f);
}

float bandLevelDb(const juce::AudioBuffer<float>& buffer, double sampleRate, double lowHz, double highHz)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() <= 0 || sampleRate <= 0.0)
        return -120.0f;

    constexpr auto fftOrder = 14;
    constexpr auto fftSize = 1 << fftOrder;
    const auto usableSamples = juce::jmin(fftSize, buffer.getNumSamples());
    if (usableSamples <= 8)
        return -120.0f;

    juce::dsp::FFT fft(fftOrder);
    juce::HeapBlock<float> fftData(static_cast<size_t>(fftSize * 2));
    juce::zeromem(fftData.getData(), sizeof(float) * static_cast<size_t>(fftSize * 2));

    for (int sample = 0; sample < usableSamples; ++sample)
    {
        const auto window = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * static_cast<float>(sample)
                                                   / static_cast<float>(juce::jmax(1, usableSamples - 1)));
        fftData[sample] = buffer.getSample(0, sample) * window;
    }

    fft.performFrequencyOnlyForwardTransform(fftData.getData());

    auto magnitudeSum = 0.0f;
    auto count = 0;
    const auto maxBin = fftSize / 2;
    for (int bin = 1; bin < maxBin; ++bin)
    {
        const auto hz = sampleRate * static_cast<double>(bin) / static_cast<double>(fftSize);
        if (hz >= lowHz && hz < highHz)
        {
            magnitudeSum += fftData[bin];
            ++count;
        }
    }

    if (count <= 0)
        return -120.0f;

    return juce::Decibels::gainToDecibels(magnitudeSum / static_cast<float>(count), -120.0f);
}

bool decodePcm16Chunk(const HansoPackage& package,
                      const juce::String& chunkId,
                      juce::AudioBuffer<float>& buffer,
                      double& sampleRate,
                      juce::String& error)
{
    const auto* chunk = package.findChunk(chunkId);
    if (chunk == nullptr)
    {
        error = "Missing chunk: " + chunkId;
        return false;
    }

    if (chunk->mediaType != "audio/x-hanso-pcm16")
    {
        error = "Unsupported audio chunk encoding for model extraction: " + chunkId;
        return false;
    }

    return HansoAudioChunkCodec::decodePcm16Audio(chunk->data, buffer, sampleRate, error);
}

CompactHansoModelAnchor buildAnchor(const juce::AudioBuffer<float>& dry,
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

    const auto lowDelta = bandLevelDb(captured, sampleRate, 60.0, 250.0)
                        - bandLevelDb(dry, sampleRate, 60.0, 250.0);
    const auto midDelta = bandLevelDb(captured, sampleRate, 250.0, 2500.0)
                        - bandLevelDb(dry, sampleRate, 250.0, 2500.0);
    const auto highDelta = bandLevelDb(captured, sampleRate, 2500.0, 10000.0)
                         - bandLevelDb(dry, sampleRate, 2500.0, 10000.0);

    const auto dryCrest = dryPeakDb - dryRmsDb;
    const auto capturedCrest = capturedPeakDb - capturedRmsDb;
    const auto crestReduction = juce::jmax(0.0f, dryCrest - capturedCrest);

    CompactHansoModelAnchor anchor;
    anchor.parameterValue = source.parameterValue;
    anchor.inputGainDb = 0.0f;
    anchor.outputGainDb = clampDb(capturedRmsDb - dryRmsDb, -36.0f, 24.0f);
    anchor.drive = juce::jlimit(1.0f, 16.0f, 1.0f + source.parameterValue * 8.0f + crestReduction * 0.2f);
    anchor.lowShelfDb = clampDb(lowDelta, -18.0f, 18.0f);
    anchor.midPeakDb = clampDb(midDelta, -18.0f, 18.0f);
    anchor.highShelfDb = clampDb(highDelta, -18.0f, 18.0f);
    anchor.sourceChunkId = source.chunkId;
    return anchor;
}
}

ModelExtractionResult ModelExtractionEngine::extractIntoPackage(HansoPackage& package) const
{
    ModelExtractionResult result;

    juce::AudioBuffer<float> dry;
    double drySampleRate = 0.0;
    juce::String error;
    if (! decodePcm16Chunk(package, "capture/shared/dry-reference.pcm16", dry, drySampleRate, error))
    {
        result.message = "Model extraction failed: " + error;
        return result;
    }

    if (dry.getNumSamples() <= 0 || rmsDb(dry) <= -90.0f)
    {
        result.message = "Model extraction failed: dry reference is silent.";
        return result;
    }

    static const AnchorSource sources[] {
        { "capture/gain-010/aligned-captured.pcm16", 0.1f },
        { "capture/gain-050/aligned-captured.pcm16", 0.5f },
        { "capture/gain-100/aligned-captured.pcm16", 1.0f }
    };

    CompactHansoModel model;
    model.sampleRate = drySampleRate;
    model.inputChannels = juce::jmax(1, dry.getNumChannels());
    model.outputChannels = juce::jmax(1, package.metadata.numOutputChannels);

    for (const auto& source : sources)
    {
        juce::AudioBuffer<float> captured;
        double capturedSampleRate = 0.0;
        juce::String decodeError;
        if (! decodePcm16Chunk(package, source.chunkId, captured, capturedSampleRate, decodeError))
            continue;

        if (captured.getNumSamples() <= 0 || rmsDb(captured) <= -90.0f)
            continue;

        model.anchors.push_back(buildAnchor(dry, captured, capturedSampleRate, source));
    }

    if (! model.isValid())
    {
        result.message = "Model extraction failed: no valid gain anchors were found.";
        return result;
    }

    package.setChunk(HansoModelCodec::compactModelChunkId,
                     "realtimePreviewModel",
                     "application/x-hanso-compact-model",
                     HansoModelCodec::encodeCompactModel(model),
                     model.sampleRate,
                     model.inputChannels,
                     0);

    package.circuitProfile.topologyName = "HANSO compact gain-tone preview model";
    package.circuitProfile.exposedParameters.clear();
    package.circuitProfile.exposedParameters.add("Gain");
    package.circuitProfile.exposedParameters.add("Level");
    package.dynamicProfile.dynamicRangeDb = package.analysisSummary.dynamicRangeDb;
    package.residualModel.present = false;
    package.residualModel.architecture = "none";
    package.residualModel.trainingSummary = "Phase 3 compact model extracted; neural residual not trained.";

    if (const auto* previewAnchor = model.nearestAnchor(0.5f))
        package.analysisSummary.estimatedGainDb = previewAnchor->outputGainDb;

    result.success = true;
    result.model = std::move(model);
    result.message = "Compact model extracted: " + result.model.summary();
    return result;
}
}

#include "App/LabWorkflow.h"

#include "Analysis/ModelFidelityEvaluator.h"
#include "Serialization/HansoAudioChunkCodec.h"
#include "Serialization/HansoModelCodec.h"

namespace hanso
{
namespace
{
// anchor.sourceChunkId looks like "capture/gain-100/aligned-captured.f32";
// the per-anchor sample recordings live under the same prefix.
juce::String anchorChunkPrefix(const CompactHansoModelAnchor& anchor)
{
    const auto marker = anchor.sourceChunkId.lastIndexOf("/aligned-captured");
    return marker > 0 ? anchor.sourceChunkId.substring(0, marker) : juce::String();
}

bool decodeAudioChunk(const HansoPackage& package,
                      const juce::String& chunkId,
                      juce::AudioBuffer<float>& buffer,
                      double& sampleRate)
{
    // Resolves the current (.f32) id and legacy (.pcm16) assets, decoding by
    // the chunk's declared mediaType.
    juce::String error;
    return HansoAudioChunkCodec::loadAudioChunk(package, chunkId, buffer, sampleRate, error);
}
}

LabWorkflow::LabWorkflow(ApplicationState& state, CaptureEngine& captureEngine, AnalysisEngine& analysisEngine)
    : appState(state),
      capture(captureEngine),
      analysis(analysisEngine)
{
}

AnalysisSummary LabWorkflow::runBasicAnalysis()
{
    capture.refresh();
    capture.alignCompletedCapture();
    return analysis.analyzeSession(capture.currentSession());
}

ModelExtractionResult LabWorkflow::extractCompactModel()
{
    auto result = modelExtraction.extractIntoPackage(appState.currentPackage());
    if (result.success)
    {
        runFidelityEvaluation(result);
        appState.markCaptureDataDirty();
        appState.appendLog(result.message);
    }
    else
    {
        appState.appendLog(result.message);
    }

    return result;
}

void LabWorkflow::runFidelityEvaluation(ModelExtractionResult& extraction)
{
    auto& package = appState.currentPackage();

    juce::StringArray sampleIds;
    for (const auto* chunk : package.chunks)
    {
        if (chunk->id.startsWith("capture/shared/sample-")
            && (chunk->id.endsWith(".f32") || chunk->id.endsWith(".pcm16")))
            sampleIds.addIfNotAlreadyThere(chunk->id.fromFirstOccurrenceOf("capture/shared/sample-", false, false)
                              .upToLastOccurrenceOf(".", false, false));
        else if (chunk->id.endsWith("/sample-hanso-probe-reference.f32")
                 || chunk->id.endsWith("/sample-hanso-probe-reference.pcm16"))
            sampleIds.addIfNotAlreadyThere("hanso-probe-reference");
    }

    if (sampleIds.isEmpty())
    {
        appState.appendLog("Fidelity evaluation skipped: package has no held-out probe or amp-processed sample recordings.");
        return;
    }

    juce::Array<juce::var> anchorReports;
    auto anyRefined = false;

    for (auto& anchor : extraction.model.anchors)
    {
        const auto prefix = anchorChunkPrefix(anchor);
        if (prefix.isEmpty())
            continue;

        std::vector<std::unique_ptr<juce::AudioBuffer<float>>> ownedBuffers;
        std::vector<const juce::AudioBuffer<float>*> dryPointers;
        std::vector<const juce::AudioBuffer<float>*> realPointers;
        juce::StringArray usedSampleIds;
        auto sampleRate = extraction.model.sampleRate;

        for (const auto& sampleId : sampleIds)
        {
            auto dry = std::make_unique<juce::AudioBuffer<float>>();
            auto real = std::make_unique<juce::AudioBuffer<float>>();
            double drySampleRate = 0.0;
            double realSampleRate = 0.0;
            const auto dryChunkId = sampleId == "hanso-probe-reference"
                                  ? prefix + "/probe-reference-dry.f32"
                                  : "capture/shared/sample-" + sampleId + ".f32";
            if (! decodeAudioChunk(package, dryChunkId, *dry, drySampleRate)
                || ! decodeAudioChunk(package, prefix + "/sample-" + sampleId + ".f32", *real, realSampleRate))
                continue;

            sampleRate = drySampleRate > 0.0 ? drySampleRate : sampleRate;
            dryPointers.push_back(dry.get());
            realPointers.push_back(real.get());
            ownedBuffers.push_back(std::move(dry));
            ownedBuffers.push_back(std::move(real));
            usedSampleIds.add(sampleId);
        }

        if (dryPointers.empty())
            continue;

        auto esrBeforeDb = 0.0f;
        auto esrAfterDb = 0.0f;
        const auto refined = ModelFidelityEvaluator::refineAnchor(anchor,
                                                                  dryPointers,
                                                                  realPointers,
                                                                  sampleRate,
                                                                  esrBeforeDb,
                                                                  esrAfterDb);
        const auto corrected = esrAfterDb < esrBeforeDb - 0.01f;
        if (corrected)
        {
            anchor = refined;
            anyRefined = true;
        }

        juce::Array<juce::var> sampleReports;
        for (size_t i = 0; i < dryPointers.size(); ++i)
        {
            const auto fidelity = ModelFidelityEvaluator::evaluate(*dryPointers[i],
                                                                   *realPointers[i],
                                                                   anchor,
                                                                   sampleRate);
            auto sampleReport = new juce::DynamicObject();
            sampleReport->setProperty("sampleId", usedSampleIds[static_cast<int>(i)]);
            sampleReport->setProperty("esrDb", fidelity.esrDb);
            sampleReport->setProperty("lowBandErrorDb", fidelity.lowBandErrorDb);
            sampleReport->setProperty("midBandErrorDb", fidelity.midBandErrorDb);
            sampleReport->setProperty("highBandErrorDb", fidelity.highBandErrorDb);
            sampleReport->setProperty("crestFactorDeltaDb", fidelity.crestFactorDeltaDb);
            sampleReports.add(sampleReport);
        }

        auto anchorReport = new juce::DynamicObject();
        anchorReport->setProperty("parameterValue", anchor.parameterValue);
        anchorReport->setProperty("esrBeforeDb", esrBeforeDb);
        anchorReport->setProperty("esrAfterDb", corrected ? esrAfterDb : esrBeforeDb);
        anchorReport->setProperty("corrected", corrected);
        anchorReport->setProperty("adoptedSource", corrected ? "sample-esr-refinement" : "probe-transfer-fit");
        anchorReport->setProperty("samples", sampleReports);

        juce::AudioBuffer<float> repeatStart;
        juce::AudioBuffer<float> repeatEnd;
        double repeatStartRate = 0.0;
        double repeatEndRate = 0.0;
        if (decodeAudioChunk(package, prefix + "/probe-reference-a-start.f32", repeatStart, repeatStartRate)
            && decodeAudioChunk(package, prefix + "/probe-reference-a-end.f32", repeatEnd, repeatEndRate))
        {
            anchorReport->setProperty("repeatabilityEsrDb",
                                      ModelFidelityEvaluator::esrDb(repeatStart, repeatEnd));
        }
        anchorReports.add(anchorReport);

        appState.appendLog("Fidelity: gain " + juce::String(anchor.parameterValue * 100.0f, 0)
                           + "% ESR " + juce::String(esrBeforeDb, 1) + " dB"
                           + (corrected ? " -> " + juce::String(esrAfterDb, 1) + " dB (refined)"
                                        : " (sweep fit kept)"));
    }

    if (anchorReports.isEmpty())
    {
        appState.appendLog("Fidelity evaluation skipped: no anchor had matching sample recordings.");
        return;
    }

    auto fidelityObject = new juce::DynamicObject();
    fidelityObject->setProperty("metric", "esr-strict");
    fidelityObject->setProperty("anchors", anchorReports);
    package.captureFidelity = fidelityObject;

    if (anyRefined)
    {
        package.setChunk(HansoModelCodec::compactModelChunkId,
                         "realtimePreviewModel",
                         "application/x-hanso-compact-model",
                         HansoModelCodec::encodeCompactModel(extraction.model),
                         extraction.model.sampleRate,
                         extraction.model.inputChannels,
                         0);
        extraction.message += "\nAnchors refined against real sample recordings (see captureFidelity).";
    }
}

juce::String LabWorkflow::formatSummary(const AnalysisSummary& summary)
{
    return "Peak " + juce::String(summary.peakLevelDb, 1)
         + " dBFS / RMS " + juce::String(summary.rmsLevelDb, 1)
         + " dBFS / Latency " + juce::String(summary.estimatedLatencyMs, 2) + " ms";
}
}

#include "Analysis/AnalysisEngine.h"

#include "Audio/AudioMetrics.h"

namespace hanso
{
AnalysisEngine::AnalysisEngine(ApplicationState& state)
    : appState(state)
{
}

AnalysisSummary AnalysisEngine::analyzeSession(const CaptureSession& session)
{
    AnalysisSummary summary;

    const auto& dry = session.dryReferenceSignal();
    const auto& captured = session.alignedCapturedSignal().getNumSamples() > 0
                         ? session.alignedCapturedSignal()
                         : session.capturedSignal();

    if (captured.getNumChannels() == 0 || captured.getNumSamples() == 0)
    {
        appState.appendLog("Analysis skipped: no captured audio.");
        return summary;
    }

    const auto levels = measureBufferLevels(captured);
    const auto peak = levels.peak;
    const auto rms = levels.rms;
    const auto frequency = frequencyAnalyzer.analyze(captured, session.getSampleRate());
    const auto transfer = transferAnalyzer.analyze(dry, captured);
    const auto dynamics = dynamicAnalyzer.analyze(captured, session.getSampleRate());

    summary.peakLevelDb = juce::Decibels::gainToDecibels(peak, -120.0f);
    summary.rmsLevelDb = juce::Decibels::gainToDecibels(rms, -120.0f);
    summary.estimatedLatencySamples = session.estimatedLatencySamples();
    summary.estimatedLatencyMs = session.estimatedLatencyMilliseconds();
    summary.lowBandDb = frequency.lowBandDb;
    summary.midBandDb = frequency.midBandDb;
    summary.highBandDb = frequency.highBandDb;
    summary.estimatedGainDb = transfer.estimatedGainDb;
    summary.dynamicRangeDb = dynamics.dynamicRangeDb;
    summary.envelopeAttackMs = dynamics.envelopeAttackMs;
    summary.envelopeReleaseMs = dynamics.envelopeReleaseMs;

    auto& package = appState.currentPackage();
    package.analysisSummary = summary;
    package.circuitProfile.topologyName = "Phase 1 capture-derived placeholder";
    package.circuitProfile.exposedParameters.clear();
    package.circuitProfile.exposedParameters.add("Gain");
    package.circuitProfile.exposedParameters.add("Tone");
    package.circuitProfile.exposedParameters.add("Level");
    package.dynamicProfile.dynamicRangeDb = summary.dynamicRangeDb;
    package.dynamicProfile.transientScore = summary.envelopeAttackMs > 0.0f ? 1000.0f / summary.envelopeAttackMs : 0.0f;

    const auto residualDataset = residualDatasetBuilder.build(dry,
                                                              captured,
                                                              session.getSampleRate(),
                                                              2048,
                                                              1024);
    package.residualDataset.prepared = residualDataset.inputSegments.size() > 0
                                    && residualDataset.targetSegments.size() > 0;
    package.residualDataset.sampleRate = residualDataset.sampleRate;
    package.residualDataset.normalizationGain = residualDataset.normalizationGain;
    package.residualDataset.segmentLength = residualDataset.segmentLength;
    package.residualDataset.hopSize = residualDataset.hopSize;
    package.residualDataset.inputSegmentCount = residualDataset.inputSegments.size();
    package.residualDataset.targetSegmentCount = residualDataset.targetSegments.size();

    appState.appendLog("Analysis completed.");
    return summary;
}
}

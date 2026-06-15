#include "App/LabWorkflow.h"

namespace hanso
{
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
        appState.markCaptureDataDirty();
        appState.appendLog(result.message);
    }
    else
    {
        appState.appendLog(result.message);
    }

    return result;
}

juce::String LabWorkflow::formatSummary(const AnalysisSummary& summary)
{
    return "Peak " + juce::String(summary.peakLevelDb, 1)
         + " dBFS / RMS " + juce::String(summary.rmsLevelDb, 1)
         + " dBFS / Latency " + juce::String(summary.estimatedLatencyMs, 2) + " ms";
}
}

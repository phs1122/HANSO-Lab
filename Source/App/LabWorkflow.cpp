#include "App/LabWorkflow.h"

namespace hanso
{
LabWorkflow::LabWorkflow(CaptureEngine& captureEngine, AnalysisEngine& analysisEngine)
    : capture(captureEngine),
      analysis(analysisEngine)
{
}

AnalysisSummary LabWorkflow::runBasicAnalysis()
{
    capture.refresh();
    capture.alignCompletedCapture();
    return analysis.analyzeSession(capture.currentSession());
}

juce::String LabWorkflow::formatSummary(const AnalysisSummary& summary)
{
    return "Peak " + juce::String(summary.peakLevelDb, 1)
         + " dBFS / RMS " + juce::String(summary.rmsLevelDb, 1)
         + " dBFS / Latency " + juce::String(summary.estimatedLatencyMs, 2) + " ms";
}
}

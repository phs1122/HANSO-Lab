#pragma once

#include "Analysis/AnalysisEngine.h"
#include "Capture/CaptureEngine.h"
#include "Model/AnalysisSummary.h"

namespace hanso
{
class LabWorkflow final
{
public:
    LabWorkflow(CaptureEngine& captureEngine, AnalysisEngine& analysisEngine);

    AnalysisSummary runBasicAnalysis();
    static juce::String formatSummary(const AnalysisSummary& summary);

private:
    CaptureEngine& capture;
    AnalysisEngine& analysis;
};
}

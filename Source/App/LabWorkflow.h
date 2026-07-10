#pragma once

#include "Analysis/AnalysisEngine.h"
#include "Analysis/ModelExtractionEngine.h"
#include "App/ApplicationState.h"
#include "Capture/CaptureEngine.h"
#include "Model/AnalysisSummary.h"

namespace hanso
{
class LabWorkflow final
{
public:
    LabWorkflow(ApplicationState& state, CaptureEngine& captureEngine, AnalysisEngine& analysisEngine);

    AnalysisSummary runBasicAnalysis();
    ModelExtractionResult extractCompactModel();
    static juce::String formatSummary(const AnalysisSummary& summary);

private:
    // Compares each extracted anchor against real amp-processed sample
    // recordings (when the package contains them), refines the anchors to
    // minimise ESR, and records a captureFidelity report in the manifest.
    void runFidelityEvaluation(ModelExtractionResult& extraction);

    ApplicationState& appState;
    CaptureEngine& capture;
    AnalysisEngine& analysis;
    ModelExtractionEngine modelExtraction;
};
}

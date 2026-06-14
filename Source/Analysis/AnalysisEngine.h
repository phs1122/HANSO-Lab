#pragma once

#include <JuceHeader.h>

#include "App/ApplicationState.h"
#include "Analysis/DynamicResponseAnalyzer.h"
#include "Analysis/FrequencyResponseAnalyzer.h"
#include "Analysis/TransferCurveAnalyzer.h"
#include "Capture/CaptureSession.h"

namespace hanso
{
class AnalysisEngine final
{
public:
    explicit AnalysisEngine(ApplicationState& state);

    AnalysisSummary analyzeSession(const CaptureSession& session);

private:
    ApplicationState& appState;
    FrequencyResponseAnalyzer frequencyAnalyzer;
    TransferCurveAnalyzer transferAnalyzer;
    DynamicResponseAnalyzer dynamicAnalyzer;
};
}

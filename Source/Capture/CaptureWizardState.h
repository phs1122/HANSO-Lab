#pragma once

#include <JuceHeader.h>

#include "Analysis/CaptureQualityReport.h"
#include "Capture/CaptureMode.h"
#include "Capture/CaptureRecipe.h"

namespace hanso
{
struct CaptureStepResult
{
    juce::String stepId;
    CaptureStepStatus status { CaptureStepStatus::NotStarted };
    CaptureQualityReport quality;
    juce::String dryChunkId;
    juce::String capturedChunkId;
    juce::String alignedChunkId;
};

class CaptureWizardState final
{
public:
    CaptureWizardState();

    CaptureMode mode { CaptureMode::Easy };
    CableGuideType cableGuide { CableGuideType::NormalTsCable };
    CaptureRecipe recipe;
    juce::String currentStepId;
    bool calibrationPassed { false };
    bool warningExportAccepted { false };

    CaptureStep* findStep(const juce::String& stepId) noexcept;
    const CaptureStep* findStep(const juce::String& stepId) const noexcept;
    CaptureStep* firstIncompleteRequiredStep() noexcept;
    const CaptureStep* firstIncompleteRequiredStep() const noexcept;

    void setStepStatus(const juce::String& stepId, CaptureStepStatus status);
    void storeResult(const CaptureStepResult& result);
    void removeResult(const juce::String& stepId);
    const CaptureStepResult* findResult(const juce::String& stepId) const noexcept;
    bool hasWarnings() const noexcept;
    bool isExportReady() const noexcept;
    juce::String exportDisabledReason() const;
    juce::var toMetadataVar() const;

private:
    std::vector<CaptureStepResult> results;
};
}

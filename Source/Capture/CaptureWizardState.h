#pragma once

#include <JuceHeader.h>

#include "Analysis/CaptureQualityReport.h"
#include "Capture/CabinetCaptureState.h"
#include "Capture/CaptureMode.h"
#include "Capture/CaptureRecipe.h"
#include "Capture/CaptureType.h"

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
    float dryPeakDbfs { -120.0f };
    float dryRmsDbfs { -120.0f };
    float captureOutputDbfs { -120.0f };
};

class CaptureWizardState final
{
public:
    CaptureWizardState();
    explicit CaptureWizardState(CaptureType type);

    CaptureType captureType { CaptureType::Amp };
    CaptureMode mode { CaptureMode::Easy };
    CableGuideType cableGuide { CableGuideType::NormalTsCable };
    CaptureRecipe recipe;
    juce::String currentStepId;
    bool calibrationPassed { false };
    bool warningExportAccepted { false };
    bool cabinetInterpolationComputed { false };
    std::vector<CabinetMicPositionSlot> cabinetSlots;

    void setCaptureType(CaptureType type);
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
    CabinetMicPositionSlot* findCabinetSlot(const juce::String& stepId) noexcept;
    const CabinetMicPositionSlot* findCabinetSlot(const juce::String& stepId) const noexcept;
    void markCabinetSlotCaptured(const juce::String& stepId, const juce::String& chunkId);
    void markCabinetSlotImported(const juce::String& stepId, const juce::String& chunkId, const juce::String& sourceFileName);
    void markCabinetSlotCapturing(const juce::String& stepId);
    void markCabinetSlotError(const juce::String& stepId, const juce::String& error);
    void resetCabinetSlot(const juce::String& stepId);
    bool hasCabinetRealSource() const noexcept;
    juce::StringArray cabinetRealSourceIds() const;
    void estimateEmptyCabinetSlots();
    juce::var toCabinetProfileVar(const juce::String& cabinetName,
                                  const juce::String& micType,
                                  const juce::String& speakerDescription,
                                  const juce::String& notes) const;

private:
    std::vector<CaptureStepResult> results;
};
}

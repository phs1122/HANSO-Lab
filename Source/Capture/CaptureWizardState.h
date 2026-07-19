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
    juce::String testSignalType;
    double testSignalDurationSeconds { 0.0 };
};

class CaptureWizardState final
{
public:
    CaptureWizardState();
    explicit CaptureWizardState(CaptureType type);

    CaptureType captureType { CaptureType::Amp };
    CaptureMode mode { CaptureMode::Easy };
    // Breakout cable (LEFT plug only) is the officially supported headphone
    // path; a single TS cable inserted into a stereo phones jack can short the
    // right output stage, so it is experimental-only (CAPTURE_CONNECTION_POLICY).
    CableGuideType cableGuide { CableGuideType::TrsToDualTsYCable };
    // Physical chain descriptors for metadata. Defaults are refined by the
    // onboarding questionnaire; setCaptureType() re-derives the return path.
    ExcitationPath excitationPath { ExcitationPath::DirectLine };
    ReturnPath returnPath { ReturnPath::Unknown };
    CaptureRecipe recipe;
    juce::String currentStepId;
    bool calibrationPassed { false };
    bool warningExportAccepted { false };
    bool cabinetInterpolationComputed { false };
    std::vector<CabinetMicPositionSlot> cabinetSlots;
    // Session-level mic used for direct cabinet captures. Applied to
    // captured-ir slots; imported slots carry their own per-import metadata.
    CabinetMicClass cabinetCaptureMicClass { CabinetMicClass::Unknown };
    juce::String cabinetCaptureMicModelName;

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
    void markCabinetSlotImported(const juce::String& stepId,
                                 const juce::String& chunkId,
                                 const juce::String& sourceFileName,
                                 CabinetMicClass micClass = CabinetMicClass::Unknown,
                                 const juce::String& micModelName = {});
    // Updates the session capture mic and retroactively applies it to every
    // captured-ir slot (imported slots keep their own metadata).
    void applyCabinetCaptureMic(CabinetMicClass micClass, const juce::String& micModelName);
    void markCabinetSlotCapturing(const juce::String& stepId);
    void markCabinetSlotError(const juce::String& stepId, const juce::String& error);
    void resetCabinetSlot(const juce::String& stepId);
    bool hasCabinetRealSource() const noexcept;
    juce::StringArray cabinetRealSourceIds() const;
    void estimateEmptyCabinetSlots();
    // Full (mic class x mic position) tone profile matrix estimated from the
    // real-source slots. Returns void var until at least one real slot has a
    // valid tone profile.
    juce::var toCabinetMicMatrixVar() const;
    juce::var toCabinetProfileVar(const juce::String& cabinetName,
                                  const juce::String& micType,
                                  const juce::String& speakerDescription,
                                  const juce::String& notes) const;

private:
    std::vector<CaptureStepResult> results;
};
}

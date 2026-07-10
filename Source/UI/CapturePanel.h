#pragma once

#include <JuceHeader.h>

#include "App/ApplicationState.h"
#include "App/LabWorkflow.h"
#include "Capture/CaptureEngine.h"
#include "Capture/CaptureMode.h"
#include "Capture/CaptureType.h"
#include "UI/CaptureStatusIcon.h"
#include "UI/CaptureInputControls.h"
#include "UI/LabPanel.h"
#include "UI/LevelMeter.h"

namespace hanso
{
class CapturePanel final : public LabPanel,
                           private juce::Timer
{
public:
    CapturePanel(ApplicationState& state, CaptureEngine& captureEngine, LabWorkflow& workflow);
    void startUpdating();
    void resized() override;
    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void setLayoutDebugEnabled(bool enabled);
    bool isLayoutDebugEnabled() const noexcept;
    // Applies an onboarding questionnaire result: capture type + mode, then
    // starts a fresh capture reflecting them.
    void applyOnboardingSelection(CaptureType type, CaptureMode mode);

private:
    void timerCallback() override;
    void applyCaptureSettings(int startChannel, int channelCount);
    void syncWizardUi();
    void rebuildStepRows();
    void updateCaptureTypeFromSelector();
    void applyCaptureType(CaptureType type);
    void selectStep(const juce::String& stepId);
    void updateModeFromButtons();
    void updateAdvancedVisibility();
    void updateCalibrationMeter();
    void handleStepButtonClicked(const juce::String& stepId);
    void updateStepActions();
    void runFinishCaptureAnalysis();
    void showImportIrChooser(const juce::String& stepId);
    void showImportIrMicMetadataDialog(const juce::String& stepId, const juce::File& file);
    void applyCaptureMicFromControls();
    void showAssetExportDialog();
    void updateCompletionActions();
    void showTonePreviewDialog();
    void confirmStartNewCapture();
    void showBusyOverlay(const juce::String& titleText, const juce::String& detailText, bool indeterminate);
    void hideBusyOverlay();
    void updateBusyOverlay();

    ApplicationState& appState;
    CaptureEngine& capture;
    LabWorkflow& labWorkflow;
    juce::Rectangle<int> leftCardBounds, rightCardBounds;   // Stage 2b card backgrounds
    juce::Label title;
    juce::Label statusLabel;
    juce::Label captureTypeLabel;
    juce::ComboBox captureTypeBox;
    juce::TextButton standardModeButton { "Standard Capture" };
    juce::TextButton easyModeButton { "Easy Capture" };
    juce::ComboBox cableGuideBox;
    juce::Label connectionGuideLabel;
    juce::Label safetyWarningLabel;
    juce::OwnedArray<CaptureStatusIcon> stepIcons;
    juce::OwnedArray<juce::TextButton> stepButtons;
    juce::OwnedArray<juce::TextButton> stepStartButtons;
    juce::OwnedArray<juce::TextButton> stepStopButtons;
    juce::OwnedArray<juce::TextButton> stepResetButtons;
    juce::OwnedArray<juce::TextButton> stepImportButtons;
    juce::Label captureMicLabel;
    juce::ComboBox captureMicClassBox;
    juce::TextEditor captureMicModelEditor;
    std::unique_ptr<juce::FileChooser> irImportChooser;
    std::unique_ptr<juce::AlertWindow> irImportMetadataDialog;
    juce::Label instructionTitleLabel;
    juce::Label instructionLabel;
    juce::TextButton generateButton { "Generate Test Signal" };
    juce::ComboBox signalType;
    CaptureInputControls captureInputControls;
    juce::TextButton advancedButton { "Advanced" };
    bool advancedOpen { false };
    bool layoutDebugEnabled { false };
    LevelMeter calibrationOutputMeter;
    LevelMeter calibrationInputMeter;
    juce::Label calibrationMeterTitle;
    juce::Label calibrationOutputSliderLabel;
    juce::Slider calibrationOutputSlider;
    juce::Label calibrationOutputMeterLabel;
    juce::Label calibrationInputMeterLabel;
    juce::Slider levelSlider;
    juce::Slider durationSlider;
    juce::Label inputMeterLabel;
    juce::Label outputMeterLabel;
    juce::Label storedCaptureLabel;
    juce::Label latencyLabel;
    juce::Label qualityLabel;
    juce::Label exportReadinessLabel;
    juce::Label analysisSummaryLabel;
    juce::TextButton exportButton { "Export" };
    juce::TextButton previewButton { "Preview" };
    juce::TextButton newCaptureButton { "Start New Capture" };
    juce::Label busyBackground;
    juce::Label busyTitle;
    juce::Label busyDetail;
    bool busyOverlayVisible { false };
    bool busyIndeterminate { false };
    juce::String busyKind;
    double busyProgressValue { 0.0 };
    juce::ProgressBar busyProgress { busyProgressValue };
};
}

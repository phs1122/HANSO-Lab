#include "UI/CapturePanel.h"

#include "App/Utf8.h"
#include "UI/AssetPanel.h"
#include "UI/TonePreviewPanel.h"

namespace hanso
{
namespace
{
juce::String modeDescription(CaptureMode mode)
{
    if (mode == CaptureMode::Easy)
        return utf8("간편 캡쳐\nPhones Out → 일반 기타 케이블 → 장비 Input\n리앰프 박스가 없는 사용자를 위한 방식입니다.\n정확도는 정식 캡쳐보다 낮을 수 있으며, 앱은 Left 채널만 출력하고 Right 채널은 무음 처리합니다.");

    return utf8("정식 캡쳐\nLine Out → 리앰프 박스 → 장비 Input\n가장 정확한 방식입니다.");
}

juce::String cableGuideText(CableGuideType type)
{
    if (type == CableGuideType::TrsToDualTsYCable)
        return utf8("TRS to Dual TS Y 케이블\n- 사용 가능\n- Left TS만 장비 Input에 연결하세요\n- Right TS는 연결하지 마세요\n- 물리적으로 조금 더 안정적인 방식입니다\n- 캡쳐 체인 자체는 일반 TS 케이블 방식과 동일합니다");

    return utf8("일반 TS 기타 케이블\n- 사용 가능\n- 가장 일반적인 방식\n- 앱은 Left 채널만 사용합니다\n- Right 채널은 무음 처리됩니다\n- 리앰프 박스 방식보다 정확도는 낮을 수 있습니다");
}

juce::String qualityText(const CaptureStepResult* result)
{
    if (result == nullptr)
        return utf8("Quality report\n아직 이 단계의 캡쳐 결과가 없습니다.");

    juce::String text = "Quality report\n";
    text = text + juce::String("Status: ") + toString(result->status) + juce::String("\n");
    text = text + juce::String("Peak ") + juce::String(result->quality.peakDbfs, 1) + juce::String(" dBFS / RMS ")
         + juce::String(result->quality.rmsDbfs, 1) + juce::String(" dBFS\n");
    text = text + juce::String("Clips ") + juce::String(result->quality.clipSampleCount)
         + juce::String(" / Latency ") + juce::String(result->quality.latencyMs, 2)
         + juce::String(" ms");

    for (const auto& issue : result->quality.issues)
    {
        text = text + juce::String("\n- ") + issue.messageKorean;
        if (issue.suggestedActionKorean.isNotEmpty())
            text = text + juce::String(" ") + issue.suggestedActionKorean;
    }

    return text;
}

bool isComplete(CaptureStepStatus status) noexcept
{
    return status == CaptureStepStatus::Passed || status == CaptureStepStatus::Warning;
}

bool isGainCaptureStep(const CaptureStep& step) noexcept
{
    return step.anchor.parameterKey == "gain";
}

juce::String firstIncompleteGainStepId(const CaptureWizardState& wizard)
{
    for (const auto& step : wizard.recipe.steps)
        if (isGainCaptureStep(step) && ! isComplete(step.status))
            return step.stepId;

    return {};
}

bool setupAndCalibrationComplete(const CaptureWizardState& wizard)
{
    const auto* setup = wizard.findStep("setup");
    const auto* calibration = wizard.findStep("calibration");
    return setup != nullptr
        && calibration != nullptr
        && isComplete(setup->status)
        && isComplete(calibration->status)
        && wizard.calibrationPassed;
}

void drawDebugBounds(juce::Graphics& g,
                     const juce::Component& component,
                     const juce::String& name,
                     juce::Colour colour)
{
    if (! component.isVisible() || component.getBounds().isEmpty())
        return;

    const auto bounds = component.getBounds().toFloat();
    g.setColour(colour.withAlpha(0.12f));
    g.fillRect(bounds);
    g.setColour(colour.withAlpha(0.9f));
    g.drawRect(bounds, 1.0f);

    const auto text = name + "  "
                    + juce::String(component.getX()) + ","
                    + juce::String(component.getY()) + " "
                    + juce::String(component.getWidth()) + "x"
                    + juce::String(component.getHeight());
    auto label = component.getBounds().withHeight(16).reduced(1, 1);
    label.setWidth(juce::jmin(label.getWidth(), 260));
    g.setColour(juce::Colours::black.withAlpha(0.72f));
    g.fillRect(label);
    g.setColour(colour.brighter(0.35f));
    g.setFont(10.0f);
    g.drawFittedText(text, label.reduced(3, 0), juce::Justification::centredLeft, 1);
}
}

CapturePanel::CapturePanel(ApplicationState& state, CaptureEngine& captureEngine, LabWorkflow& workflow)
    : appState(state),
      capture(captureEngine),
      labWorkflow(workflow),
      captureInputControls([this](int startChannel, int channelCount)
      {
          applyCaptureSettings(startChannel, channelCount);
      })
{
    styleSectionTitle(title, "Capture Wizard");
    addAndMakeVisible(title);

    standardModeButton.setButtonText(utf8("정식 캡쳐"));
    easyModeButton.setButtonText(utf8("간편 캡쳐"));
    standardModeButton.setClickingTogglesState(true);
    easyModeButton.setClickingTogglesState(true);
    standardModeButton.setRadioGroupId(1001);
    easyModeButton.setRadioGroupId(1001);
    standardModeButton.onClick = [this] { updateModeFromButtons(); };
    easyModeButton.onClick = [this] { updateModeFromButtons(); };
    addAndMakeVisible(standardModeButton);
    addAndMakeVisible(easyModeButton);

    cableGuideBox.addItem(utf8("일반 TS 기타 케이블"), 1);
    cableGuideBox.addItem(utf8("TRS to Dual TS Y 케이블"), 2);
    cableGuideBox.setSelectedId(1);
    cableGuideBox.onChange = [this]
    {
        appState.captureWizard().cableGuide = cableGuideBox.getSelectedId() == 2
                                            ? CableGuideType::TrsToDualTsYCable
                                            : CableGuideType::NormalTsCable;
        appState.currentPackage().captureWorkflow = appState.captureWizard().toMetadataVar();
        syncWizardUi();
    };
    addAndMakeVisible(cableGuideBox);

    connectionGuideLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(connectionGuideLabel);

    safetyWarningLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    safetyWarningLabel.setJustificationType(juce::Justification::topLeft);
    safetyWarningLabel.setText(utf8("앰프의 Speaker Out을 오디오 인터페이스 Input에 직접 연결하지 마세요.\n반드시 스피커 캐비넷+마이크 또는 적절한 로드박스를 사용해야 합니다."), juce::dontSendNotification);
    addAndMakeVisible(safetyWarningLabel);

    for (const auto& step : appState.captureWizard().recipe.steps)
    {
        auto* icon = new CaptureStatusIcon();
        if (step.stepId == "setup")
            icon->onClick = [this] { capture.completeNonAudioStep("setup"); syncWizardUi(); };
        addAndMakeVisible(icon);
        stepIcons.add(icon);

        auto* button = new juce::TextButton(step.title);
        button->onClick = [this, id = step.stepId] { handleStepButtonClicked(id); };
        addAndMakeVisible(button);
        stepButtons.add(button);

        auto* start = new juce::TextButton("Start Capture");
        start->onClick = [this, id = step.stepId]
        {
            selectStep(id);
            showBusyOverlay("Capturing...", "Sending test signal and recording the return.", false);
            capture.startCaptureStep(id);
            syncWizardUi();
        };
        addAndMakeVisible(start);
        stepStartButtons.add(start);

        auto* recapture = new juce::TextButton("Re-Capture");
        recapture->onClick = [this, id = step.stepId]
        {
            selectStep(id);
            showBusyOverlay("Capturing...", "Re-capturing this gain anchor.", false);
            capture.startCaptureStep(id);
            syncWizardUi();
        };
        addAndMakeVisible(recapture);
        stepRecaptureButtons.add(recapture);

        auto* stop = new juce::TextButton("Stop");
        stop->onClick = [this] { capture.stop(); hideBusyOverlay(); syncWizardUi(); };
        addAndMakeVisible(stop);
        stepStopButtons.add(stop);

        auto* reset = new juce::TextButton("Reset");
        reset->onClick = [this, id = step.stepId]
        {
            capture.resetCaptureStep(id);
            syncWizardUi();
        };
        addAndMakeVisible(reset);
        stepResetButtons.add(reset);
    }

    instructionTitleLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    addAndMakeVisible(instructionTitleLabel);

    instructionLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(instructionLabel);

    captureInputControls.setModeLabelText("Input");
    addAndMakeVisible(captureInputControls);
    capture.setCaptureInputChannel(-1);
    capture.setCaptureChannelCount(2);
    captureInputControls.syncFromEngine(capture.captureInputChannel(), capture.captureChannelCount());

    advancedButton.setClickingTogglesState(true);
    advancedButton.onClick = [this]
    {
        advancedOpen = advancedButton.getToggleState();
        updateAdvancedVisibility();
        resized();
    };
    addAndMakeVisible(advancedButton);

    generateButton.onClick = [this]
    {
        const auto selected = signalType.getSelectedId();
        auto type = TestSignalType::LogSineSweep;
        if (selected == 2) type = TestSignalType::WhiteNoise;
        if (selected == 3) type = TestSignalType::PinkNoise;
        if (selected == 4) type = TestSignalType::ImpulseBurst;
        if (selected == 5) type = TestSignalType::MultiSine;

        capture.generateTestSignal(type,
                                   durationSlider.getValue(),
                                   static_cast<float>(levelSlider.getValue()));
    };
    addAndMakeVisible(generateButton);

    signalType.addItem("Log sine sweep", 1);
    signalType.addItem("White noise", 2);
    signalType.addItem("Pink noise", 3);
    signalType.addItem("Impulse burst", 4);
    signalType.addItem("Multi-sine", 5);
    signalType.setSelectedId(1);
    addAndMakeVisible(signalType);

    levelSlider.setRange(0.0, 0.5, 0.001);
    levelSlider.setValue(0.2);
    levelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 24);
    addAndMakeVisible(levelSlider);

    durationSlider.setRange(1.0, 20.0, 0.5);
    durationSlider.setValue(5.0);
    durationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 24);
    addAndMakeVisible(durationSlider);

    calibrationMeterTitle.setText("Level meters", juce::dontSendNotification);
    calibrationMeterTitle.setJustificationType(juce::Justification::centredLeft);
    calibrationOutputMeterLabel.setJustificationType(juce::Justification::topLeft);
    calibrationInputMeterLabel.setJustificationType(juce::Justification::topLeft);
    calibrationMeterTitle.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    calibrationOutputSliderLabel.setText(utf8("앱 출력 (목표 -42 ~ -24 dBFS)"), juce::dontSendNotification);
    calibrationOutputSliderLabel.setJustificationType(juce::Justification::centredLeft);
    calibrationOutputSlider.setRange(-50.0, -18.0, 0.5);
    calibrationOutputSlider.setValue(capture.calibrationOutputDb(), juce::dontSendNotification);
    calibrationOutputSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22);
    calibrationOutputSlider.setTextValueSuffix(" dBFS");
    calibrationOutputSlider.onValueChange = [this]
    {
        capture.setCalibrationOutputDb(static_cast<float>(calibrationOutputSlider.getValue()));
    };
    calibrationOutputMeter.setRange(-60.0f, 0.0f);
    calibrationOutputMeter.setTargetRange(-42.0f, -24.0f);
    calibrationInputMeter.setRange(-60.0f, 0.0f);
    calibrationInputMeter.setTargetRange(-36.0f, -8.0f);
    addAndMakeVisible(calibrationMeterTitle);
    addAndMakeVisible(calibrationOutputSliderLabel);
    addAndMakeVisible(calibrationOutputSlider);
    addAndMakeVisible(calibrationOutputMeterLabel);
    addAndMakeVisible(calibrationOutputMeter);
    addAndMakeVisible(calibrationInputMeterLabel);
    addAndMakeVisible(calibrationInputMeter);

    analysisSummaryLabel.setJustificationType(juce::Justification::centredLeft);
    analysisSummaryLabel.setText("Finish Capture runs analysis and validation before export.", juce::dontSendNotification);
    addAndMakeVisible(analysisSummaryLabel);

    exportButton.onClick = [this] { showAssetExportDialog(); };
    addAndMakeVisible(exportButton);

    previewButton.onClick = [this] { showTonePreviewDialog(); };
    previewButton.setTooltip("Open sample-based tone preview.");
    addAndMakeVisible(previewButton);

    newCaptureButton.onClick = [this] { confirmStartNewCapture(); };
    addAndMakeVisible(newCaptureButton);

    busyBackground.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(32, 35, 38));
    busyBackground.setColour(juce::Label::outlineColourId, juce::Colour::fromRGB(95, 115, 125));
    addAndMakeVisible(busyBackground);

    busyTitle.setJustificationType(juce::Justification::centred);
    busyTitle.setFont(juce::Font(19.0f, juce::Font::bold));
    addAndMakeVisible(busyTitle);

    busyDetail.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(busyDetail);
    addAndMakeVisible(busyProgress);

    for (auto* label : { &statusLabel, &inputMeterLabel, &outputMeterLabel, &storedCaptureLabel, &latencyLabel, &qualityLabel, &exportReadinessLabel })
    {
        label->setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(label);
    }

    qualityLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    exportReadinessLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    analysisSummaryLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    easyModeButton.setToggleState(true, juce::dontSendNotification);
    updateAdvancedVisibility();
    hideBusyOverlay();
    selectStep(appState.captureWizard().currentStepId);
    updateModeFromButtons();
}

void CapturePanel::startUpdating()
{
    startTimerHz(6);
}

void CapturePanel::resized()
{
    auto area = getLocalBounds().reduced(contentMargin);
    title.setBounds(area.removeFromTop(36));

    auto top = area.removeFromTop(44);
    standardModeButton.setBounds(top.removeFromLeft(140).reduced(0, 3));
    top.removeFromLeft(8);
    easyModeButton.setBounds(top.removeFromLeft(140).reduced(0, 3));
    top.removeFromLeft(16);
    cableGuideBox.setBounds(top.removeFromLeft(240).reduced(0, 5));
    statusLabel.setBounds(top);

    area.removeFromTop(10);
    const auto completionHeight = 52;
    const auto advancedHeight = advancedOpen ? 54 : 0;
    const auto columnsHeight = juce::jmin(area.getHeight(), juce::jmax(500, area.getHeight() - advancedHeight - completionHeight - 28));
    auto columns = area.removeFromTop(columnsHeight);
    auto left = columns.removeFromLeft(720);
    columns.removeFromLeft(18);
    const auto infoColumnWidth = juce::jmin(460, columns.getWidth());
    auto centre = columns.removeFromRight(infoColumnWidth);

    connectionGuideLabel.setBounds(left.removeFromTop(120));
    left.removeFromTop(10);
    safetyWarningLabel.setBounds(left.removeFromTop(82));
    left.removeFromTop(12);

    for (int i = 0; i < stepButtons.size(); ++i)
    {
        auto row = left.removeFromTop(34);
        auto iconArea = row.removeFromLeft(34);
        stepIcons[i]->setBounds(iconArea.withSizeKeepingCentre(28, 28));
        stepButtons[i]->setBounds(row.removeFromLeft(220).reduced(0, 2));
        row.removeFromLeft(8);
        stepStartButtons[i]->setBounds(row.removeFromLeft(106).reduced(0, 2));
        row.removeFromLeft(6);
        stepRecaptureButtons[i]->setBounds(row.removeFromLeft(106).reduced(0, 2));
        row.removeFromLeft(6);
        stepStopButtons[i]->setBounds(row.removeFromLeft(62).reduced(0, 2));
        row.removeFromLeft(6);
        stepResetButtons[i]->setBounds(row.removeFromLeft(62).reduced(0, 2));
        left.removeFromTop(4);
    }

    instructionTitleLabel.setBounds(centre.removeFromTop(34));
    instructionLabel.setBounds(centre.removeFromTop(122));

    const auto showLevelMeters = appState.captureWizard().currentStepId != "setup";
    const auto showCalibrationControls = appState.captureWizard().currentStepId == "calibration";
    if (showLevelMeters)
    {
        centre.removeFromTop(10);
        calibrationMeterTitle.setBounds(centre.removeFromTop(22));
        centre.removeFromTop(3);

        if (showCalibrationControls)
        {
            calibrationOutputSliderLabel.setBounds(centre.removeFromTop(22));
            calibrationOutputSlider.setBounds(centre.removeFromTop(26));
            centre.removeFromTop(8);
        }
        else
        {
            calibrationOutputSliderLabel.setBounds({});
            calibrationOutputSlider.setBounds({});
        }

        calibrationOutputMeterLabel.setBounds(centre.removeFromTop(38));
        calibrationOutputMeter.setBounds(centre.removeFromTop(20));
        centre.removeFromTop(8);
        calibrationInputMeterLabel.setBounds(centre.removeFromTop(38));
        calibrationInputMeter.setBounds(centre.removeFromTop(20));
        centre.removeFromTop(12);
    }
    else
    {
        calibrationMeterTitle.setBounds({});
        calibrationOutputSliderLabel.setBounds({});
        calibrationOutputSlider.setBounds({});
        calibrationOutputMeterLabel.setBounds({});
        calibrationOutputMeter.setBounds({});
        calibrationInputMeterLabel.setBounds({});
        calibrationInputMeter.setBounds({});
        centre.removeFromTop(10);
    }

    qualityLabel.setBounds(centre.removeFromTop(126));
    centre.removeFromTop(6);
    exportReadinessLabel.setBounds(centre.removeFromTop(46));
    centre.removeFromTop(8);
    inputMeterLabel.setBounds(centre.removeFromTop(24));
    outputMeterLabel.setBounds(centre.removeFromTop(24));
    storedCaptureLabel.setBounds(centre.removeFromTop(40));
    latencyLabel.setBounds(centre.removeFromTop(24));

    centre.removeFromTop(12);
    captureInputControls.layoutRows(centre.removeFromTop(32), true);

    area.removeFromTop(10);
    auto completion = area.removeFromTop(completionHeight);
    exportButton.setBounds(completion.removeFromLeft(130).reduced(0, 8));
    completion.removeFromLeft(12);
    previewButton.setBounds(completion.removeFromLeft(130).reduced(0, 8));
    completion.removeFromLeft(12);
    newCaptureButton.setBounds(completion.removeFromLeft(170).reduced(0, 8));
    completion.removeFromLeft(12);
    advancedButton.setBounds(completion.removeFromLeft(130).reduced(0, 8));
    analysisSummaryLabel.setBounds({});

    if (advancedOpen)
    {
        area.removeFromTop(8);
        auto advanced = area.removeFromTop(36);
        generateButton.setBounds(advanced.removeFromLeft(170).reduced(0, 2));
        advanced.removeFromLeft(8);
        signalType.setBounds(advanced.removeFromLeft(180).reduced(0, 2));
        advanced.removeFromLeft(8);
        levelSlider.setBounds(advanced.removeFromLeft(280).reduced(0, 2));
        advanced.removeFromLeft(8);
        durationSlider.setBounds(advanced.removeFromLeft(280).reduced(0, 2));
    }

    const auto overlay = getLocalBounds().withSizeKeepingCentre(420, 136);
    busyBackground.setBounds(overlay);
    auto busyArea = overlay.reduced(18, 14);
    busyTitle.setBounds(busyArea.removeFromTop(30));
    busyArea.removeFromTop(6);
    busyDetail.setBounds(busyArea.removeFromTop(32));
    busyArea.removeFromTop(12);
    busyProgress.setBounds(busyArea.removeFromTop(18));
}

void CapturePanel::paintOverChildren(juce::Graphics& g)
{
    if (! layoutDebugEnabled)
        return;

    drawDebugBounds(g, title, "title", juce::Colours::yellow);
    drawDebugBounds(g, standardModeButton, "standardMode", juce::Colours::orange);
    drawDebugBounds(g, easyModeButton, "easyMode", juce::Colours::orange);
    drawDebugBounds(g, cableGuideBox, "cableGuide", juce::Colours::orange);
    drawDebugBounds(g, statusLabel, "status", juce::Colours::orange);
    drawDebugBounds(g, connectionGuideLabel, "connectionGuide", juce::Colours::cyan);
    drawDebugBounds(g, safetyWarningLabel, "safetyWarning", juce::Colours::red);
    drawDebugBounds(g, instructionTitleLabel, "instructionTitle", juce::Colours::lightgreen);
    drawDebugBounds(g, instructionLabel, "instruction", juce::Colours::lightgreen);
    drawDebugBounds(g, captureInputControls, "captureInputControls", juce::Colours::lightgreen);
    drawDebugBounds(g, calibrationMeterTitle, "levelMetersTitle", juce::Colours::deepskyblue);
    drawDebugBounds(g, calibrationOutputSliderLabel, "calOutputSliderLabel", juce::Colours::deepskyblue);
    drawDebugBounds(g, calibrationOutputSlider, "calOutputSlider", juce::Colours::deepskyblue);
    drawDebugBounds(g, calibrationOutputMeterLabel, "outputMeterLabel", juce::Colours::deepskyblue);
    drawDebugBounds(g, calibrationOutputMeter, "outputMeter", juce::Colours::deepskyblue);
    drawDebugBounds(g, calibrationInputMeterLabel, "returnMeterLabel", juce::Colours::deepskyblue);
    drawDebugBounds(g, calibrationInputMeter, "returnMeter", juce::Colours::deepskyblue);
    drawDebugBounds(g, qualityLabel, "quality", juce::Colours::violet);
    drawDebugBounds(g, exportReadinessLabel, "exportReadiness", juce::Colours::violet);
    drawDebugBounds(g, inputMeterLabel, "inputText", juce::Colours::violet);
    drawDebugBounds(g, outputMeterLabel, "outputText", juce::Colours::violet);
    drawDebugBounds(g, storedCaptureLabel, "storedCapture", juce::Colours::violet);
    drawDebugBounds(g, latencyLabel, "latency", juce::Colours::violet);
    drawDebugBounds(g, exportButton, "export", juce::Colours::white);
    drawDebugBounds(g, previewButton, "preview", juce::Colours::white);
    drawDebugBounds(g, newCaptureButton, "newCapture", juce::Colours::white);
    drawDebugBounds(g, advancedButton, "advanced", juce::Colours::white);
    drawDebugBounds(g, generateButton, "generate", juce::Colours::grey);
    drawDebugBounds(g, signalType, "signalType", juce::Colours::grey);
    drawDebugBounds(g, levelSlider, "advancedLevel", juce::Colours::grey);
    drawDebugBounds(g, durationSlider, "advancedDuration", juce::Colours::grey);

    for (int i = 0; i < stepButtons.size(); ++i)
    {
        drawDebugBounds(g, *stepIcons[i], "stepIcon" + juce::String(i + 1), juce::Colours::lime);
        drawDebugBounds(g, *stepButtons[i], "step" + juce::String(i + 1), juce::Colours::lime);
        drawDebugBounds(g, *stepStartButtons[i], "start" + juce::String(i + 1), juce::Colours::limegreen);
        drawDebugBounds(g, *stepRecaptureButtons[i], "recap" + juce::String(i + 1), juce::Colours::limegreen);
        drawDebugBounds(g, *stepStopButtons[i], "stop" + juce::String(i + 1), juce::Colours::limegreen);
        drawDebugBounds(g, *stepResetButtons[i], "reset" + juce::String(i + 1), juce::Colours::limegreen);
    }
}

void CapturePanel::setLayoutDebugEnabled(bool enabled)
{
    layoutDebugEnabled = enabled;
    repaint();
}

bool CapturePanel::isLayoutDebugEnabled() const noexcept
{
    return layoutDebugEnabled;
}

void CapturePanel::timerCallback()
{
    capture.refresh();
    capture.updateLiveCalibration();
    updateBusyOverlay();
    captureInputControls.syncFromEngine(capture.captureInputChannel(), capture.captureChannelCount());
    updateCalibrationMeter();
    statusLabel.setText("State: " + capture.stateText(), juce::dontSendNotification);
    inputMeterLabel.setText("Input: " + juce::String(juce::Decibels::gainToDecibels(capture.inputLevel(), -120.0f), 1) + " dBFS",
                            juce::dontSendNotification);
    outputMeterLabel.setText("Output: " + juce::String(juce::Decibels::gainToDecibels(capture.outputLevel(), -120.0f), 1) + " dBFS",
                             juce::dontSendNotification);
    storedCaptureLabel.setText(capture.captureStorageText(), juce::dontSendNotification);
    latencyLabel.setText("Latency: " + juce::String(capture.estimatedLatencySamples())
                         + " samples / " + juce::String(capture.estimatedLatencyMs(), 2) + " ms",
                         juce::dontSendNotification);
    syncWizardUi();
}

void CapturePanel::applyCaptureSettings(int startChannel, int channelCount)
{
    capture.setCaptureInputChannel(startChannel);
    capture.setCaptureChannelCount(channelCount);
    appState.appendLog("Capture routing updated from Capture tab.");
}

void CapturePanel::syncWizardUi()
{
    const auto& wizard = appState.captureWizard();
    connectionGuideLabel.setText(modeDescription(wizard.mode) + "\n\n" + cableGuideText(wizard.cableGuide), juce::dontSendNotification);

    for (int i = 0; i < static_cast<int>(wizard.recipe.steps.size()) && i < stepIcons.size(); ++i)
    {
        const auto& rowStep = wizard.recipe.steps[static_cast<size_t>(i)];
        stepIcons[i]->setStatus(rowStep.status);
        stepButtons[i]->setToggleState(rowStep.stepId == wizard.currentStepId, juce::dontSendNotification);
        stepIcons[i]->setTooltipText({});

        if (rowStep.status == CaptureStepStatus::Warning || rowStep.status == CaptureStepStatus::Failed)
        {
            if (const auto* result = wizard.findResult(rowStep.stepId))
            {
                juce::String tooltip;
                for (const auto& issue : result->quality.issues)
                {
                    if (issue.severity == CaptureQualitySeverity::Warning
                        || issue.severity == CaptureQualitySeverity::Error)
                    {
                        if (tooltip.isNotEmpty())
                            tooltip += "\n";
                        tooltip += issue.messageKorean.isNotEmpty() ? issue.messageKorean : issue.messageEnglish;
                    }
                }
                stepIcons[i]->setTooltipText(tooltip);
            }
        }

        if (rowStep.stepId == "calibration")
        {
            if (rowStep.status == CaptureStepStatus::Passed)
                stepButtons[i]->setButtonText("Calibration Complete");
            else if (capture.isCalibrationMonitorRunning())
                stepButtons[i]->setButtonText("Calibration Stop");
            else
                stepButtons[i]->setButtonText("Calibration Start");
        }
        else
        {
            stepButtons[i]->setButtonText(rowStep.title);
        }
    }

    const auto* step = wizard.findStep(wizard.currentStepId);
    if (step != nullptr)
    {
        instructionTitleLabel.setText(step->title, juce::dontSendNotification);
        instructionLabel.setText(step->instructionText, juce::dontSendNotification);
        qualityLabel.setText(qualityText(wizard.findResult(step->stepId)), juce::dontSendNotification);
        const auto showLevelMeters = step->stepId != "setup";
        const auto showCalibrationControls = step->stepId == "calibration";
        const auto visibilityChanged = calibrationInputMeter.isVisible() != showLevelMeters
                                    || calibrationOutputSlider.isVisible() != showCalibrationControls;
        calibrationMeterTitle.setVisible(showLevelMeters);
        calibrationOutputSliderLabel.setVisible(showCalibrationControls);
        calibrationOutputSlider.setVisible(showCalibrationControls);
        calibrationOutputMeter.setVisible(showLevelMeters);
        calibrationOutputMeterLabel.setVisible(showLevelMeters);
        calibrationInputMeter.setVisible(showLevelMeters);
        calibrationInputMeterLabel.setVisible(showLevelMeters);

        if (visibilityChanged)
            resized();
    }

    const auto exportText = wizard.isExportReady()
                          ? (wizard.hasWarnings() ? utf8("Export ready with warnings: 경고가 있는 캡쳐가 있습니다.") : utf8("Export ready: 모든 필수 단계가 완료되었습니다."))
                          : wizard.exportDisabledReason();
    exportReadinessLabel.setText(exportText, juce::dontSendNotification);
    updateStepActions();
    updateCompletionActions();
}

void CapturePanel::selectStep(const juce::String& stepId)
{
    appState.captureWizard().currentStepId = stepId;
    syncWizardUi();
}

void CapturePanel::handleStepButtonClicked(const juce::String& stepId)
{
    auto& wizard = appState.captureWizard();
    wizard.currentStepId = stepId;

    if (stepId == "setup")
    {
        capture.completeNonAudioStep(stepId);
    }
    else if (stepId == "calibration")
    {
        if (capture.isCalibrationMonitorRunning())
            capture.stopCalibrationMonitor();
        else if (! wizard.calibrationPassed)
            capture.startCalibrationMonitor();
    }
    else if (stepId == "final-validation")
    {
        if (setupAndCalibrationComplete(wizard) && firstIncompleteGainStepId(wizard).isEmpty())
            runFinishCaptureAnalysis();
    }

    syncWizardUi();
}

void CapturePanel::updateModeFromButtons()
{
    auto& wizard = appState.captureWizard();
    wizard.mode = easyModeButton.getToggleState() ? CaptureMode::Easy : CaptureMode::Standard;
    appState.currentPackage().captureWorkflow = wizard.toMetadataVar();
    syncWizardUi();
}

void CapturePanel::updateAdvancedVisibility()
{
    advancedButton.setButtonText(advancedOpen ? "Advanced Hide" : "Advanced");
    generateButton.setVisible(advancedOpen);
    signalType.setVisible(advancedOpen);
    levelSlider.setVisible(advancedOpen);
    durationSlider.setVisible(advancedOpen);
}

void CapturePanel::updateCalibrationMeter()
{
    const auto inputDb = juce::Decibels::gainToDecibels(capture.inputLevel(), -120.0f);
    const auto outputDb = juce::Decibels::gainToDecibels(capture.outputLevel(), -120.0f);
    calibrationInputMeter.setLevel(inputDb);
    calibrationOutputMeter.setLevel(outputDb);

    juce::String inputState;
    if (inputDb < -36.0f)
        inputState = utf8("낮음: Phones 볼륨 또는 장비 출력을 조금 올리세요.");
    else if (inputDb > -8.0f)
        inputState = utf8("높음: Phones 볼륨 또는 장비 출력을 낮추세요.");
    else
        inputState = utf8("안전 범위");

    juce::String outputState;
    if (outputDb < -42.0f)
        outputState = utf8("낮음: 위 슬라이더로 앱 출력을 올리세요.");
    else if (outputDb > -24.0f)
        outputState = utf8("높음: 앱 테스트 신호가 너무 큽니다.");
    else
        outputState = utf8("안전 범위");

    calibrationOutputMeterLabel.setText(utf8("Output: ")
                                        + juce::String(outputDb, 1)
                                        + " dBFS / Target "
                                        + juce::String(capture.calibrationOutputDb(), 1)
                                        + " dBFS\n"
                                        + outputState,
                                        juce::dontSendNotification);

    calibrationInputMeterLabel.setText(utf8("Return: ")
                                       + juce::String(inputDb, 1)
                                       + " dBFS\nTarget -36 to -8 / "
                                       + inputState
                                       + (capture.isCalibrationLevelSafe() && capture.isCalibrationOutputLevelSafe()
                                            ? utf8(" / 3초")
                                            : juce::String()),
                                       juce::dontSendNotification);
}

void CapturePanel::runFinishCaptureAnalysis()
{
    auto& wizard = appState.captureWizard();
    auto* step = wizard.findStep("final-validation");
    if (step == nullptr)
        return;

    wizard.currentStepId = "final-validation";
    wizard.setStepStatus("final-validation", CaptureStepStatus::InProgress);
    appState.currentPackage().captureWorkflow = wizard.toMetadataVar();
    capture.stopOutputSignal();
    showBusyOverlay("Analyzing...", "Validating capture anchors and package readiness.", true);
    syncWizardUi();

    juce::MessageManager::callAsync([this]
    {
        const auto summary = labWorkflow.runBasicAnalysis();
        const auto extraction = labWorkflow.extractCompactModel();
        if (extraction.success)
            capture.loadPreviewModel(extraction.model);

        analysisSummaryLabel.setText("Analysis: " + LabWorkflow::formatSummary(summary)
                                     + (extraction.success ? " / Model ready" : " / Model extraction skipped"),
                                     juce::dontSendNotification);
        capture.completeNonAudioStep("final-validation");
        hideBusyOverlay();
        syncWizardUi();
        appState.appendLog("Finish Capture analysis, validation, and model extraction completed.");
    });
}

void CapturePanel::showAssetExportDialog()
{
    if (! appState.captureWizard().isExportReady())
    {
        appState.appendLog(appState.captureWizard().exportDisabledReason());
        return;
    }

    auto* panel = new AssetPanel(appState);
    panel->refreshSummary();

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "HANSO Package";
    options.dialogBackgroundColour = juce::Colour::fromRGB(24, 25, 27);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.content.setOwned(panel);
    if (auto* window = options.launchAsync())
        window->centreWithSize(720, 520);
}

void CapturePanel::confirmStartNewCapture()
{
    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::WarningIcon)
        .withTitle("Start New Capture?")
        .withMessage(utf8("저장되지 않은 모든 캡쳐 데이터가 삭제됩니다.\n새 캡쳐를 시작할까요?"))
        .withButton("Start New Capture")
        .withButton("Cancel")
        .withAssociatedComponent(this);

    juce::AlertWindow::showAsync(options, [this](int result)
    {
        if (result == 1)
        {
            capture.reset();
            appState.resetForNewCapture();
            analysisSummaryLabel.setText("Finish Capture runs analysis and validation before export.", juce::dontSendNotification);
            hideBusyOverlay();
            selectStep(appState.captureWizard().currentStepId);
            updateModeFromButtons();
            appState.appendLog("Started a new capture workflow.");
        }
    });
}

void CapturePanel::updateCompletionActions()
{
    const auto ready = appState.captureWizard().isExportReady();
    exportButton.setVisible(true);
    exportButton.setEnabled(ready);

    previewButton.setVisible(true);
    previewButton.setEnabled(true);
    previewButton.setButtonText("Preview");
}

void CapturePanel::showTonePreviewDialog()
{
    auto* panel = new TonePreviewPanel(appState, capture);

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Tone Preview";
    options.dialogBackgroundColour = juce::Colour::fromRGB(22, 24, 26);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.content.setOwned(panel);
    if (auto* window = options.launchAsync())
        window->centreWithSize(780, 620);
}

void CapturePanel::showBusyOverlay(const juce::String& titleText, const juce::String& detailText, bool indeterminate)
{
    busyOverlayVisible = true;
    busyIndeterminate = indeterminate;
    busyKind = titleText;
    busyProgressValue = indeterminate ? -1.0 : 0.0;
    busyTitle.setText(titleText, juce::dontSendNotification);
    busyDetail.setText(detailText, juce::dontSendNotification);

    for (auto* component : { static_cast<juce::Component*>(&busyBackground),
                            static_cast<juce::Component*>(&busyTitle),
                            static_cast<juce::Component*>(&busyDetail),
                            static_cast<juce::Component*>(&busyProgress) })
    {
        component->setVisible(true);
        component->toFront(false);
    }
}

void CapturePanel::hideBusyOverlay()
{
    busyOverlayVisible = false;
    busyBackground.setVisible(false);
    busyTitle.setVisible(false);
    busyDetail.setVisible(false);
    busyProgress.setVisible(false);
}

void CapturePanel::updateBusyOverlay()
{
    if (! busyOverlayVisible)
        return;

    if (capture.state() == CaptureSessionState::Recording)
    {
        busyProgressValue = capture.captureProgress();
        busyDetail.setText("Capturing... " + juce::String(static_cast<int>(busyProgressValue * 100.0)) + "%", juce::dontSendNotification);
        return;
    }

    if (busyKind == "Capturing...")
        hideBusyOverlay();
}

void CapturePanel::updateStepActions()
{
    const auto& wizard = appState.captureWizard();
    const auto gainsUnlocked = setupAndCalibrationComplete(wizard);
    const auto activeGainStepId = gainsUnlocked ? firstIncompleteGainStepId(wizard) : juce::String();
    const auto activeRecordingStepId = capture.state() == CaptureSessionState::Recording
                                     ? capture.activeCaptureStepId()
                                     : juce::String();

    for (int i = 0; i < static_cast<int>(wizard.recipe.steps.size()) && i < stepButtons.size(); ++i)
    {
        const auto& step = wizard.recipe.steps[static_cast<size_t>(i)];
        const auto isGain = isGainCaptureStep(step);
        const auto completed = isComplete(step.status);
        const auto isActiveGain = gainsUnlocked && step.stepId == activeGainStepId;
        const auto isRecordingThisStep = activeRecordingStepId == step.stepId;
        const auto showCaptureActions = isGain && gainsUnlocked && (completed || isActiveGain || isRecordingThisStep);

        stepStartButtons[i]->setVisible(showCaptureActions);
        stepRecaptureButtons[i]->setVisible(showCaptureActions);
        stepStopButtons[i]->setVisible(showCaptureActions);
        const auto hasCaptureData = wizard.findResult(step.stepId) != nullptr;
        stepResetButtons[i]->setVisible(isGain && hasCaptureData);
        stepResetButtons[i]->setEnabled(isGain && hasCaptureData);

        const auto captureControlsEnabled = isActiveGain || isRecordingThisStep;
        stepStartButtons[i]->setEnabled(captureControlsEnabled && ! isRecordingThisStep);
        stepRecaptureButtons[i]->setEnabled(captureControlsEnabled
                                            && ! isRecordingThisStep
                                            && (step.status == CaptureStepStatus::Failed
                                                || step.status == CaptureStepStatus::Warning));
        stepStopButtons[i]->setEnabled(isRecordingThisStep);
    }
}
}

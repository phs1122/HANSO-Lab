#include "UI/CapturePanel.h"

#include "App/Utf8.h"
#include "Capture/CabinetMessages.h"
#include "Capture/CaptureStepUtils.h"
#include "UI/AssetPanel.h"
#include "UI/TonePreviewPanel.h"

namespace hanso
{
namespace
{
// Capture-type dropdown item ids (stable, not the display order).
int captureTypeBoxId(CaptureType type)
{
    switch (type)
    {
        case CaptureType::Cabinet: return 2;
        case CaptureType::Pedal:
        case CaptureType::Effect:  return 3;
        case CaptureType::FullRig: return 5;
        case CaptureType::PreAmp:  return 7;
        case CaptureType::Amp:     return 1;
    }
    return 1;
}

CaptureType captureTypeFromBoxId(int id)
{
    switch (id)
    {
        case 2: return CaptureType::Cabinet;
        case 3: return CaptureType::Pedal;
        case 5: return CaptureType::FullRig;
        case 7: return CaptureType::PreAmp;
        default: return CaptureType::Amp;
    }
}

juce::String modeDescription(CaptureMode mode)
{
    if (mode == CaptureMode::Easy)
        return utf8("간편 캡쳐 (직접 연결 호환 모드)\n언밸런스드 Line Out 직결, 또는 Phones Out → TRS→듀얼 TS 브레이크아웃(LEFT 플러그만) → 장비 Input\n앱은 출력 레벨 관리·라우팅·캘리브레이션을 보조하지만, 리앰프 박스의 임피던스 변환·밸런스 변환·절연은 대신하지 못합니다.\n인터페이스와 직접 연결 조건의 영향이 결과에 포함될 수 있어, 일반 캡쳐보다 장비 간 재현성과 입력 조건 표준화 수준이 낮습니다. 앱은 Left 채널만 출력합니다.");

    return utf8("일반 캡쳐\nLine Out → 리앰프 박스 → 장비 Input\n직접 연결 방식에 비해 입력 조건의 재현성과 장비 간 호환성을 높일 수 있는 권장 방식입니다. 낮은 출력에서 캘리브레이션을 시작해 사용 가능한 레벨을 확정합니다.");
}

juce::String workflowDescription(const CaptureWizardState& wizard)
{
    if (wizard.captureType == CaptureType::Cabinet)
    {
        return utf8("캐비넷 캡쳐\nCone / Cone Edge / Edge / Off-Axis 슬롯 중 원하는 위치를 직접 캡쳐하거나 IR로 가져올 수 있습니다.\n비워둔 위치는 Finish 단계에서 실제 source와 포지션 색채 커브를 기반으로 추정됩니다.");
    }

    if (wizard.captureType == CaptureType::Pedal || wizard.captureType == CaptureType::Effect)
    {
        return utf8("정적 페달 캡쳐\n앱 출력 → 리앰프 박스(권장) → 페달 Input → 페달 Output → 인터페이스 Input\nDistortion / Overdrive / Fuzz / Boost / 고정 EQ를 지원합니다. Modulation / Delay / Reverb는 현재 지원하지 않습니다.");
    }

    return modeDescription(wizard.mode);
}

juce::String cableGuideText(CableGuideType type)
{
    if (type == CableGuideType::TrsToDualTsYCable)
        return utf8("TRS→듀얼 TS 브레이크아웃 케이블 (공식 지원)\n- 헤드폰 출력 직결의 기본 요구 케이블입니다\n- 브레이크아웃 케이블의 LEFT(L) 플러그만 앰프 Input에 연결하세요\n- RIGHT(R) 플러그는 연결하지 마세요 (앱은 Left만 출력, Right는 무음)\n- 간편 캡쳐 결과에는 인터페이스 직접 연결 조건이 포함될 수 있습니다");

    return utf8("단일 TS 직결 — 실험적 빠른 시작 (공식 기본 연결 아님)\n- 스테레오 헤드폰 출력에 단일 TS 플러그를 꽂으면 사용하지 않는 Right 출력 채널이 지속적으로 접지될 수 있습니다\n- 앱은 Right를 무음 처리하지만, 앱 외부의 출력(OS·다른 앱·DSP 믹서·앱 종료 후 상태)까지 통제할 수는 없습니다\n- 대부분의 현대적인 헤드폰 출력에서 즉각적인 손상 가능성은 높지 않을 수 있으나, 불특정 인터페이스의 출력 회로와 보호 설계를 보증할 수 없어 공식 기본 연결로 제공하지 않습니다\n- 가능하면 TRS→듀얼 TS 브레이크아웃 케이블을 사용하세요");
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
    if (result->quality.signalToNoiseDb > -120.0f)
        text = text + juce::String("\nNoise floor ") + juce::String(result->quality.noiseFloorDbfs, 1)
             + juce::String(" dBFS / SNR ") + juce::String(result->quality.signalToNoiseDb, 1)
             + juce::String(" dB");

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
    // "Capture Wizard" 타이틀 제거 (앱 타이틀로 충분). Stage 2에서 이 자리를
    // 캡쳐 대상 + 진행 단계 맥락 헤더로 대체 예정.
    styleSectionTitle(title, "");
    addAndMakeVisible(title);

    captureTypeLabel.setText("Capture Type", juce::dontSendNotification);
    captureTypeLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(captureTypeLabel);

    captureTypeBox.addItem("Full Rig", 5);
    captureTypeBox.addItem("Amp Head", 1);
    captureTypeBox.addItem(utf8("Pedal / Effect"), 3);
    captureTypeBox.addItem("Pre Amp Only", 7);
    captureTypeBox.addItem("Cabinet", 2);
    captureTypeBox.setSelectedId(captureTypeBoxId(CaptureType::FullRig), juce::dontSendNotification);
    captureTypeBox.onChange = [this] { updateCaptureTypeFromSelector(); };
    addAndMakeVisible(captureTypeBox);

    standardModeButton.setButtonText(utf8("일반 캡쳐"));
    easyModeButton.setButtonText(utf8("간편 캡쳐"));
    standardModeButton.setClickingTogglesState(true);
    easyModeButton.setClickingTogglesState(true);
    standardModeButton.setRadioGroupId(1001);
    easyModeButton.setRadioGroupId(1001);
    standardModeButton.onClick = [this] { updateModeFromButtons(); };
    easyModeButton.onClick = [this] { updateModeFromButtons(); };
    addAndMakeVisible(standardModeButton);
    addAndMakeVisible(easyModeButton);

    cableGuideBox.addItem(utf8("TRS→듀얼 TS 브레이크아웃 · LEFT만 (기본)"), 2);
    cableGuideBox.addItem(utf8("실험적 빠른 시작 · 단일 TS 직결"), 1);
    cableGuideBox.setSelectedId(2);
    cableGuideBox.onChange = [this]
    {
        auto applyCableGuide = [this](CableGuideType type)
        {
            appState.captureWizard().cableGuide = type;
            appState.currentPackage().captureWorkflow = appState.captureWizard().toMetadataVar();
            syncWizardUi();
        };

        if (cableGuideBox.getSelectedId() != 1)
        {
            applyCableGuide(CableGuideType::TrsToDualTsYCable);
            return;
        }

        // Experimental quick start is consent-gated: the one-click dropdown pick
        // alone must not switch to the unsupported single-TS connection
        // (CAPTURE_CONNECTION_POLICY §2).
        juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::WarningIcon,
            utf8("실험적 연결"),
            utf8("이 방식은 스테레오 헤드폰 출력에 모노 TS 케이블을 직접 연결합니다. "
                 "일부 오디오 인터페이스에서는 사용하지 않는 출력 채널이 접지될 수 있습니다.\n\n"
                 "대부분의 장비에서 정상적으로 동작할 수 있지만, HANSO Lab은 모든 오디오 "
                 "인터페이스의 출력 회로와 보호 설계를 확인할 수 없습니다.\n\n"
                 "가능하면 TRS→듀얼 TS 브레이크아웃 케이블을 사용하세요."),
            utf8("이해했으며 실험적 연결로 계속"),
            utf8("권장 연결 방법 사용"),
            this,
            juce::ModalCallbackFunction::create([this, applyCableGuide](int confirmed)
            {
                if (confirmed != 0)
                {
                    applyCableGuide(CableGuideType::NormalTsCable);
                    return;
                }

                cableGuideBox.setSelectedId(2, juce::dontSendNotification);
                applyCableGuide(CableGuideType::TrsToDualTsYCable);
            }));
    };
    addAndMakeVisible(cableGuideBox);

    connectionGuideLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(connectionGuideLabel);

    safetyWarningLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    safetyWarningLabel.setJustificationType(juce::Justification::topLeft);
    safetyWarningLabel.setText(utf8("앰프의 Speaker Out을 오디오 인터페이스 Input에 직접 연결하지 마세요.\n반드시 스피커 캐비넷+마이크 또는 적절한 로드박스를 사용해야 합니다."), juce::dontSendNotification);
    addAndMakeVisible(safetyWarningLabel);

    // Cabinet workflow only: the mic used for direct slot captures. Imported
    // IR slots get their own metadata from the import dialog instead.
    captureMicLabel.setText(utf8("캡쳐 마이크"), juce::dontSendNotification);
    addAndMakeVisible(captureMicLabel);
    captureMicClassBox.addItem("Unknown", 1);
    captureMicClassBox.addItem("Dynamic", 2);
    captureMicClassBox.addItem("Ribbon", 3);
    captureMicClassBox.addItem("Condenser", 4);
    captureMicClassBox.setSelectedId(1, juce::dontSendNotification);
    captureMicClassBox.onChange = [this] { applyCaptureMicFromControls(); };
    addAndMakeVisible(captureMicClassBox);
    captureMicModelEditor.setTextToShowWhenEmpty(utf8("마이크 모델명 (선택)"), juce::Colours::grey);
    captureMicModelEditor.onFocusLost = [this] { applyCaptureMicFromControls(); };
    captureMicModelEditor.onReturnKey = [this] { applyCaptureMicFromControls(); };
    addAndMakeVisible(captureMicModelEditor);

    rebuildStepRows();

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
    calibrationOutputSliderLabel.setText(utf8("앱 출력 레벨 (낮게 시작해 단계적으로 · 리턴 -36 ~ -8 dBFS 창으로 확정)"), juce::dontSendNotification);
    calibrationOutputSliderLabel.setJustificationType(juce::Justification::centredLeft);
    calibrationOutputSlider.setRange(-50.0, -3.0, 0.5);
    calibrationOutputSlider.setValue(capture.calibrationOutputDb(), juce::dontSendNotification);
    calibrationOutputSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22);
    calibrationOutputSlider.setTextValueSuffix(" dBFS");
    calibrationOutputSlider.onValueChange = [this]
    {
        capture.setCalibrationOutputDb(static_cast<float>(calibrationOutputSlider.getValue()));
    };
    calibrationOutputMeter.setRange(-60.0f, 0.0f);
    // Output target mirrors the engine's calibration output window: below
    // -42 dBFS flags a dead/too-weak output, and the top sits at the slider
    // ceiling (-3 dBFS) because every slider-reachable level is legitimate —
    // the return window is the actual safety guard, not the output level.
    calibrationOutputMeter.setTargetRange(-42.0f, -3.0f);
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

void CapturePanel::rebuildStepRows()
{
    stepIcons.clear(true);
    stepButtons.clear(true);
    stepStartButtons.clear(true);
    stepStopButtons.clear(true);
    stepResetButtons.clear(true);
    stepImportButtons.clear(true);

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

        auto* start = new juce::TextButton("Start");
        start->onClick = [this, id = step.stepId]
        {
            selectStep(id);
            showBusyOverlay("Capturing...", "Sending test signal and recording the return.", false);
            capture.startCaptureStep(id);
            syncWizardUi();
        };
        addAndMakeVisible(start);
        stepStartButtons.add(start);

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

        auto* importIr = new juce::TextButton("Import IR");
        importIr->onClick = [this, id = step.stepId] { showImportIrChooser(id); };
        addAndMakeVisible(importIr);
        stepImportButtons.add(importIr);
    }

    resized();
    syncWizardUi();
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
    captureTypeLabel.setBounds(top.removeFromLeft(92).reduced(0, 5));
    captureTypeBox.setBounds(top.removeFromLeft(130).reduced(0, 5));
    top.removeFromLeft(12);
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

    // Card backgrounds (drawn in paint(), behind the column content).
    leftCardBounds = left.expanded(6, 8);
    rightCardBounds = centre.expanded(6, 8);

    connectionGuideLabel.setBounds(left.removeFromTop(120));
    left.removeFromTop(10);
    safetyWarningLabel.setBounds(left.removeFromTop(82));
    left.removeFromTop(12);

    if (appState.captureWizard().captureType == CaptureType::Cabinet)
    {
        auto micRow = left.removeFromTop(30);
        captureMicLabel.setBounds(micRow.removeFromLeft(92));
        captureMicClassBox.setBounds(micRow.removeFromLeft(130).reduced(0, 2));
        micRow.removeFromLeft(8);
        captureMicModelEditor.setBounds(micRow.removeFromLeft(200).reduced(0, 2));
        left.removeFromTop(8);
    }

    for (int i = 0; i < stepButtons.size(); ++i)
    {
        auto row = left.removeFromTop(34);
        auto iconArea = row.removeFromLeft(34);
        stepIcons[i]->setBounds(iconArea.withSizeKeepingCentre(28, 28));
        stepButtons[i]->setBounds(row.removeFromLeft(190).reduced(0, 2));
        row.removeFromLeft(8);
        stepStartButtons[i]->setBounds(row.removeFromLeft(76).reduced(0, 2));
        row.removeFromLeft(6);
        stepStopButtons[i]->setBounds(row.removeFromLeft(54).reduced(0, 2));
        row.removeFromLeft(6);
        stepResetButtons[i]->setBounds(row.removeFromLeft(54).reduced(0, 2));
        row.removeFromLeft(6);
        stepImportButtons[i]->setBounds(row.removeFromLeft(82).reduced(0, 2));
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
        calibrationInputMeterLabel.setBounds(centre.removeFromTop(showCalibrationControls ? 82 : 38));
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

void CapturePanel::paint(juce::Graphics& g)
{
    const auto card = juce::Colour::fromRGB(31, 33, 36);
    const auto border = juce::Colour::fromRGB(58, 62, 66);
    for (const auto& r : { leftCardBounds, rightCardBounds })
    {
        if (r.isEmpty())
            continue;
        g.setColour(card);
        g.fillRoundedRectangle(r.toFloat(), 12.0f);
        g.setColour(border.withAlpha(0.55f));
        g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 12.0f, 1.0f);
    }
}

void CapturePanel::paintOverChildren(juce::Graphics& g)
{
    if (! layoutDebugEnabled)
        return;

    drawDebugBounds(g, title, "title", juce::Colours::yellow);
    drawDebugBounds(g, captureTypeLabel, "captureTypeLabel", juce::Colours::orange);
    drawDebugBounds(g, captureTypeBox, "captureType", juce::Colours::orange);
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
        drawDebugBounds(g, *stepStopButtons[i], "stop" + juce::String(i + 1), juce::Colours::limegreen);
        drawDebugBounds(g, *stepResetButtons[i], "reset" + juce::String(i + 1), juce::Colours::limegreen);
        drawDebugBounds(g, *stepImportButtons[i], "import" + juce::String(i + 1), juce::Colours::limegreen);
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

void CapturePanel::updateCaptureTypeFromSelector()
{
    const auto selectedType = captureTypeFromBoxId(captureTypeBox.getSelectedId());
    if (selectedType == appState.captureWizard().captureType)
        return;

    if (! appState.hasUnsavedCaptureData())
    {
        applyCaptureType(selectedType);
        return;
    }

    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::WarningIcon)
        .withTitle("Change Capture Type?")
        .withMessage(utf8("캡쳐 타입을 변경하면 저장되지 않은 현재 캡쳐 데이터가 삭제됩니다.\n계속할까요?"))
        .withButton("Change Type")
        .withButton("Cancel")
        .withAssociatedComponent(this);

    juce::AlertWindow::showAsync(options, [this, selectedType](int result)
    {
        if (result == 1)
        {
            applyCaptureType(selectedType);
            return;
        }

        const auto curType = appState.captureWizard().captureType;
        captureTypeBox.setSelectedId(captureTypeBoxId(curType), juce::dontSendNotification);
    });
}

void CapturePanel::applyOnboardingSelection(const OnboardingResult& result)
{
    // Set the mode radios first: applyCaptureType() calls updateModeFromButtons()
    // which reads them. It also resets the wizard, so the mode lock is clear.
    easyModeButton.setToggleState(result.mode == CaptureMode::Easy, juce::dontSendNotification);
    standardModeButton.setToggleState(result.mode == CaptureMode::Standard, juce::dontSendNotification);
    captureTypeBox.setSelectedId(captureTypeBoxId(result.captureType), juce::dontSendNotification);
    applyCaptureType(result.captureType);

    // Refine the wizard defaults with the questionnaire's actual answers. The
    // experimental single-TS path was already consent-gated inside onboarding.
    auto& wizard = appState.captureWizard();
    wizard.excitationPath = result.excitationPath;
    wizard.returnPath = result.returnPath;
    wizard.cableGuide = result.cableGuide;
    cableGuideBox.setSelectedId(result.cableGuide == CableGuideType::TrsToDualTsYCable ? 2 : 1,
                                juce::dontSendNotification);
    appState.currentPackage().captureWorkflow = wizard.toMetadataVar();
    syncWizardUi();
}

void CapturePanel::applyCaptureType(CaptureType type)
{
    capture.stopOutputSignal();
    capture.reset();
    appState.resetForNewCapture();
    appState.captureWizard().setCaptureType(type);

    auto& package = appState.currentPackage();
    package.metadata.name = "Untitled HANSO Asset";
    package.metadata.category = categoryForCaptureType(type);
    package.metadata.deviceType = toString(type);
    package.captureWorkflow = appState.captureWizard().toMetadataVar();

    analysisSummaryLabel.setText(type == CaptureType::Cabinet
                                     ? "Finish / Build Cabinet prepares cabinet mic-position package metadata."
                                     : "Finish Capture runs analysis and validation before export.",
                                 juce::dontSendNotification);
    hideBusyOverlay();
    rebuildStepRows();
    updateModeFromButtons();
    appState.appendLog("Capture type changed to " + toString(type) + ".");
}

void CapturePanel::syncWizardUi()
{
    const auto& wizard = appState.captureWizard();
    captureTypeBox.setSelectedId(captureTypeBoxId(wizard.captureType), juce::dontSendNotification);
    // Cable guidance only applies to Easy direct connections; Standard runs
    // through a reamp box, so the headphone-cable text would be misleading there.
    connectionGuideLabel.setText(wizard.mode == CaptureMode::Easy
                                     ? workflowDescription(wizard) + "\n\n" + cableGuideText(wizard.cableGuide)
                                     : workflowDescription(wizard),
                                 juce::dontSendNotification);

    {
        const auto isCabinet = wizard.captureType == CaptureType::Cabinet;
        captureMicLabel.setVisible(isCabinet);
        captureMicClassBox.setVisible(isCabinet);
        captureMicModelEditor.setVisible(isCabinet);

        const auto boxId = wizard.cabinetCaptureMicClass == CabinetMicClass::Dynamic ? 2
                         : wizard.cabinetCaptureMicClass == CabinetMicClass::Ribbon ? 3
                         : wizard.cabinetCaptureMicClass == CabinetMicClass::Condenser ? 4
                         : 1;
        captureMicClassBox.setSelectedId(boxId, juce::dontSendNotification);
        if (! captureMicModelEditor.hasKeyboardFocus(true))
            captureMicModelEditor.setText(wizard.cabinetCaptureMicModelName, false);
    }

    // Contextual header (replaces the removed "Capture Wizard" title):
    // 모드 · 캡쳐 대상 · 진행 단계.
    {
        const auto typeLabel = wizard.captureType == CaptureType::Cabinet ? juce::String("Cabinet")
                             : wizard.captureType == CaptureType::FullRig ? juce::String("Full Rig")
                             : wizard.captureType == CaptureType::PreAmp ? juce::String("Pre Amp")
                             : wizard.captureType == CaptureType::Pedal || wizard.captureType == CaptureType::Effect ? juce::String("Pedal / Effect")
                             : juce::String("Amp Head");
        auto passed = 0;
        for (const auto& s : wizard.recipe.steps)
            if (s.status == CaptureStepStatus::Passed)
                ++passed;
        title.setText(toKoreanString(wizard.mode) + utf8("   ·   ") + typeLabel + utf8("   ·   ")
                          + juce::String(passed) + "/" + juce::String((int) wizard.recipe.steps.size())
                          + utf8(" 단계"),
                      juce::dontSendNotification);
    }

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
            juce::String titleText = rowStep.title;
            if (wizard.captureType == CaptureType::Cabinet)
            {
                if (const auto* slot = wizard.findCabinetSlot(rowStep.stepId))
                {
                    titleText += utf8(" · ") + toString(slot->source);
                    if (slot->sourceFileName.isNotEmpty())
                        stepButtons[i]->setTooltip("Imported: " + slot->sourceFileName);
                    else if (slot->estimatedFrom.isNotEmpty())
                        stepButtons[i]->setTooltip("Estimated from: " + slot->estimatedFrom);
                    else if (slot->errorMessage.isNotEmpty())
                        stepButtons[i]->setTooltip(slot->errorMessage);
                    else
                        stepButtons[i]->setTooltip({});
                }
            }
            stepButtons[i]->setButtonText(titleText);
        }
    }

    const auto* step = wizard.findStep(wizard.currentStepId);
    if (step != nullptr)
    {
        const auto pedal = wizard.captureType == CaptureType::Pedal || wizard.captureType == CaptureType::Effect;
        safetyWarningLabel.setText(pedal
            ? utf8("페달 Output은 인터페이스의 Instrument/Line Input으로 받으세요.\n페달 전원 규격과 극성을 확인하고, 앰프 Speaker Out은 절대 인터페이스에 연결하지 마세요.")
            : utf8("앰프의 Speaker Out을 오디오 인터페이스 Input에 직접 연결하지 마세요.\n반드시 스피커 캐비넷+마이크 또는 적절한 로드박스를 사용해야 합니다."),
            juce::dontSendNotification);
        instructionTitleLabel.setText(step->title, juce::dontSendNotification);
        instructionLabel.setText(step->instructionText, juce::dontSendNotification);
        qualityLabel.setText(qualityText(wizard.findResult(step->stepId)), juce::dontSendNotification);
        const auto showLevelMeters = step->stepId != "setup";
        const auto showCalibrationControls = step->stepId == "calibration";
        // Both modes drive the app output with the slider: a fixed dBFS level
        // cannot pin the analog voltage reaching the amp (it varies per interface,
        // reference level, volume knob, cabling), so the staged low-start
        // calibration with the return window is the actual level authority
        // (CAPTURE_CONNECTION_POLICY §3).
        const auto easyMode = wizard.mode == CaptureMode::Easy;
        const auto showOutputSlider = showCalibrationControls;
        const auto showOutputMeter = showLevelMeters;
        const auto visibilityChanged = calibrationInputMeter.isVisible() != showLevelMeters
                                    || calibrationOutputSlider.isVisible() != showOutputSlider
                                    || calibrationOutputMeter.isVisible() != showOutputMeter;
        calibrationMeterTitle.setVisible(showLevelMeters);
        calibrationOutputSliderLabel.setVisible(showCalibrationControls);
        calibrationOutputSliderLabel.setText(easyMode
            ? utf8("앱 출력 레벨 (낮게 시작해 단계적으로 · 리턴 클리핑 주의)")
            : utf8("앱 출력 레벨 (낮게 시작해 단계적으로 · 드라이브는 리앰프 박스로 · 초기 제안 -12 dBFS 이하)"),
            juce::dontSendNotification);
        calibrationOutputSlider.setVisible(showOutputSlider);
        calibrationOutputMeter.setVisible(showOutputMeter);
        calibrationOutputMeterLabel.setVisible(showOutputMeter);
        calibrationInputMeter.setVisible(showLevelMeters);
        calibrationInputMeterLabel.setVisible(showLevelMeters);

        if (visibilityChanged)
            resized();
    }

    const auto exportText = wizard.isExportReady()
                          ? (wizard.hasWarnings() ? utf8("Export ready with warnings: 경고가 있는 캡쳐가 있습니다.") : utf8("Export ready: 모든 필수 단계가 완료되었습니다."))
                          : wizard.exportDisabledReason();
    exportReadinessLabel.setText(exportText, juce::dontSendNotification);

    // Capture mode (Easy/Standard) describes the physical rig (reamp box or not),
    // which cannot change by a click mid-session. Keep the buttons in sync with
    // the wizard and lock them once calibration has started or passed; changing
    // the mode then requires "새 캡쳐" (full reset), which invalidates calibration.
    const bool modeLocked = capture.isCalibrationMonitorRunning() || wizard.calibrationPassed;
    easyModeButton.setToggleState(wizard.mode == CaptureMode::Easy, juce::dontSendNotification);
    standardModeButton.setToggleState(wizard.mode == CaptureMode::Standard, juce::dontSendNotification);
    easyModeButton.setEnabled(! modeLocked);
    standardModeButton.setEnabled(! modeLocked);

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
        if (wizard.captureType == CaptureType::Cabinet)
        {
            if (! setupAndCalibrationComplete(wizard))
            {
                appState.appendLog("Cabinet build requires setup and calibration first.");
            }
            else if (! wizard.hasCabinetRealSource())
            {
                appState.appendLog(cabinetExportRequiresRealSourceMessage());
            }
            else if (capture.buildCabinetFromSlots())
            {
                appState.currentPackage().captureWorkflow = wizard.toMetadataVar();
            }
        }
        else if (setupAndCalibrationComplete(wizard) && firstIncompleteGainStepId(wizard).isEmpty())
        {
            runFinishCaptureAnalysis();
        }
    }

    syncWizardUi();
}

void CapturePanel::updateModeFromButtons()
{
    auto& wizard = appState.captureWizard();
    const bool modeLocked = capture.isCalibrationMonitorRunning() || wizard.calibrationPassed;
    const auto requested = easyModeButton.getToggleState() ? CaptureMode::Easy : CaptureMode::Standard;

    // Defense in depth: the buttons are disabled while locked, but if a change
    // still reaches here, refuse it and re-sync so calibration stays valid for
    // the rig it was measured on.
    if (modeLocked && requested != wizard.mode)
    {
        appState.appendLog(utf8("캡쳐 모드는 calibration 시작 후 변경할 수 없습니다. 바꾸려면 '새 캡쳐'로 초기화하세요."));
        syncWizardUi();
        return;
    }

    wizard.mode = requested;
    // Keep the physical-path metadata consistent with the mode: Standard means
    // a reamp box drives the device; Easy means a direct connection (line by
    // default — onboarding refines to direct_headphone when applicable).
    if (requested == CaptureMode::Standard)
        wizard.excitationPath = ExcitationPath::Reamp;
    else if (wizard.excitationPath == ExcitationPath::Reamp)
        wizard.excitationPath = ExcitationPath::DirectLine;
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

    // Output has no high-side limit: driving hard is desirable (esp. for weak
    // sources) and harmless since the app controls its own digital output. The
    // only real risk lives on the return side, so only flag a too-weak output.
    juce::String outputState;
    if (outputDb < -42.0f)
        outputState = utf8("낮음: 앱 테스트 신호가 너무 작습니다.");
    else
        outputState = utf8("정상");

    calibrationOutputMeterLabel.setText(utf8("Output: ")
                                        + juce::String(outputDb, 1)
                                        + utf8(" dBFS / 실효 ")
                                        + juce::String(capture.effectiveOutputDb(), 1)
                                        + utf8(" dBFS\n리턴이 클리핑 안 하도록 입력게인 조절 / ")
                                        + outputState,
                                        juce::dontSendNotification);

    auto returnText = utf8("Return: ")
                    + juce::String(inputDb, 1)
                    + " dBFS\nTarget -36 to -8 / "
                    + inputState
                    + (capture.isCalibrationLevelSafe() && capture.isCalibrationOutputLevelSafe()
                         ? utf8(" / 3초")
                         : juce::String());

    if (capture.calibrationStatusText().isNotEmpty())
        returnText += "\n" + capture.calibrationStatusText();

    calibrationInputMeterLabel.setText(returnText, juce::dontSendNotification);
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
        {
            // Insert the fresh capture into its preview rig slot: pedals go in
            // front of the amp, everything else fills the amp slot.
            const auto type = appState.captureWizard().captureType;
            if (type == CaptureType::Pedal || type == CaptureType::Effect)
                capture.loadPreviewPedalModel(extraction.model);
            else
                capture.loadPreviewModel(extraction.model);
        }

        analysisSummaryLabel.setText("Analysis: " + LabWorkflow::formatSummary(summary)
                                     + (extraction.success ? " / Model ready" : " / Model extraction skipped"),
                                     juce::dontSendNotification);
        capture.completeNonAudioStep("final-validation");
        hideBusyOverlay();
        syncWizardUi();
        appState.appendLog("Finish Capture analysis, validation, and model extraction completed.");
    });
}

void CapturePanel::showImportIrChooser(const juce::String& stepId)
{
    auto& wizard = appState.captureWizard();
    const auto* step = wizard.findStep(stepId);
    if (step == nullptr || ! isCabinetPositionStep(*step))
        return;

    irImportChooser = std::make_unique<juce::FileChooser>("Import cabinet IR",
                                                          juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                                                          "*.wav");
    irImportChooser->launchAsync(juce::FileBrowserComponent::openMode
                                     | juce::FileBrowserComponent::canSelectFiles,
        [this, stepId](const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (file != juce::File())
                showImportIrMicMetadataDialog(stepId, file);

            irImportChooser = nullptr;
        });
}

void CapturePanel::showImportIrMicMetadataDialog(const juce::String& stepId, const juce::File& file)
{
    irImportMetadataDialog = std::make_unique<juce::AlertWindow>(
        "Import IR Metadata",
        utf8("이 IR을 녹음한 마이크 정보를 입력하세요.\n"
             "마이크 클래스는 나머지 마이크 종류/위치 tone 추정의 기준이 됩니다.\n"
             "클래스를 Unknown으로 두고 모델명만 입력하면 클래스를 자동 추정합니다."),
        juce::MessageBoxIconType::QuestionIcon, this);

    irImportMetadataDialog->addComboBox("micClass",
                                        { "Unknown", "Dynamic", "Ribbon", "Condenser" },
                                        utf8("마이크 클래스"));
    if (auto* combo = irImportMetadataDialog->getComboBoxComponent("micClass"))
        combo->setSelectedItemIndex(0, juce::dontSendNotification);
    irImportMetadataDialog->addTextEditor("micModel", {}, utf8("마이크 모델명 (선택)"));
    irImportMetadataDialog->addButton("Import", 1, juce::KeyPress(juce::KeyPress::returnKey));
    irImportMetadataDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    irImportMetadataDialog->enterModalState(true,
        juce::ModalCallbackFunction::create([this, stepId, file](int result)
        {
            if (irImportMetadataDialog == nullptr)
                return;

            if (result == 1)
            {
                const auto micModelName = irImportMetadataDialog->getTextEditorContents("micModel").trim();
                auto micClass = CabinetMicClass::Unknown;
                if (auto* combo = irImportMetadataDialog->getComboBoxComponent("micClass"))
                {
                    switch (combo->getSelectedItemIndex())
                    {
                        case 1: micClass = CabinetMicClass::Dynamic; break;
                        case 2: micClass = CabinetMicClass::Ribbon; break;
                        case 3: micClass = CabinetMicClass::Condenser; break;
                        default: break;
                    }
                }

                if (micClass == CabinetMicClass::Unknown)
                    micClass = suggestMicClassForModelName(micModelName);

                capture.importCabinetIrForStep(stepId, file, micClass, micModelName);
                syncWizardUi();
            }

            irImportMetadataDialog = nullptr;
        }),
        false);
}

void CapturePanel::applyCaptureMicFromControls()
{
    auto& wizard = appState.captureWizard();
    if (wizard.captureType != CaptureType::Cabinet)
        return;

    const auto micModelName = captureMicModelEditor.getText().trim();
    auto micClass = CabinetMicClass::Unknown;
    switch (captureMicClassBox.getSelectedId())
    {
        case 2: micClass = CabinetMicClass::Dynamic; break;
        case 3: micClass = CabinetMicClass::Ribbon; break;
        case 4: micClass = CabinetMicClass::Condenser; break;
        default: break;
    }

    if (micClass == CabinetMicClass::Unknown && micModelName.isNotEmpty())
    {
        micClass = suggestMicClassForModelName(micModelName);
        if (micClass != CabinetMicClass::Unknown)
        {
            const auto boxId = micClass == CabinetMicClass::Dynamic ? 2
                             : micClass == CabinetMicClass::Ribbon ? 3
                             : 4;
            captureMicClassBox.setSelectedId(boxId, juce::dontSendNotification);
        }
    }

    if (micClass == wizard.cabinetCaptureMicClass
        && micModelName == wizard.cabinetCaptureMicModelName)
        return;

    wizard.applyCabinetCaptureMic(micClass, micModelName);
    appState.currentPackage().captureWorkflow = wizard.toMetadataVar();
    syncWizardUi();
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
            const auto currentType = appState.captureWizard().captureType;
            capture.reset();
            appState.resetForNewCapture();
            appState.captureWizard().setCaptureType(currentType);
            appState.currentPackage().metadata.category = categoryForCaptureType(currentType);
            appState.currentPackage().metadata.deviceType = toString(currentType);
            appState.currentPackage().captureWorkflow = appState.captureWizard().toMetadataVar();
            analysisSummaryLabel.setText("Finish Capture runs analysis and validation before export.", juce::dontSendNotification);
            hideBusyOverlay();
            rebuildStepRows();
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
    const auto audioStepsUnlocked = setupAndCalibrationComplete(wizard);
    const auto activeGainStepId = audioStepsUnlocked ? firstIncompleteGainStepId(wizard) : juce::String();
    const auto activeRecordingStepId = capture.state() == CaptureSessionState::Recording
                                     ? capture.activeCaptureStepId()
                                     : juce::String();

    for (int i = 0; i < static_cast<int>(wizard.recipe.steps.size()) && i < stepButtons.size(); ++i)
    {
        const auto& step = wizard.recipe.steps[static_cast<size_t>(i)];
        const auto isGain = isGainCaptureStep(step);
        const auto isCabSlot = isCabinetPositionStep(step);
        const auto isActiveGain = audioStepsUnlocked && step.stepId == activeGainStepId;
        const auto isRecordingThisStep = activeRecordingStepId == step.stepId;
        const auto hasCaptureData = wizard.findResult(step.stepId) != nullptr;
        const auto hasCabinetData = isCabSlot
                                 && wizard.findCabinetSlot(step.stepId) != nullptr
                                 && wizard.findCabinetSlot(step.stepId)->hasAnyData();
        const auto isCaptureStep = isGain || isCabSlot;
        const auto calibrationLocked = isCaptureStep && ! audioStepsUnlocked;

        // Keep the action layout stable throughout the wizard. Before
        // calibration, capture actions remain visible but disabled instead of
        // disappearing, so the next required action is apparent at a glance.
        stepStartButtons[i]->setVisible(isCaptureStep);
        stepStopButtons[i]->setVisible(isCaptureStep);
        stepImportButtons[i]->setVisible(isCabSlot);
        stepResetButtons[i]->setVisible(isCaptureStep);
        stepResetButtons[i]->setEnabled((isGain && hasCaptureData) || (isCabSlot && hasCabinetData));

        const auto captureControlsEnabled = (isGain && (isActiveGain || isRecordingThisStep))
                                         || (isCabSlot && audioStepsUnlocked);
        stepStartButtons[i]->setEnabled(captureControlsEnabled && ! isRecordingThisStep);
        stepStopButtons[i]->setEnabled(isRecordingThisStep);
        stepImportButtons[i]->setEnabled(isCabSlot
                                         && audioStepsUnlocked
                                         && capture.state() != CaptureSessionState::Recording);

        const auto calibrationTooltip = utf8("Calibration을 진행해주세요");
        stepStartButtons[i]->setTooltip(calibrationLocked ? calibrationTooltip : juce::String());
        stepStopButtons[i]->setTooltip(calibrationLocked ? calibrationTooltip : juce::String());
        stepImportButtons[i]->setTooltip(calibrationLocked ? calibrationTooltip : juce::String());
        stepResetButtons[i]->setTooltip(calibrationLocked ? calibrationTooltip : juce::String());
    }
}
}

#include "UI/TonePreviewPanel.h"

#include "Analysis/ModelExtractionEngine.h"
#include "Audio/PreviewMicDistanceProcessor.h"
#include "App/Utf8.h"
#include "Capture/CabinetMicPositions.h"
#include "Capture/CaptureStepUtils.h"
#include "Preview/PreviewChainPolicy.h"
#include "Preview/PreviewSampleLibrary.h"
#include "Serialization/HansoAudioChunkCodec.h"
#include "Serialization/HansoModelCodec.h"
#include "Serialization/HansoSerializer.h"

#include <cmath>

namespace hanso
{
namespace
{
juce::String fileDurationText(const juce::File& file, juce::AudioFormatManager& formatManager)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr || reader->sampleRate <= 0.0)
        return {};

    const auto seconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
    const auto minutes = static_cast<int>(seconds / 60.0);
    const auto rest = static_cast<int>(std::round(seconds)) % 60;
    return juce::String(minutes) + ":" + juce::String(rest).paddedLeft('0', 2);
}
}

TonePreviewPanel::TonePreviewPanel(ApplicationState& state, CaptureEngine& captureEngine)
    : appState(state),
      capture(captureEngine)
{
    formatManager.registerBasicFormats();

    titleLabel.setText("Tone Preview", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    modelLabel.setJustificationType(juce::Justification::centredLeft);
    modelLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    addAndMakeVisible(modelLabel);

    folderLabel.setJustificationType(juce::Justification::centredLeft);
    folderLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(folderLabel);

    ampLabel.setText("HANSO AMP\nPreview Cabinet", juce::dontSendNotification);
    ampLabel.setJustificationType(juce::Justification::centred);
    ampLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    ampLabel.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(42, 34, 29));
    addAndMakeVisible(ampLabel);

    gainSlider.setRange(10.0, 100.0, 1.0);
    gainSlider.setValue(50.0, juce::dontSendNotification);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 74, 24);
    gainSlider.setTextValueSuffix("% Gain");
    gainSlider.onValueChange = [this] { updateGainModel(); };
    addAndMakeVisible(gainSlider);

    micDistanceSlider.setRange(PreviewMicDistanceProcessor::minimumDistanceCm,
                               PreviewMicDistanceProcessor::maximumDistanceCm,
                               0.5);
    micDistanceSlider.setValue(PreviewMicDistanceProcessor::referenceDistanceCm,
                               juce::dontSendNotification);
    micDistanceSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 74, 24);
    micDistanceSlider.setTextValueSuffix(" cm Distance");
    micDistanceSlider.setTooltip(utf8("선택한 포지션에서 캐비넷 면과 마이크 사이의 거리입니다. 거리가 늘면 레벨과 고역이 감쇠합니다."));
    micDistanceSlider.onValueChange = [this]
    {
        capture.setPreviewCabinetMicDistanceCm(static_cast<float>(micDistanceSlider.getValue()));
    };
    addAndMakeVisible(micDistanceSlider);

    for (int index = 0; index < cabinetMicPositionCount(); ++index)
        micPositionBox.addItem("Position: " + juce::String(kCabinetMicPositions[index].label), index + 1);
    micPositionBox.setSelectedId(2, juce::dontSendNotification);
    micPositionBox.setTooltip(utf8("Cone, Cone Edge, Edge, Off-Axis 중 목표 마이크 포지션을 선택합니다. 추정 슬롯도 tone profile EQ로 프리뷰됩니다."));
    micPositionBox.onChange = [this]
    {
        selectMicPositionPreset(micPositionBox.getSelectedId() - 1);
    };
    addAndMakeVisible(micPositionBox);

    chainStrip.onBlockClicked = [this](const juce::String& blockId) { handleChainBlockClicked(blockId); };
    chainStrip.onBlockReset = [this](const juce::String& blockId) { handleChainBlockReset(blockId); };

    sampleList.setColour(juce::ListBox::backgroundColourId, juce::Colour::fromRGB(26, 28, 30));
    sampleList.setRowHeight(30);
    addAndMakeVisible(sampleList);

    playButton.setButtonText(utf8("▶"));
    stopButton.setButtonText(utf8("■"));
    loopButton.setClickingTogglesState(true);
    normalizationButton.setClickingTogglesState(true);
    cabinetButton.setClickingTogglesState(true);
    loopButton.setToggleState(capture.isPreviewLoopEnabled(), juce::dontSendNotification);
    normalizationButton.setToggleState(capture.isPreviewNormalizationEnabled(), juce::dontSendNotification);
    cabinetButton.setToggleState(capture.isPreviewCabinetEnabled(), juce::dontSendNotification);
    playButton.setTooltip("Play selected preview sample");
    stopButton.setTooltip("Stop preview playback");
    loopButton.setTooltip("Loop selected preview sample");
    normalizationButton.setTooltip(utf8("클린/앰프 톤 간 체감 볼륨 차이를 맞춥니다."));
    cabinetButton.setTooltip(utf8("앰프 모델 출력에 캐비넷 DSP를 적용합니다. (앰프 모델 로드 시에만)"));
    playButton.onClick = [this] { playSelectedSample(); };
    stopButton.onClick = [this] { stopPlayback(); };
    loopButton.onClick = [this] { capture.setPreviewLoopEnabled(loopButton.getToggleState()); };
    normalizationButton.onClick = [this] { capture.setPreviewNormalizationEnabled(normalizationButton.getToggleState()); };
    cabinetButton.onClick = [this] { capture.setPreviewCabinetEnabled(cabinetButton.getToggleState()); };
    realCaptureButton.setClickingTogglesState(true);
    realCaptureButton.setTooltip(utf8("모델 렌더 대신, 캡쳐 시 실제 앰프를 거쳐 녹음된 샘플을 재생합니다 (A/B 비교)."));
    realCaptureButton.onClick = [this] { toggleRealCaptureMode(); };

    previewMicBox.addItem("Mic: Original", 1);
    previewMicBox.addItem("Mic: Dynamic", 2);
    previewMicBox.addItem("Mic: Ribbon", 3);
    previewMicBox.addItem("Mic: Condenser", 4);
    previewMicBox.setSelectedId(1, juce::dontSendNotification);
    previewMicBox.setTooltip(utf8("micMatrix 기반 마이크 전환 EQ 프리뷰입니다. Original은 패키지의 실측 마이크 그대로 재생합니다."));
    previewMicBox.onChange = [this]
    {
        auto micClass = CabinetMicClass::Unknown;
        switch (previewMicBox.getSelectedId())
        {
            case 2: micClass = CabinetMicClass::Dynamic; break;
            case 3: micClass = CabinetMicClass::Ribbon; break;
            case 4: micClass = CabinetMicClass::Condenser; break;
            default: break;
        }
        capture.setPreviewCabinetMicClass(micClass);
    };
    addAndMakeVisible(previewMicBox);

    cabSourceBox.addItem("Cab: Standard EQ", 1);
    cabSourceBox.addItem(utf8("Cab: Custom .hanso…"), 2);
    cabSourceBox.setSelectedId(1, juce::dontSendNotification);
    cabSourceBox.setTooltip(utf8("보완 캐비넷의 소스입니다. Standard EQ는 내장 캡 시뮬, Custom은 캐비넷 .hanso의 IR 컨볼루션을 사용합니다."));
    cabSourceBox.onChange = [this] { handleCabSourceSelection(); };
    addAndMakeVisible(cabSourceBox);
    volumeSlider.setRange(-24.0, 12.0, 0.1);
    volumeSlider.setValue(capture.previewOutputGainDb(), juce::dontSendNotification);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 22);
    volumeSlider.setTextValueSuffix(" dB");
    volumeSlider.setTooltip(utf8("스피커로 나가는 최종 출력 볼륨입니다."));
    volumeSlider.onValueChange = [this]
    {
        capture.setPreviewOutputGainDb(static_cast<float>(volumeSlider.getValue()));
    };
    addButton.onClick = [this] { importSamples(); };
    openHansoButton.onClick = [this] { openHansoFile(); };
    openFolderButton.onClick = []
    {
        juce::String error;
        PreviewSampleLibrary::ensureSamplesDirectory(error);
        PreviewSampleLibrary::samplesDirectory().revealToUser();
    };
    refreshButton.onClick = [this] { refreshSamples(); };
    addAndMakeVisible(playButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(loopButton);
    addAndMakeVisible(normalizationButton);
    addAndMakeVisible(cabinetButton);
    addAndMakeVisible(realCaptureButton);
    addAndMakeVisible(volumeSlider);
    addAndMakeVisible(addButton);
    addAndMakeVisible(openHansoButton);
    addAndMakeVisible(openFolderButton);
    addAndMakeVisible(refreshButton);

    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(chainStrip);
    ampLabel.setVisible(false);
    addAndMakeVisible(waveform);
    waveform.onSeek = [this](double progress)
    {
        capture.seekPreviewSample(progress);
    };

    applyPreviewControlLabels();
    loadInitialPreviewModel();
    refreshSamples();
    updateButtonState();
    startTimerHz(12);
    setSize(780, 620);
}

TonePreviewPanel::~TonePreviewPanel()
{
    capture.stopPreviewSample();
}

void TonePreviewPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(22, 24, 26));
}

void TonePreviewPanel::resized()
{
    auto area = getLocalBounds().reduced(18);
    titleLabel.setBounds(area.removeFromTop(32));
    modelLabel.setBounds(area.removeFromTop(24));
    folderLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(12);

    chainStrip.setBounds(area.removeFromTop(78).reduced(0, 6));
    area.removeFromTop(6);

    auto gainRow = area.removeFromTop(34);
    gainRow.removeFromLeft(90);
    gainSlider.setBounds(gainRow.removeFromLeft(420).reduced(0, 3));
    gainRow.removeFromLeft(12);
    cabSourceBox.setBounds(gainRow.removeFromLeft(160).reduced(0, 4));

    auto micRow = area.removeFromTop(34);
    micRow.removeFromLeft(90);
    micPositionBox.setBounds(micRow.removeFromLeft(170).reduced(0, 4));
    micRow.removeFromLeft(12);
    micDistanceSlider.setBounds(micRow.removeFromLeft(238).reduced(0, 3));
    micRow.removeFromLeft(12);
    previewMicBox.setBounds(micRow.removeFromLeft(160).reduced(0, 4));

    area.removeFromTop(12);
    auto buttonRow = area.removeFromTop(36);
    addButton.setBounds(buttonRow.removeFromLeft(128).reduced(0, 3));
    buttonRow.removeFromLeft(8);
    openHansoButton.setBounds(buttonRow.removeFromLeft(128).reduced(0, 3));
    buttonRow.removeFromLeft(8);
    openFolderButton.setBounds(buttonRow.removeFromLeft(128).reduced(0, 3));
    buttonRow.removeFromLeft(8);
    refreshButton.setBounds(buttonRow.removeFromLeft(96).reduced(0, 3));

    auto bottom = area.removeFromBottom(126);
    waveform.setBounds(bottom.removeFromTop(76));
    bottom.removeFromTop(10);
    auto transport = bottom.removeFromTop(36);
    playButton.setBounds(transport.removeFromLeft(42).reduced(0, 3));
    transport.removeFromLeft(6);
    stopButton.setBounds(transport.removeFromLeft(42).reduced(0, 3));
    transport.removeFromLeft(6);
    loopButton.setBounds(transport.removeFromLeft(58).reduced(0, 3));
    transport.removeFromLeft(8);
    normalizationButton.setBounds(transport.removeFromLeft(64).reduced(0, 3));
    transport.removeFromLeft(6);
    cabinetButton.setBounds(transport.removeFromLeft(58).reduced(0, 3));
    transport.removeFromLeft(6);
    realCaptureButton.setBounds(transport.removeFromLeft(58).reduced(0, 3));
    transport.removeFromLeft(10);
    volumeSlider.setBounds(transport.removeFromLeft(190).reduced(0, 4));
    transport.removeFromLeft(12);
    statusLabel.setBounds(transport);

    area.removeFromTop(8);
    sampleList.setBounds(area);
}

int TonePreviewPanel::getNumRows()
{
    return samples.size();
}

void TonePreviewPanel::paintListBoxItem(int rowNumber,
                                        juce::Graphics& g,
                                        int width,
                                        int height,
                                        bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= samples.size())
        return;

    g.fillAll(rowIsSelected ? juce::Colour::fromRGB(45, 63, 74)
                            : juce::Colour::fromRGB(26, 28, 30));

    const auto& file = samples[rowNumber];
    g.setColour(juce::Colours::white);
    g.setFont(17.5f);
    g.drawText(file.getFileNameWithoutExtension(), 10, 0, width - 96, height, juce::Justification::centredLeft);

    g.setColour(juce::Colours::grey);
    g.drawText(fileDurationText(file, formatManager), width - 84, 0, 74, height, juce::Justification::centredRight);
}

void TonePreviewPanel::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (row >= 0)
        playSelectedSample();
}

void TonePreviewPanel::selectedRowsChanged(int)
{
    updateButtonState();
}

void TonePreviewPanel::timerCallback()
{
    syncModelFromCaptureEngine();
    waveform.setProgress(capture.previewSampleProgress());
    updateButtonState();
}

void TonePreviewPanel::applyPreviewControlLabels()
{
    // Cabinet-slot controls appear when a cabinet package occupies the slot;
    // the standard-cab source combo shows while the slot is at its default.
    const auto cabLoaded = capture.hasPreviewCabinetPackage();
    micPositionBox.setVisible(cabLoaded);
    micDistanceSlider.setVisible(cabLoaded);
    previewMicBox.setVisible(cabLoaded);
    cabSourceBox.setVisible(! cabLoaded);
}

void TonePreviewPanel::handleCabSourceSelection()
{
    if (cabSourceBox.getSelectedId() == 1)
    {
        capture.setPreviewComplementCabUseCustom(false);
        return;
    }

    // Custom: reuse an already-loaded cab or ask for one.
    if (capture.hasPreviewComplementCabPackage())
    {
        capture.setPreviewComplementCabUseCustom(true);
        return;
    }

    complementCabChooser = std::make_unique<juce::FileChooser>("Choose complement cabinet HANSO",
                                                               juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                               "*.hanso");
    complementCabChooser->launchAsync(juce::FileBrowserComponent::openMode
                                          | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            const auto loaded = file != juce::File() && capture.loadPreviewComplementCabFile(file);
            if (! loaded)
                cabSourceBox.setSelectedId(1, juce::dontSendNotification);

            capture.setPreviewComplementCabUseCustom(loaded);
            complementCabChooser = nullptr;
            updateButtonState();
        });
}

void TonePreviewPanel::refreshChainStrip()
{
    using Block = PreviewChainStrip::Block;
    using Kind = PreviewChainStrip::BlockKind;

    // The full rig skeleton is always shown: Pedal -> Amp Head -> Cabinet.
    // Slots at their defaults are clean standard equipment (dashed); captured
    // or imported packages fill them solid and can be reset individually.
    std::vector<Block> blocks;
    blocks.push_back({ "sample", "Sample", utf8("클린 DI"), Kind::Io, false, false });

    if (pedalSlotSource != RigSlotSource::Default)
        blocks.push_back({ "pedal", "Pedal",
                           (pedalSlotSource == RigSlotSource::Session ? utf8("세션 캡쳐") : utf8("불러옴")),
                           Kind::Package, true, false });
    else
        blocks.push_back({ "pedal", "Pedal", utf8("비어 있음"), Kind::Complement, false, false });

    const auto ampCaptured = modelReady && ampSlotSource != RigSlotSource::Default;
    const auto isFullRig = ampCaptured && loadedDeviceLabel == "FullRig";
    const auto isPreampOnly = ampCaptured && loadedDeviceLabel == "PreAmp";
    const auto ampSourceText = ampSlotSource == RigSlotSource::Session ? utf8("세션 캡쳐") : utf8("불러옴");

    if (isFullRig)
    {
        blocks.push_back({ "amp", "Full Rig", ampSourceText + utf8(" · amp+cab"), Kind::Package, true, false });
    }
    else if (isPreampOnly)
    {
        // A captured preamp keeps the amp-head slot split open: captured
        // preamp plus the clean power-amp placeholder (.hanso power amps are
        // in production elsewhere).
        blocks.push_back({ "preamp", "PreAmp", ampSourceText, Kind::Package, true, false });
        blocks.push_back({ "power", "Power", utf8("Clean · placeholder"), Kind::Complement, false, false });
    }
    else if (ampExpanded && ! ampCaptured)
    {
        blocks.push_back({ "amp", "PreAmp", utf8("Clean · 무착색"), Kind::Complement, false, true });
        blocks.push_back({ "power", "Power", utf8("Clean · placeholder"), Kind::Complement, false, false });
    }
    else if (ampCaptured)
    {
        blocks.push_back({ "amp", "Amp Head", ampSourceText, Kind::Package, true, false });
    }
    else
    {
        blocks.push_back({ "amp", "Amp Head", utf8("Clean standard"), Kind::Complement, false, true });
    }

    if (! isFullRig)
    {
        if (cabSlotSource != RigSlotSource::Default)
        {
            const auto detail = capture.previewCabinetHasMicMatrix() ? utf8(" · IR+micMatrix") : utf8(" · IR");
            blocks.push_back({ "cab", "Cab",
                               (cabSlotSource == RigSlotSource::Session ? utf8("세션 캡쳐") : utf8("불러옴")) + detail,
                               Kind::Package, true, false });
        }
        else if (capture.isPreviewCabinetEnabled())
        {
            const auto subtitle = capture.previewComplementCabUseCustom()
                                ? capture.previewComplementCabSummary()
                                : juce::String("Standard EQ");
            blocks.push_back({ "cab", "Cab", subtitle, Kind::Complement,
                               capture.previewComplementCabUseCustom(), false });
        }
        else
        {
            blocks.push_back({ "cab", "Cab", utf8("FRFR · 무착색"), Kind::Complement, false, false });
        }
    }

    blocks.push_back({ "out", "Out", {}, Kind::Io, false, false });
    chainStrip.setBlocks(std::move(blocks));
}

void TonePreviewPanel::handleChainBlockClicked(const juce::String& blockId)
{
    if (blockId == "amp" && ! (modelReady && ampSlotSource != RigSlotSource::Default))
    {
        ampExpanded = ! ampExpanded;
        refreshChainStrip();
    }
}

void TonePreviewPanel::handleChainBlockReset(const juce::String& blockId)
{
    if (blockId == "pedal")
    {
        capture.clearPreviewPedalModel();
        pedalSlotSource = RigSlotSource::Default;
        pedalSlotLabel.clear();
        statusLabel.setText(utf8("Pedal 슬롯을 비웠습니다."), juce::dontSendNotification);
    }
    else if (blockId == "amp" || blockId == "preamp")
    {
        const auto wasFullRig = loadedDeviceLabel == "FullRig";
        capture.clearPreviewModel();
        ampSlotSource = RigSlotSource::Default;
        loadedDeviceLabel.clear();
        modelReady = false;
        model = {};
        observedPreviewRevision = capture.previewModelRevision();
        if (wasFullRig || ! capture.hasPreviewCabinetPackage())
            capture.setPreviewCabinetEnabled(true);
        statusLabel.setText(utf8("Amp 슬롯을 클린 표준 앰프로 되돌렸습니다."), juce::dontSendNotification);
    }
    else if (blockId == "cab")
    {
        if (capture.hasPreviewCabinetPackage())
        {
            capture.clearPreviewCabinetPackage();
            observedPreviewCabinetRevision = capture.previewCabinetRevision();
        }
        capture.setPreviewComplementCabUseCustom(false);
        capture.setPreviewCabinetEnabled(true);
        cabSlotSource = RigSlotSource::Default;
        cabSlotLabel.clear();
        applyPreviewControlLabels();
        statusLabel.setText(utf8("Cabinet 슬롯을 표준 캐비넷으로 되돌렸습니다."), juce::dontSendNotification);
    }

    updateButtonState();
}

bool TonePreviewPanel::isCabinetPackage(const HansoPackage& package) const noexcept
{
    if (package.metadata.category == HansoCategory::Cabinet)
        return true;

    if (! package.cabinetProfile.isVoid())
        return true;

    return false;
}

void TonePreviewPanel::loadInitialPreviewModel()
{
    if (capture.hasPreviewCabinetPackage())
    {
        cabSlotSource = RigSlotSource::Session;
        cabSlotLabel = capture.previewCabinetSummary();
    }
    observedPreviewCabinetRevision = capture.previewCabinetRevision();

    if (capture.hasPreviewPedalModel())
    {
        pedalSlotSource = RigSlotSource::Session;
        pedalSlotLabel = capture.previewPedalSummary();
    }

    if (const auto* currentModel = capture.currentPreviewModel())
    {
        model = *currentModel;
        modelReady = true;
        ampSlotSource = RigSlotSource::Session;
        loadedDeviceLabel = toString(appState.captureWizard().captureType);
        observedPreviewRevision = capture.previewModelRevision();
        applyPreviewControlLabels();
        modelLabel.setText(model.summary(), juce::dontSendNotification);
        return;
    }

    loadModelFromPackage();
}

void TonePreviewPanel::loadModelFromPackage()
{
    const auto* chunk = appState.currentPackage().findChunk(HansoModelCodec::compactModelChunkId);
    if (chunk == nullptr)
    {
        modelReady = false;
        ampSlotSource = RigSlotSource::Default;
        observedPreviewRevision = capture.previewModelRevision();
        applyPreviewControlLabels();
        modelLabel.setText(utf8("클린 프리뷰: 표준 체인으로 원본 샘플을 재생합니다."), juce::dontSendNotification);
        return;
    }

    juce::String error;
    modelReady = HansoModelCodec::decodeCompactModel(chunk->data, model, error);
    if (! modelReady)
    {
        ampSlotSource = RigSlotSource::Default;
        modelLabel.setText("Model decode failed: " + error, juce::dontSendNotification);
        return;
    }

    capture.loadPreviewModel(model);
    capture.setPreviewGainPercent(static_cast<float>(gainSlider.getValue()));
    if (! capture.hasPreviewCabinetPackage())
        capture.setPreviewCabinetEnabled(
            previewComplementCabDefaultForCaptureType(appState.captureWizard().captureType));
    loadedDeviceLabel = toString(appState.captureWizard().captureType);
    ampSlotSource = RigSlotSource::Session;
    observedPreviewRevision = capture.previewModelRevision();
    applyPreviewControlLabels();
    modelLabel.setText(utf8("Unsaved current tone: ") + model.summary(), juce::dontSendNotification);
}

bool TonePreviewPanel::loadModelFromHansoFile(const juce::File& file)
{
    auto package = std::make_shared<HansoPackage>();
    juce::String error;
    if (! HansoSerializer::readFromFile(file, *package, error))
    {
        statusLabel.setText("Open HANSO failed: " + error, juce::dontSendNotification);
        modelLabel.setText("Open HANSO failed: " + error, juce::dontSendNotification);
        return false;
    }

    // Classify the package, name its target slot, and confirm before loading —
    // especially when the load would replace an unsaved session capture.
    const auto cabinet = isCabinetPackage(*package);
    const auto deviceType = package->metadata.deviceType;
    const auto pedal = ! cabinet
                       && (deviceType == "Pedal" || deviceType == "Effect"
                           || package->metadata.category == HansoCategory::Pedal);
    const auto fullRig = ! cabinet && ! pedal
                         && (deviceType == "FullRig" || package->metadata.category == HansoCategory::Rig);

    juce::String typeLabel, targetLabel;
    bool displacesSession = false;
    if (cabinet)
    {
        typeLabel = "Cabinet";
        targetLabel = "Cabinet";
        displacesSession = cabSlotSource == RigSlotSource::Session;
    }
    else if (pedal)
    {
        typeLabel = "Pedal / Effect";
        targetLabel = "Pedal";
        displacesSession = pedalSlotSource == RigSlotSource::Session;
    }
    else if (fullRig)
    {
        typeLabel = "Full Rig (amp+cab)";
        targetLabel = "Amp Head + Cabinet";
        displacesSession = ampSlotSource == RigSlotSource::Session
                           || cabSlotSource == RigSlotSource::Session;
    }
    else
    {
        typeLabel = deviceType.isNotEmpty() ? deviceType : toString(package->metadata.category);
        targetLabel = "Amp Head";
        displacesSession = ampSlotSource == RigSlotSource::Session;
    }

    auto message = utf8("파일 타입: ") + typeLabel + "\n"
                 + utf8("대상 슬롯: ") + targetLabel;
    if (displacesSession && appState.hasUnsavedCaptureData())
        message += utf8("\n\n경고: 저장(Export)되지 않은 캡쳐가 프리뷰 체인에서 대체되며 되돌릴 수 없습니다. "
                        "유지하려면 먼저 Export 하세요.");

    auto options = juce::MessageBoxOptions()
        .withIconType(displacesSession ? juce::MessageBoxIconType::WarningIcon
                                       : juce::MessageBoxIconType::QuestionIcon)
        .withTitle("Open HANSO: " + file.getFileName())
        .withMessage(message)
        .withButton("Load")
        .withButton("Cancel")
        .withAssociatedComponent(this);

    juce::AlertWindow::showAsync(options, [this, package, file](int result)
    {
        if (result == 1)
            routeHansoPackage(package, file);
    });

    return true;
}

void TonePreviewPanel::routeHansoPackage(std::shared_ptr<HansoPackage> package, const juce::File& file)
{
    const auto toneName = package->metadata.name.trim().isNotEmpty()
                        ? package->metadata.name.trim()
                        : file.getFileNameWithoutExtension();

    if (isCabinetPackage(*package))
    {
        if (! capture.loadPreviewCabinetPackage(*package))
        {
            statusLabel.setText(utf8("캐비넷 .hanso를 프리뷰할 수 없습니다. captured/imported IR이 필요합니다."),
                                juce::dontSendNotification);
            return;
        }

        cabSlotSource = RigSlotSource::Imported;
        cabSlotLabel = toneName;
        observedPreviewCabinetRevision = capture.previewCabinetRevision();
        selectMicPositionPreset(micPositionBox.getSelectedId() - 1);
        capture.setPreviewCabinetMicDistanceCm(static_cast<float>(micDistanceSlider.getValue()));
        previewMicBox.setSelectedId(1, juce::dontSendNotification);
        capture.setPreviewCabinetMicClass(CabinetMicClass::Unknown);
        applyPreviewControlLabels();
        modelLabel.setText(utf8("Cabinet 슬롯: ") + toneName, juce::dontSendNotification);
        statusLabel.setText("Opened cabinet HANSO: " + file.getFileName(), juce::dontSendNotification);
        updateButtonState();
        return;
    }

    CompactHansoModel loadedModel;
    juce::String statusText;
    if (! loadCompactModelChunk(*package, file, loadedModel, statusText))
    {
        statusLabel.setText(statusText, juce::dontSendNotification);
        modelLabel.setText(statusText, juce::dontSendNotification);
        return;
    }

    const auto deviceType = package->metadata.deviceType;
    const auto pedal = deviceType == "Pedal" || deviceType == "Effect"
                       || package->metadata.category == HansoCategory::Pedal;

    if (pedal)
    {
        if (capture.loadPreviewPedalModel(loadedModel))
        {
            pedalSlotSource = RigSlotSource::Imported;
            pedalSlotLabel = toneName;
            observedPreviewRevision = capture.previewModelRevision();
            modelLabel.setText(utf8("Pedal 슬롯: ") + toneName, juce::dontSendNotification);
            statusLabel.setText(statusText, juce::dontSendNotification);
        }
        updateButtonState();
        return;
    }

    model = std::move(loadedModel);
    modelReady = capture.loadPreviewModel(model);
    if (modelReady)
    {
        capture.setPreviewGainPercent(static_cast<float>(gainSlider.getValue()));
        ampSlotSource = RigSlotSource::Imported;
        loadedDeviceLabel = deviceType.isNotEmpty() ? deviceType : toString(package->metadata.category);

        if (loadedDeviceLabel == "FullRig" || package->metadata.category == HansoCategory::Rig)
        {
            // A full rig bundles amp+cab: it displaces the cabinet slot.
            loadedDeviceLabel = "FullRig";
            capture.clearPreviewCabinetPackage();
            capture.setPreviewComplementCabUseCustom(false);
            cabSlotSource = RigSlotSource::Default;
            cabSlotLabel.clear();
            capture.setPreviewCabinetEnabled(false);
        }
        else if (! capture.hasPreviewCabinetPackage())
        {
            capture.setPreviewCabinetEnabled(previewComplementCabDefaultForPackage(package->metadata.deviceType,
                                                                                   package->metadata.category));
        }
    }
    observedPreviewRevision = capture.previewModelRevision();
    observedPreviewCabinetRevision = capture.previewCabinetRevision();
    applyPreviewControlLabels();
    modelLabel.setText("Preview Tone: " + toneName, juce::dontSendNotification);
    statusLabel.setText(statusText, juce::dontSendNotification);
    updateButtonState();
}

bool TonePreviewPanel::loadCompactModelChunk(HansoPackage& package,
                                             const juce::File& sourceFile,
                                             CompactHansoModel& destination,
                                             juce::String& statusText)
{
    const auto* chunk = package.findChunk(HansoModelCodec::compactModelChunkId);

    if (chunk == nullptr)
    {
        ModelExtractionEngine extractor;
        auto extraction = extractor.extractIntoPackage(package);
        if (! extraction.success)
        {
            statusText = utf8("이 .hanso 파일에는 프리뷰 가능한 모델이 없습니다: ") + extraction.message;
            return false;
        }

        destination = std::move(extraction.model);
        statusText = "Opened HANSO preview tone: " + sourceFile.getFileName()
                   + " / compact model generated from capture data.";
        return true;
    }

    juce::String error;
    if (! HansoModelCodec::decodeCompactModel(chunk->data, destination, error))
    {
        statusText = "Model decode failed: " + error;
        return false;
    }

    statusText = "Opened HANSO preview tone: " + sourceFile.getFileName();
    return true;
}

void TonePreviewPanel::openHansoFile()
{
    hansoChooser = std::make_unique<juce::FileChooser>("Open HANSO preview tone",
                                                       juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                       "*.hanso");

    hansoChooser->launchAsync(juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (file != juce::File())
                loadModelFromHansoFile(file);

            hansoChooser = nullptr;
            updateButtonState();
        });
}

void TonePreviewPanel::syncModelFromCaptureEngine()
{
    // Session captures land in the rig via engine revision bumps; imports set
    // their tags and observed revisions directly, so anything new seen here
    // came from the capture workflow.
    const auto cabinetRevision = capture.previewCabinetRevision();
    if (cabinetRevision != observedPreviewCabinetRevision)
    {
        observedPreviewCabinetRevision = cabinetRevision;
        if (capture.hasPreviewCabinetPackage())
        {
            cabSlotSource = RigSlotSource::Session;
            cabSlotLabel = capture.previewCabinetSummary();
            selectMicPositionPreset(micPositionBox.getSelectedId() - 1);
            capture.setPreviewCabinetMicDistanceCm(static_cast<float>(micDistanceSlider.getValue()));
            previewMicBox.setSelectedId(1, juce::dontSendNotification);
            capture.setPreviewCabinetMicClass(CabinetMicClass::Unknown);
            modelLabel.setText(utf8("Cabinet 슬롯: 세션 캡쳐가 삽입되었습니다."), juce::dontSendNotification);
        }
        else
        {
            cabSlotSource = RigSlotSource::Default;
            cabSlotLabel.clear();
        }
        applyPreviewControlLabels();
    }

    const auto revision = capture.previewModelRevision();
    if (revision == observedPreviewRevision)
        return;

    observedPreviewRevision = revision;

    if (capture.hasPreviewPedalModel())
    {
        if (pedalSlotSource == RigSlotSource::Default)
        {
            pedalSlotSource = RigSlotSource::Session;
            modelLabel.setText(utf8("Pedal 슬롯: 세션 캡쳐가 삽입되었습니다."), juce::dontSendNotification);
        }
        pedalSlotLabel = capture.previewPedalSummary();
    }
    else
    {
        pedalSlotSource = RigSlotSource::Default;
        pedalSlotLabel.clear();
    }

    if (const auto* currentModel = capture.currentPreviewModel())
    {
        model = *currentModel;
        modelReady = true;
        ampSlotSource = RigSlotSource::Session;
        loadedDeviceLabel = toString(appState.captureWizard().captureType);
        capture.setPreviewGainPercent(static_cast<float>(gainSlider.getValue()));

        if (loadedDeviceLabel == "FullRig")
        {
            // A full rig capture bundles amp+cab and displaces the cab slot.
            capture.clearPreviewCabinetPackage();
            capture.setPreviewComplementCabUseCustom(false);
            cabSlotSource = RigSlotSource::Default;
            cabSlotLabel.clear();
            capture.setPreviewCabinetEnabled(false);
        }
        else if (! capture.hasPreviewCabinetPackage())
        {
            capture.setPreviewCabinetEnabled(
                previewComplementCabDefaultForCaptureType(appState.captureWizard().captureType));
        }

        applyPreviewControlLabels();
        modelLabel.setText(utf8("Amp 슬롯: ") + model.summary(), juce::dontSendNotification);
    }
    else
    {
        model = {};
        modelReady = false;
        ampSlotSource = RigSlotSource::Default;
        loadedDeviceLabel.clear();
        applyPreviewControlLabels();
    }
}

void TonePreviewPanel::refreshSamples()
{
    juce::String error;
    PreviewSampleLibrary::ensureSamplesDirectory(error);
    samples = PreviewSampleLibrary::listSampleFiles();
    sampleList.updateContent();
    if (samples.size() > 0 && sampleList.getSelectedRow() < 0)
        sampleList.selectRow(0);

    folderLabel.setText("Preview Samples: " + PreviewSampleLibrary::samplesDirectory().getFullPathName(),
                        juce::dontSendNotification);

    if (samples.isEmpty())
        statusLabel.setText(utf8("Preview Samples 폴더에 WAV/AIFF/FLAC 파일을 추가하세요."), juce::dontSendNotification);
}

void TonePreviewPanel::importSamples()
{
    importChooser = std::make_unique<juce::FileChooser>("Import preview samples",
                                                        juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
                                                        "*.wav;*.aif;*.aiff;*.flac");

    importChooser->launchAsync(juce::FileBrowserComponent::openMode
                                   | juce::FileBrowserComponent::canSelectFiles
                                   | juce::FileBrowserComponent::canSelectMultipleItems,
        [this](const juce::FileChooser& chooser)
        {
            const auto selected = chooser.getResults();
            auto importedCount = 0;
            juce::String lastError;

            for (const auto& file : selected)
            {
                auto imported = PreviewSampleLibrary::importSampleFile(file, lastError);
                if (imported.existsAsFile())
                    ++importedCount;
            }

            refreshSamples();
            statusLabel.setText(importedCount > 0
                                    ? "Imported " + juce::String(importedCount) + " preview sample(s)."
                                    : lastError,
                                juce::dontSendNotification);
            importChooser = nullptr;
        });
}

void TonePreviewPanel::playSelectedSample()
{
    const auto row = sampleList.getSelectedRow();
    if (row < 0 || row >= samples.size())
    {
        statusLabel.setText(utf8("프리뷰할 샘플을 선택하세요."), juce::dontSendNotification);
        return;
    }

    if (realCaptureMode)
    {
        if (playRealCaptureRecording())
            return;

        // No recording for this sample/anchor: fall back to model rendering.
        realCaptureMode = false;
        realCaptureButton.setToggleState(false, juce::dontSendNotification);
        statusLabel.setText(utf8("이 샘플/게인 조합의 실캡쳐 녹음이 없어 모델 렌더로 재생합니다."),
                            juce::dontSendNotification);
    }

    juce::AudioBuffer<float> sample;
    juce::String error;
    if (! readSampleFile(samples[row], sample, error))
    {
        statusLabel.setText(error, juce::dontSendNotification);
        return;
    }

    if (modelReady && ! capture.hasPreviewModel())
        loadModelFromPackage();

    waveform.setAudioBuffer(sample, capture.currentSampleRate());
    updateGainModel();
    capture.loadPreviewSample(sample);
    if (capture.startPreviewSample())
        statusLabel.setText("Playing Sample: " + samples[row].getFileName(), juce::dontSendNotification);
}

juce::String TonePreviewPanel::realCaptureChunkIdForSelection() const
{
    const auto row = sampleList.getSelectedRow();
    if (row < 0 || row >= samples.size())
        return {};

    const auto sampleId = sanitizePreviewSampleId(samples[row].getFileNameWithoutExtension());

    // Nearest gain anchor to the slider position.
    const auto gainPercent = gainSlider.getValue();
    juce::String anchorDirectory = "gain-050";
    if (std::abs(gainPercent - 10.0) < std::abs(gainPercent - 50.0)
        && std::abs(gainPercent - 10.0) < std::abs(gainPercent - 100.0))
        anchorDirectory = "gain-010";
    else if (std::abs(gainPercent - 100.0) <= std::abs(gainPercent - 50.0))
        anchorDirectory = "gain-100";

    const auto chunkId = "capture/" + anchorDirectory + "/sample-" + sampleId + ".pcm16";
    return appState.currentPackage().findChunk(chunkId) != nullptr ? chunkId : juce::String();
}

bool TonePreviewPanel::playRealCaptureRecording()
{
    const auto chunkId = realCaptureChunkIdForSelection();
    if (chunkId.isEmpty())
        return false;

    const auto* chunk = appState.currentPackage().findChunk(chunkId);
    juce::AudioBuffer<float> recording;
    double recordingSampleRate = 0.0;
    juce::String error;
    if (chunk == nullptr
        || ! HansoAudioChunkCodec::decodePcm16Audio(chunk->data, recording, recordingSampleRate, error))
        return false;

    if (recordingSampleRate > 0.0 && std::abs(recordingSampleRate - capture.currentSampleRate()) > 0.5)
    {
        juce::AudioBuffer<float> resampled;
        resampleLinear(recording, recordingSampleRate, capture.currentSampleRate(), resampled);
        recording = std::move(resampled);
    }

    // The recording already went through the real amp — bypass the model so
    // it is not amped twice. The model is restored when Real mode is left.
    capture.clearPreviewModel();
    waveform.setAudioBuffer(recording, capture.currentSampleRate());
    capture.loadPreviewSample(recording);
    if (! capture.startPreviewSample())
        return false;

    statusLabel.setText("Playing REAL capture: " + chunkId, juce::dontSendNotification);
    return true;
}

void TonePreviewPanel::toggleRealCaptureMode()
{
    realCaptureMode = realCaptureButton.getToggleState();

    if (realCaptureMode)
    {
        if (realCaptureChunkIdForSelection().isEmpty())
        {
            realCaptureMode = false;
            realCaptureButton.setToggleState(false, juce::dontSendNotification);
            statusLabel.setText(utf8("이 샘플/게인 조합의 실캡쳐 녹음이 패키지에 없습니다."),
                                juce::dontSendNotification);
            return;
        }

        statusLabel.setText(utf8("Real Capture 모드: 실제 앰프를 거친 녹음을 재생합니다."),
                            juce::dontSendNotification);
    }
    else
    {
        capture.stopPreviewSample();
        if (modelReady)
            loadModelFromPackage();
        statusLabel.setText(utf8("Model 모드: compact model 렌더로 재생합니다."), juce::dontSendNotification);
    }
}

void TonePreviewPanel::stopPlayback()
{
    capture.stopPreviewSample();
    waveform.setProgress(0.0);
    statusLabel.setText("Stopped.", juce::dontSendNotification);
}

void TonePreviewPanel::updateGainModel()
{
    capture.setPreviewGainPercent(static_cast<float>(gainSlider.getValue()));
}

void TonePreviewPanel::selectMicPositionPreset(int positionIndex)
{
    if (positionIndex < 0 || positionIndex >= cabinetMicPositionCount())
        return;

    const auto percent = static_cast<double>(kCabinetMicPositions[positionIndex].normalizedPosition * 100.0f);
    capture.setPreviewMicPositionPercent(static_cast<float>(percent));
}

void TonePreviewPanel::updateButtonState()
{
    const auto hasSelection = sampleList.getSelectedRow() >= 0 && sampleList.getSelectedRow() < samples.size();
    playButton.setEnabled(hasSelection);
    stopButton.setEnabled(capture.isPreviewSamplePlaying());
    const auto cabPackageLoaded = capture.hasPreviewCabinetPackage();
    gainSlider.setEnabled(modelReady);
    cabinetButton.setEnabled(! cabPackageLoaded);
    micPositionBox.setEnabled(cabPackageLoaded);
    micDistanceSlider.setEnabled(cabPackageLoaded);
    previewMicBox.setEnabled(cabPackageLoaded && capture.previewCabinetHasMicMatrix());
    cabSourceBox.setEnabled(! cabPackageLoaded && capture.isPreviewCabinetEnabled());
    if (! cabSourceBox.isPopupActive() && complementCabChooser == nullptr)
        cabSourceBox.setSelectedId(capture.previewComplementCabUseCustom() ? 2 : 1,
                                   juce::dontSendNotification);
    realCaptureButton.setEnabled(modelReady && realCaptureChunkIdForSelection().isNotEmpty());
    loopButton.setToggleState(capture.isPreviewLoopEnabled(), juce::dontSendNotification);
    normalizationButton.setToggleState(capture.isPreviewNormalizationEnabled(), juce::dontSendNotification);
    cabinetButton.setToggleState(capture.isPreviewCabinetEnabled(), juce::dontSendNotification);
    refreshChainStrip();
}

bool TonePreviewPanel::readSampleFile(const juce::File& file,
                                      juce::AudioBuffer<float>& buffer,
                                      juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
    {
        error = "Could not read preview sample: " + file.getFileName();
        return false;
    }

    const auto sourceChannels = juce::jlimit(1, 2, static_cast<int>(reader->numChannels));
    juce::AudioBuffer<float> source(sourceChannels, static_cast<int>(reader->lengthInSamples));
    if (! reader->read(&source, 0, source.getNumSamples(), 0, true, true))
    {
        error = "Could not load preview sample audio.";
        return false;
    }

    resampleLinear(source, reader->sampleRate, capture.currentSampleRate(), buffer);
    return buffer.getNumSamples() > 0;
}

void TonePreviewPanel::resampleLinear(const juce::AudioBuffer<float>& source,
                                      double sourceSampleRate,
                                      double targetSampleRate,
                                      juce::AudioBuffer<float>& destination)
{
    if (source.getNumSamples() <= 0 || sourceSampleRate <= 0.0 || targetSampleRate <= 0.0)
    {
        destination.setSize(0, 0);
        return;
    }

    if (std::abs(sourceSampleRate - targetSampleRate) < 1.0)
    {
        destination.makeCopyOf(source, true);
        return;
    }

    const auto ratio = sourceSampleRate / targetSampleRate;
    const auto outSamples = juce::jmax(1, static_cast<int>(std::round(source.getNumSamples() / ratio)));
    destination.setSize(source.getNumChannels(), outSamples, false, true, true);

    for (int channel = 0; channel < source.getNumChannels(); ++channel)
    {
        juce::LagrangeInterpolator interpolator;
        interpolator.reset();
        interpolator.process(ratio,
                             source.getReadPointer(channel),
                             destination.getWritePointer(channel),
                             outSamples);
    }
}
}

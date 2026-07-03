#include "UI/TonePreviewPanel.h"

#include "Analysis/ModelExtractionEngine.h"
#include "App/Utf8.h"
#include "Preview/PreviewSampleLibrary.h"
#include "Serialization/HansoModelCodec.h"
#include "Serialization/HansoSerializer.h"

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
    addAndMakeVisible(volumeSlider);
    addAndMakeVisible(addButton);
    addAndMakeVisible(openHansoButton);
    addAndMakeVisible(openFolderButton);
    addAndMakeVisible(refreshButton);

    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(statusLabel);
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

    auto amp = ampLabel.getBounds().expanded(10).toFloat();
    g.setColour(juce::Colour::fromRGB(62, 49, 40));
    g.fillRoundedRectangle(amp, 8.0f);
    g.setColour(juce::Colour::fromRGB(135, 112, 88));
    g.drawRoundedRectangle(amp, 8.0f, 2.0f);

    auto grille = ampLabel.getBounds().reduced(26, 44).toFloat();
    g.setColour(juce::Colour::fromRGB(30, 32, 32));
    g.fillRoundedRectangle(grille, 4.0f);
    g.setColour(juce::Colour::fromRGB(80, 82, 78));
    for (int x = static_cast<int>(grille.getX()); x < static_cast<int>(grille.getRight()); x += 12)
        g.drawVerticalLine(x, grille.getY(), grille.getBottom());
}

void TonePreviewPanel::resized()
{
    auto area = getLocalBounds().reduced(18);
    titleLabel.setBounds(area.removeFromTop(32));
    modelLabel.setBounds(area.removeFromTop(24));
    folderLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(12);

    auto ampArea = area.removeFromTop(210);
    ampLabel.setBounds(ampArea.reduced(18, 18));

    auto gainRow = area.removeFromTop(36);
    gainRow.removeFromLeft(90);
    gainSlider.setBounds(gainRow.removeFromLeft(420).reduced(0, 4));

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
    g.setFont(15.0f);
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
    if (previewSourceMode == PreviewSourceMode::CabinetPackage)
    {
        gainSlider.setTextValueSuffix("% Mic");
        ampLabel.setText("HANSO CAB\nMic Position", juce::dontSendNotification);
    }
    else
    {
        gainSlider.setTextValueSuffix("% Gain");
        ampLabel.setText("HANSO AMP\nPreview Cabinet", juce::dontSendNotification);
    }
}

bool TonePreviewPanel::isCabinetPackage(const HansoPackage& package) const noexcept
{
    if (package.metadata.category == HansoCategory::Cabinet)
        return true;

    if (! package.cabinetProfile.isVoid())
        return true;

    return false;
}

bool TonePreviewPanel::loadCabinetPackageFromFile(const juce::File& file, HansoPackage& package)
{
    juce::String error;
    if (! HansoSerializer::readFromFile(file, package, error))
    {
        statusLabel.setText("Open HANSO failed: " + error, juce::dontSendNotification);
        modelLabel.setText("Open HANSO failed: " + error, juce::dontSendNotification);
        return false;
    }

    if (! isCabinetPackage(package))
        return false;

    if (! capture.loadPreviewCabinetPackage(package))
    {
        statusLabel.setText(utf8("캐비넷 .hanso를 프리뷰할 수 없습니다. captured/imported IR이 필요합니다."),
                            juce::dontSendNotification);
        modelLabel.setText(utf8("Cabinet preview unavailable"), juce::dontSendNotification);
        return false;
    }

    previewSourceMode = PreviewSourceMode::CabinetPackage;
    model = {};
    modelReady = true;
    observedPreviewCabinetRevision = capture.previewCabinetRevision();
    observedPreviewRevision = capture.previewModelRevision();
    applyPreviewControlLabels();
    capture.setPreviewMicPositionPercent(static_cast<float>(gainSlider.getValue()));

    const auto toneName = package.metadata.name.trim().isNotEmpty()
                        ? package.metadata.name.trim()
                        : file.getFileNameWithoutExtension();
    modelLabel.setText(utf8("Cabinet Preview: ") + toneName, juce::dontSendNotification);
    statusLabel.setText("Opened cabinet HANSO: " + file.getFileName(), juce::dontSendNotification);
    return true;
}

void TonePreviewPanel::loadInitialPreviewModel()
{
    if (capture.hasPreviewCabinetPackage())
    {
        previewSourceMode = PreviewSourceMode::CabinetPackage;
        modelReady = true;
        observedPreviewCabinetRevision = capture.previewCabinetRevision();
        applyPreviewControlLabels();
        modelLabel.setText(utf8("Cabinet preview loaded."), juce::dontSendNotification);
        return;
    }

    if (const auto* currentModel = capture.currentPreviewModel())
    {
        model = *currentModel;
        previewSourceMode = PreviewSourceMode::AmpModel;
        modelReady = true;
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
        previewSourceMode = PreviewSourceMode::Clean;
        observedPreviewRevision = capture.previewModelRevision();
        applyPreviewControlLabels();
        modelLabel.setText(utf8("Clean preview: 모델 없이 원본 샘플을 재생합니다."), juce::dontSendNotification);
        return;
    }

    juce::String error;
    modelReady = HansoModelCodec::decodeCompactModel(chunk->data, model, error);
    if (! modelReady)
    {
        previewSourceMode = PreviewSourceMode::Clean;
        modelLabel.setText("Model decode failed: " + error, juce::dontSendNotification);
        return;
    }

    capture.loadPreviewModel(model);
    capture.setPreviewGainPercent(static_cast<float>(gainSlider.getValue()));
    previewSourceMode = PreviewSourceMode::AmpModel;
    observedPreviewRevision = capture.previewModelRevision();
    applyPreviewControlLabels();
    modelLabel.setText(utf8("Unsaved current tone: ") + model.summary(), juce::dontSendNotification);
}

bool TonePreviewPanel::loadModelFromHansoFile(const juce::File& file)
{
    HansoPackage package;
    if (loadCabinetPackageFromFile(file, package))
        return true;

    juce::String error;
    if (! HansoSerializer::readFromFile(file, package, error))
    {
        statusLabel.setText("Open HANSO failed: " + error, juce::dontSendNotification);
        modelLabel.setText("Open HANSO failed: " + error, juce::dontSendNotification);
        return false;
    }

    CompactHansoModel loadedModel;
    juce::String statusText;
    if (! loadCompactModelChunk(package, file, loadedModel, statusText))
    {
        statusLabel.setText(statusText, juce::dontSendNotification);
        modelLabel.setText(statusText, juce::dontSendNotification);
        return false;
    }

    capture.clearPreviewCabinetPackage();
    model = std::move(loadedModel);
    modelReady = capture.loadPreviewModel(model);
    if (modelReady)
        capture.setPreviewGainPercent(static_cast<float>(gainSlider.getValue()));
    previewSourceMode = modelReady ? PreviewSourceMode::AmpModel : PreviewSourceMode::Clean;
    observedPreviewRevision = capture.previewModelRevision();
    observedPreviewCabinetRevision = capture.previewCabinetRevision();
    applyPreviewControlLabels();
    const auto toneName = package.metadata.name.trim().isNotEmpty()
                        ? package.metadata.name.trim()
                        : file.getFileNameWithoutExtension();
    modelLabel.setText("Preview Tone: " + toneName, juce::dontSendNotification);
    statusLabel.setText(statusText, juce::dontSendNotification);
    return modelReady;
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
    const auto cabinetRevision = capture.previewCabinetRevision();
    if (cabinetRevision != observedPreviewCabinetRevision)
    {
        observedPreviewCabinetRevision = cabinetRevision;
        if (capture.hasPreviewCabinetPackage())
        {
            previewSourceMode = PreviewSourceMode::CabinetPackage;
            modelReady = true;
            capture.setPreviewMicPositionPercent(static_cast<float>(gainSlider.getValue()));
            applyPreviewControlLabels();
            modelLabel.setText(utf8("Cabinet preview active."), juce::dontSendNotification);
            return;
        }

        if (previewSourceMode == PreviewSourceMode::CabinetPackage)
        {
            previewSourceMode = PreviewSourceMode::Clean;
            modelReady = false;
            applyPreviewControlLabels();
            modelLabel.setText(utf8("Clean preview: 모델 없이 원본 샘플을 재생합니다."), juce::dontSendNotification);
        }
    }

    const auto revision = capture.previewModelRevision();
    if (revision == observedPreviewRevision)
        return;

    observedPreviewRevision = revision;
    if (const auto* currentModel = capture.currentPreviewModel())
    {
        model = *currentModel;
        previewSourceMode = PreviewSourceMode::AmpModel;
        modelReady = true;
        capture.setPreviewGainPercent(static_cast<float>(gainSlider.getValue()));
        applyPreviewControlLabels();
        modelLabel.setText(utf8("Current preview tone: ") + model.summary(), juce::dontSendNotification);
    }
    else if (previewSourceMode != PreviewSourceMode::CabinetPackage)
    {
        model = {};
        previewSourceMode = PreviewSourceMode::Clean;
        modelReady = false;
        applyPreviewControlLabels();
        modelLabel.setText(utf8("Clean preview: 모델 없이 원본 샘플을 재생합니다."), juce::dontSendNotification);
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

    juce::AudioBuffer<float> sample;
    juce::String error;
    if (! readSampleFile(samples[row], sample, error))
    {
        statusLabel.setText(error, juce::dontSendNotification);
        return;
    }

    waveform.setAudioBuffer(sample, capture.currentSampleRate());
    updateGainModel();
    capture.loadPreviewSample(sample);
    if (capture.startPreviewSample())
        statusLabel.setText("Playing Sample: " + samples[row].getFileName(), juce::dontSendNotification);
}

void TonePreviewPanel::stopPlayback()
{
    capture.stopPreviewSample();
    waveform.setProgress(0.0);
    statusLabel.setText("Stopped.", juce::dontSendNotification);
}

void TonePreviewPanel::updateGainModel()
{
    if (! modelReady)
        return;

    if (previewSourceMode == PreviewSourceMode::CabinetPackage)
        capture.setPreviewMicPositionPercent(static_cast<float>(gainSlider.getValue()));
    else
        capture.setPreviewGainPercent(static_cast<float>(gainSlider.getValue()));
}

void TonePreviewPanel::updateButtonState()
{
    const auto hasSelection = sampleList.getSelectedRow() >= 0 && sampleList.getSelectedRow() < samples.size();
    playButton.setEnabled(hasSelection);
    stopButton.setEnabled(capture.isPreviewSamplePlaying());
    gainSlider.setEnabled(modelReady);
    cabinetButton.setEnabled(modelReady && previewSourceMode == PreviewSourceMode::AmpModel);
    loopButton.setToggleState(capture.isPreviewLoopEnabled(), juce::dontSendNotification);
    normalizationButton.setToggleState(capture.isPreviewNormalizationEnabled(), juce::dontSendNotification);
    cabinetButton.setToggleState(capture.isPreviewCabinetEnabled(), juce::dontSendNotification);
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

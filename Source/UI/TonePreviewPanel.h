#pragma once

#include <JuceHeader.h>

#include "App/ApplicationState.h"
#include "Capture/CaptureEngine.h"
#include "Capture/CaptureType.h"
#include "Model/CompactHansoModel.h"
#include "Model/HansoPackage.h"
#include "UI/PreviewChainStrip.h"
#include "UI/PreviewWaveformComponent.h"

namespace hanso
{
enum class PreviewSourceMode
{
    Clean,
    AmpModel,
    CabinetPackage
};

class TonePreviewPanel final : public juce::Component,
                               private juce::ListBoxModel,
                               private juce::Timer
{
public:
    TonePreviewPanel(ApplicationState& state, CaptureEngine& captureEngine);
    ~TonePreviewPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    int getNumRows() override;
    void paintListBoxItem(int rowNumber,
                          juce::Graphics& g,
                          int width,
                          int height,
                          bool rowIsSelected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void timerCallback() override;

    void loadInitialPreviewModel();
    void loadModelFromPackage();
    bool loadModelFromHansoFile(const juce::File& file);
    bool loadCabinetPackageFromFile(const juce::File& file, HansoPackage& package);
    bool isCabinetPackage(const HansoPackage& package) const noexcept;
    void applyPreviewControlLabels();
    void refreshChainStrip();
    bool loadCompactModelChunk(HansoPackage& package,
                               const juce::File& sourceFile,
                               CompactHansoModel& destination,
                               juce::String& statusText);
    void openHansoFile();
    void syncModelFromCaptureEngine();
    void refreshSamples();
    void importSamples();
    void playSelectedSample();
    void stopPlayback();
    void updateGainModel();
    void updateButtonState();
    void toggleRealCaptureMode();
    juce::String realCaptureChunkIdForSelection() const;
    bool playRealCaptureRecording();
    bool readSampleFile(const juce::File& file, juce::AudioBuffer<float>& buffer, juce::String& error);
    static void resampleLinear(const juce::AudioBuffer<float>& source,
                               double sourceSampleRate,
                               double targetSampleRate,
                               juce::AudioBuffer<float>& destination);

    ApplicationState& appState;
    CaptureEngine& capture;
    CompactHansoModel model;
    PreviewSourceMode previewSourceMode { PreviewSourceMode::Clean };
    bool modelReady { false };
    int observedPreviewRevision { -1 };
    int observedPreviewCabinetRevision { -1 };
    juce::AudioFormatManager formatManager;
    juce::Array<juce::File> samples;
    std::unique_ptr<juce::FileChooser> importChooser;
    std::unique_ptr<juce::FileChooser> hansoChooser;

    juce::Label titleLabel;
    juce::Label modelLabel;
    juce::Label folderLabel;
    juce::Label ampLabel;
    PreviewChainStrip chainStrip;
    // What kind of device the loaded model is (CaptureType string), shown as
    // the package block title in the chain strip.
    juce::String loadedDeviceLabel;
    juce::Slider gainSlider;
    // Cabinet package mode only: mic-swap EQ preview from cabProfile.micMatrix.
    juce::ComboBox previewMicBox;
    juce::ListBox sampleList { "Preview Samples", this };
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton loopButton { "Loop" };
    juce::TextButton normalizationButton { "Norm" };
    juce::TextButton cabinetButton { "Cab" };
    // A/B: plays the amp-processed recording of the selected sample (stored
    // during capture) instead of rendering the model — honest reference.
    juce::TextButton realCaptureButton { "Real" };
    bool realCaptureMode { false };
    juce::Slider volumeSlider;
    juce::TextButton addButton { "Add Samples" };
    juce::TextButton openHansoButton { "Open HANSO" };
    juce::TextButton openFolderButton { "Open Folder" };
    juce::TextButton refreshButton { "Refresh" };
    juce::Label statusLabel;
    PreviewWaveformComponent waveform;
};
}

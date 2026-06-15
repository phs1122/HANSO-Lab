#pragma once

#include <JuceHeader.h>

#include "App/ApplicationState.h"
#include "Capture/CaptureEngine.h"
#include "Model/CompactHansoModel.h"
#include "Model/HansoPackage.h"
#include "UI/PreviewWaveformComponent.h"

namespace hanso
{
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
    bool readSampleFile(const juce::File& file, juce::AudioBuffer<float>& buffer, juce::String& error);
    static void resampleLinear(const juce::AudioBuffer<float>& source,
                               double sourceSampleRate,
                               double targetSampleRate,
                               juce::AudioBuffer<float>& destination);

    ApplicationState& appState;
    CaptureEngine& capture;
    CompactHansoModel model;
    bool modelReady { false };
    int observedPreviewRevision { -1 };
    juce::AudioFormatManager formatManager;
    juce::Array<juce::File> samples;
    std::unique_ptr<juce::FileChooser> importChooser;
    std::unique_ptr<juce::FileChooser> hansoChooser;

    juce::Label titleLabel;
    juce::Label modelLabel;
    juce::Label folderLabel;
    juce::Label ampLabel;
    juce::Slider gainSlider;
    juce::ListBox sampleList { "Preview Samples", this };
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton loopButton { "Loop" };
    juce::TextButton normalizationButton { "Norm" };
    juce::TextButton cabinetButton { "Cab" };
    juce::Slider volumeSlider;
    juce::TextButton addButton { "Add Samples" };
    juce::TextButton openHansoButton { "Open HANSO" };
    juce::TextButton openFolderButton { "Open Folder" };
    juce::TextButton refreshButton { "Refresh" };
    juce::Label statusLabel;
    PreviewWaveformComponent waveform;
};
}

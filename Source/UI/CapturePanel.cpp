#include "UI/CapturePanel.h"

namespace hanso
{
CapturePanel::CapturePanel(ApplicationState& state, CaptureEngine& captureEngine)
    : appState(state),
      capture(captureEngine),
      captureInputControls([this](int startChannel, int channelCount)
      {
          applyCaptureSettings(startChannel, channelCount);
      })
{
    styleSectionTitle(title, "Capture");
    addAndMakeVisible(title);
    addAndMakeVisible(statusLabel);

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
    startButton.onClick = [this] { capture.start(); };
    stopButton.onClick = [this] { capture.stop(); };
    resetButton.onClick = [this] { capture.reset(); };

    addAndMakeVisible(generateButton);
    addAndMakeVisible(startButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(resetButton);

    signalType.addItem("Log sine sweep", 1);
    signalType.addItem("White noise", 2);
    signalType.addItem("Pink noise", 3);
    signalType.addItem("Impulse burst", 4);
    signalType.addItem("Multi-sine", 5);
    signalType.setSelectedId(1);
    signalType.onChange = [this] { appState.appendLog("Test signal changed."); };
    addAndMakeVisible(signalType);

    captureInputControls.setModeLabelText("Mode");
    addAndMakeVisible(captureInputControls);
    capture.setCaptureInputChannel(-1);
    capture.setCaptureChannelCount(2);
    captureInputControls.syncFromEngine(capture.captureInputChannel(), capture.captureChannelCount());

    levelSlider.setRange(0.0, 0.5, 0.001);
    levelSlider.setValue(0.2);
    levelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 24);
    addAndMakeVisible(levelSlider);

    durationSlider.setRange(1.0, 20.0, 0.5);
    durationSlider.setValue(5.0);
    durationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 24);
    addAndMakeVisible(durationSlider);

    addAndMakeVisible(inputMeterLabel);
    addAndMakeVisible(outputMeterLabel);
    addAndMakeVisible(storedCaptureLabel);
    addAndMakeVisible(latencyLabel);
}

void CapturePanel::startUpdating()
{
    startTimerHz(6);
}

void CapturePanel::resized()
{
    auto area = getLocalBounds().reduced(contentMargin);
    title.setBounds(area.removeFromTop(36));
    statusLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(14);

    auto buttons = area.removeFromTop(36);
    generateButton.setBounds(buttons.removeFromLeft(170).reduced(0, 1));
    buttons.removeFromLeft(8);
    startButton.setBounds(buttons.removeFromLeft(90).reduced(0, 1));
    buttons.removeFromLeft(8);
    stopButton.setBounds(buttons.removeFromLeft(90).reduced(0, 1));
    buttons.removeFromLeft(8);
    resetButton.setBounds(buttons.removeFromLeft(90).reduced(0, 1));

    area.removeFromTop(24);
    signalType.setBounds(area.removeFromTop(32).removeFromLeft(180));
    area.removeFromTop(12);
    captureInputControls.layoutRows(area.removeFromTop(32), true);
    area.removeFromTop(12);
    levelSlider.setBounds(area.removeFromTop(32).removeFromLeft(320));
    area.removeFromTop(12);
    durationSlider.setBounds(area.removeFromTop(32).removeFromLeft(320));
    area.removeFromTop(18);
    inputMeterLabel.setBounds(area.removeFromTop(26));
    outputMeterLabel.setBounds(area.removeFromTop(26));
    storedCaptureLabel.setBounds(area.removeFromTop(26));
    latencyLabel.setBounds(area.removeFromTop(26));
}

void CapturePanel::timerCallback()
{
    capture.refresh();
    captureInputControls.syncFromEngine(capture.captureInputChannel(), capture.captureChannelCount());
    statusLabel.setText("State: " + capture.stateText(), juce::dontSendNotification);
    inputMeterLabel.setText("Input level: " + juce::String(juce::Decibels::gainToDecibels(capture.inputLevel(), -120.0f), 1) + " dBFS",
                            juce::dontSendNotification);
    outputMeterLabel.setText("Output level: " + juce::String(juce::Decibels::gainToDecibels(capture.outputLevel(), -120.0f), 1) + " dBFS",
                             juce::dontSendNotification);
    storedCaptureLabel.setText(capture.captureStorageText(), juce::dontSendNotification);
    latencyLabel.setText("Estimated latency: " + juce::String(capture.estimatedLatencySamples())
                         + " samples / " + juce::String(capture.estimatedLatencyMs(), 2) + " ms",
                         juce::dontSendNotification);
}

void CapturePanel::applyCaptureSettings(int startChannel, int channelCount)
{
    capture.setCaptureInputChannel(startChannel);
    capture.setCaptureChannelCount(channelCount);
    appState.appendLog("Capture routing updated from Capture tab.");
}
}

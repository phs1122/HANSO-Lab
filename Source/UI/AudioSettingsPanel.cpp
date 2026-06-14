#include "UI/AudioSettingsPanel.h"

namespace hanso
{
AudioSettingsPanel::AudioSettingsPanel(ApplicationState& state,
                                       AudioEngine& audioEngine,
                                       CaptureEngine& captureEngine)
    : appState(state),
      audio(audioEngine),
      capture(captureEngine),
      captureInputControls([this](int startChannel, int channelCount)
      {
          applyCaptureSettings(startChannel, channelCount);
      })
{
    styleSectionTitle(title, "Audio Device");
    addAndMakeVisible(title);

    deviceDescription.setText(audio.deviceController().describeCurrentDevice(), juce::dontSendNotification);
    addAndMakeVisible(deviceDescription);

    settingsButton.onClick = [this]
    {
        audio.deviceController().showSettingsDialog(this);
        appState.appendLog("Audio settings opened.");
    };
    addAndMakeVisible(settingsButton);

    monitorToggle.setToggleState(capture.isMonitoringEnabled(), juce::dontSendNotification);
    monitorToggle.onClick = [this]
    {
        capture.setMonitoringEnabled(monitorToggle.getToggleState());
    };
    addAndMakeVisible(monitorToggle);

    addAndMakeVisible(captureInputControls);
    captureInputControls.syncFromEngine(capture.captureInputChannel(), capture.captureChannelCount());
}

void AudioSettingsPanel::startUpdating()
{
    startTimerHz(2);
}

void AudioSettingsPanel::resized()
{
    auto area = getLocalBounds().reduced(contentMargin);
    title.setBounds(area.removeFromTop(36));
    deviceDescription.setBounds(area.removeFromTop(32));
    area.removeFromTop(10);
    settingsButton.setBounds(area.removeFromTop(34).removeFromLeft(220));
    area.removeFromTop(14);
    monitorToggle.setBounds(area.removeFromTop(30).removeFromLeft(180));
    area.removeFromTop(14);
    captureInputControls.layoutRows(area.removeFromTop(74), false);
}

void AudioSettingsPanel::timerCallback()
{
    deviceDescription.setText(audio.deviceController().describeCurrentDevice(), juce::dontSendNotification);
    captureInputControls.syncFromEngine(capture.captureInputChannel(), capture.captureChannelCount());
}

void AudioSettingsPanel::applyCaptureSettings(int startChannel, int channelCount)
{
    capture.setCaptureInputChannel(startChannel);
    capture.setCaptureChannelCount(channelCount);
    appState.appendLog("Capture routing updated from Audio tab.");
}
}

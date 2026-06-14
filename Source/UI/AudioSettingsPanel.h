#pragma once

#include <JuceHeader.h>

#include "App/ApplicationState.h"
#include "Audio/AudioEngine.h"
#include "Capture/CaptureEngine.h"
#include "UI/CaptureInputControls.h"
#include "UI/LabPanel.h"

namespace hanso
{
class AudioSettingsPanel final : public LabPanel,
                                 private juce::Timer
{
public:
    AudioSettingsPanel(ApplicationState& state, AudioEngine& audioEngine, CaptureEngine& captureEngine);
    void startUpdating();
    void resized() override;

private:
    void timerCallback() override;
    void applyCaptureSettings(int startChannel, int channelCount);

    ApplicationState& appState;
    AudioEngine& audio;
    CaptureEngine& capture;
    juce::Label title;
    juce::Label deviceDescription;
    juce::TextButton settingsButton { "Audio Device Settings" };
    juce::ToggleButton monitorToggle { "Input monitor" };
    CaptureInputControls captureInputControls;
};
}

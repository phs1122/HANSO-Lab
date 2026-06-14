#pragma once

#include <JuceHeader.h>

#include "App/ApplicationState.h"
#include "Capture/CaptureEngine.h"
#include "UI/CaptureInputControls.h"
#include "UI/LabPanel.h"

namespace hanso
{
class CapturePanel final : public LabPanel,
                           private juce::Timer
{
public:
    CapturePanel(ApplicationState& state, CaptureEngine& captureEngine);
    void startUpdating();
    void resized() override;

private:
    void timerCallback() override;
    void applyCaptureSettings(int startChannel, int channelCount);

    ApplicationState& appState;
    CaptureEngine& capture;
    juce::Label title;
    juce::Label statusLabel;
    juce::TextButton generateButton { "Generate Test Signal" };
    juce::TextButton startButton { "Start" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton resetButton { "Reset" };
    juce::ComboBox signalType;
    CaptureInputControls captureInputControls;
    juce::Slider levelSlider;
    juce::Slider durationSlider;
    juce::Label inputMeterLabel;
    juce::Label outputMeterLabel;
    juce::Label storedCaptureLabel;
    juce::Label latencyLabel;
};
}

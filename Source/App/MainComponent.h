#pragma once

#include <JuceHeader.h>

#include "App/ApplicationState.h"
#include "App/LabWorkflow.h"
#include "Audio/AudioEngine.h"
#include "Capture/CaptureEngine.h"
#include "Analysis/AnalysisEngine.h"
#include "UI/AudioSettingsPanel.h"
#include "UI/CapturePanel.h"
#include "UI/LogPanel.h"

namespace hanso
{
class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void requestCloseWithWarning(std::function<void()> closeApplication);

private:
    void timerCallback() override;
    void showSettingsDialog();
    void showAudioSettingsDialog();
    void showLogDialog();
    void toggleLayoutDebugMode();

    ApplicationState state;
    AudioEngine audioEngine;
    CaptureEngine captureEngine;
    AnalysisEngine analysisEngine;
    LabWorkflow workflow;

    juce::LookAndFeel_V4 lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 700 };
    juce::Label appTitle;
    juce::TextButton settingsButton { "Settings" };
    bool layoutDebugEnabled { false };
    CapturePanel capturePanel;
};
}

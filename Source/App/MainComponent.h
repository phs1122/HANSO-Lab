#pragma once

#include <JuceHeader.h>

#include "App/ApplicationState.h"
#include "App/LabWorkflow.h"
#include "Audio/AudioEngine.h"
#include "Capture/CaptureEngine.h"
#include "Analysis/AnalysisEngine.h"
#include "UI/AnalysisPanel.h"
#include "UI/AssetPanel.h"
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

private:
    void timerCallback() override;

    ApplicationState state;
    AudioEngine audioEngine;
    CaptureEngine captureEngine;
    AnalysisEngine analysisEngine;
    LabWorkflow workflow;

    juce::LookAndFeel_V4 lookAndFeel;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    AudioSettingsPanel audioSettingsPanel;
    CapturePanel capturePanel;
    AnalysisPanel analysisPanel;
    AssetPanel assetPanel;
    LogPanel logPanel;
};
}

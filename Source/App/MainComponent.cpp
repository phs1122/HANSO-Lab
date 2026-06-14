#include "App/MainComponent.h"

namespace hanso
{
MainComponent::MainComponent()
    : audioEngine(state),
      captureEngine(state, audioEngine),
      analysisEngine(state),
      workflow(captureEngine, analysisEngine),
      audioSettingsPanel(state, audioEngine, captureEngine),
      capturePanel(state, captureEngine),
      analysisPanel(workflow),
      assetPanel(state),
      logPanel(state)
{
    setLookAndFeel(&lookAndFeel);
    tabs.setLookAndFeel(&lookAndFeel);

    lookAndFeel.setColour(juce::ResizableWindow::backgroundColourId, juce::Colour::fromRGB(24, 25, 27));
    lookAndFeel.setColour(juce::TabbedComponent::backgroundColourId, juce::Colour::fromRGB(24, 25, 27));
    lookAndFeel.setColour(juce::TabbedButtonBar::tabTextColourId, juce::Colours::white);

    addAndMakeVisible(tabs);
    tabs.addTab("Audio", juce::Colours::transparentBlack, &audioSettingsPanel, false);
    tabs.addTab("Capture", juce::Colours::transparentBlack, &capturePanel, false);
    tabs.addTab("Analysis", juce::Colours::transparentBlack, &analysisPanel, false);
    tabs.addTab("Assets", juce::Colours::transparentBlack, &assetPanel, false);
    tabs.addTab("Log", juce::Colours::transparentBlack, &logPanel, false);

    audioSettingsPanel.startUpdating();
    capturePanel.startUpdating();

    audioEngine.initialise();
    state.appendLog("HANSO Lab initialized.");
    setSize(1180, 760);
    startTimerHz(12);
}

MainComponent::~MainComponent()
{
    stopTimer();
    audioEngine.shutdown();
    tabs.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(24, 25, 27));
}

void MainComponent::resized()
{
    tabs.setBounds(getLocalBounds().reduced(10));
}

void MainComponent::timerCallback()
{
    logPanel.refresh();
}
}

#pragma once

#include <JuceHeader.h>

#include "App/ApplicationState.h"
#include "UI/LabPanel.h"

namespace hanso
{
class LogPanel final : public LabPanel
{
public:
    explicit LogPanel(ApplicationState& state);
    void resized() override;
    void paint(juce::Graphics& g) override;
    void refresh();

private:
    ApplicationState& appState;
    juce::TextEditor logView;
};
}
